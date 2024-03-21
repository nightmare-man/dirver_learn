#include <ntddk.h>
#include "SysMon.h"
#define DRIVER_TAG 12345678U
struct Globals g_global;

//����һ����ͨ�ں�ģʽAPC����PASSIVE_LEVEL ��irql 0�������У���˿���ʹ�÷�ҳ�ڴ棬
// ������֪ͨ�ص����ɴ����ĵ�һ���̵߳��� 
// �˳���֪ͨ�ص������˳������һ���̵߳��á�
//�����handle������� handle������һ��DWORD pid

void PushItem(PLIST_ENTRY entry_to_insert) {
	ExAcquireFastMutex(&g_global.mutex);
	//�����ϲ�Ӧ��ʹ��1024��Ӧ���ö�ȡע�����������
	if (g_global.item_count >= 1024) {
		PLIST_ENTRY entry1 = RemoveHeadList(&g_global.item_header);
		g_global.item_count--;
		//��entry���������ṹ���׵�ַ
		struct FullProcessExitInfo* item = CONTAINING_RECORD(entry1, struct FullProcessExitInfo, entry);
		ExFreePool(item);//�ͷ��ڴ� ��ֹй¶
	}
	InsertTailList(&g_global.item_header, entry_to_insert);
	g_global.item_count++;
	ExReleaseFastMutex(&g_global.mutex);
}

VOID on_process_create(_Inout_ PEPROCESS process, _In_ HANDLE process_id, _Inout_opt_ PPS_CREATE_NOTIFY_INFO create_info) {
	UNREFERENCED_PARAMETER(process);
	if (create_info) {
		//create process
		unsigned short alloc_size = sizeof(struct FullProcessCreateInfo);
		unsigned short cmd_line_size = 0;
		if (create_info->CommandLine) {
			cmd_line_size = create_info->CommandLine->Length;
			//alloc_size += cmd_line_size * sizeof(WCHAR);//����ط�ԭ��д��û��*wchar�Ĵ�С���Ҿ���������
			//�󲹳䣺ԭ���ǶԵģ���ΪUNICODE_STRING��Length���ֽڵĴ�С
			alloc_size += cmd_line_size;
		}
		struct FullProcessCreateInfo* item = (struct FullProcessCreateInfo*)ExAllocatePoolWithTag(PagedPool, alloc_size, DRIVER_TAG);
		if (!item) {
			KdPrint(("[SysMon] fail to allocate mem"));
			return;
		}
		//���size����full��size������������ṹ�����ݵ�ʵ�ʴ�С
		item->info.header.size = sizeof(struct ProcessCreateInfo) + cmd_line_size;
		item->info.header.type = ProcessCreate;
		KeQuerySystemTimePrecise(&item->info.header.time);
		item->info.process_id = HandleToULong(process_id);
		item->info.cmd_line_offset = sizeof(struct ProcessCreateInfo);
		item->info.cmd_line_len = cmd_line_size;
		memcpy((unsigned char*)(&(item->info)) + item->info.cmd_line_offset, create_info->CommandLine->Buffer, cmd_line_size );
		PushItem(&item->entry);
	}
	else {
		//exit process
		struct FullProcessExitInfo* info = (struct FullProcessExitInfo*)ExAllocatePoolWithTag(PagedPool, sizeof(struct FullProcessExitInfo), DRIVER_TAG);
		if (!info) {
			KdPrint(("[SysMon] fail to allocate mem"));
			return;
		}
		info->info.header.size = sizeof(struct ProcessExitInfo);
		info->info.header.type = ProcessExit;
		KeQuerySystemTimePrecise(&(info->info.header.time));
		info->info.process_id = HandleToULong(process_id);
		PushItem(&(info->entry));
	}

}

NTSTATUS complete_irp(PIRP irp, NTSTATUS status, ULONG information) {
	irp->IoStatus.Status = status;
	irp->IoStatus.Information = information;
	IoCompleteRequest(irp, 0);
	return status;
}


NTSTATUS driver_unload(_In_ PDRIVER_OBJECT driver_object) {
	PsSetCreateProcessNotifyRoutineEx(on_process_create, TRUE);
	UNICODE_STRING dev_link_name = RTL_CONSTANT_STRING(L"\\??\\SysMon");
	IoDeleteSymbolicLink(&dev_link_name);
	IoDeleteDevice(driver_object->DeviceObject);
	while (!IsListEmpty(&g_global.item_header)) {
		PLIST_ENTRY entry1 = RemoveHeadList(&g_global.item_header);
		struct FullProcessExitInfo* item = CONTAINING_RECORD(entry1, struct FullProcessExitInfo, entry);
		ExFreePool(item);
	}
	return STATUS_SUCCESS;
}

NTSTATUS driver_create_close(_In_ PDEVICE_OBJECT dev, _In_ PIRP irp) {
	UNREFERENCED_PARAMETER(dev);
	return complete_irp(irp, STATUS_SUCCESS, 0);
}

NTSTATUS driver_read(_In_ PDEVICE_OBJECT dev, _In_ PIRP irp) {
	UNREFERENCED_PARAMETER(dev);
	PIO_STACK_LOCATION stack= IoGetCurrentIrpStackLocation(irp);
	ULONG len = stack->Parameters.Read.Length;
	ULONG count = 0;//��������д����ֽ���
	NT_ASSERT(irp->MdlAddress);
	UCHAR* buffer = (UCHAR*)MmGetSystemAddressForMdlSafe(irp->MdlAddress, NormalPagePriority);
	if (!buffer) {
		return complete_irp(irp, STATUS_INVALID_BUFFER_SIZE, 0);
	}
	ExAcquireFastMutex(&g_global.mutex);
	while (TRUE) {
		if (IsListEmpty(&g_global.item_header)) {
			break;
		}
		//�Ӷ�β�� �Ӷ�ͷ��
		LIST_ENTRY* entry1 = RemoveHeadList(&g_global.item_header);
		//����˵�������ǲ�֪����һ��item���������ֽṹ��FullProcessCreate/Exit��������������㰴��һ���ṹ��,
		//��Ϊ����ֻ��Ҫ������ṹ�Ĵ�С�����ˣ�������ͬ��λ��
		struct FullProcessExitInfo* item = CONTAINING_RECORD(entry1, struct FullProcessExitInfo, entry);
		USHORT size = item->info.header.size;
		if (len < size) {
			//�û�̬��������bufferС�ˣ������Ƶģ����ȥ��
			InsertHeadList(&g_global.item_header, entry1);
			break;
		}
		g_global.item_count--;
		memcpy(buffer, &item->info, size);
		buffer += size;
		len -= size;
		count += size;
		ExFreePool(item);
	}
	ExReleaseFastMutex(&g_global.mutex);
	return complete_irp(irp, STATUS_SUCCESS, count);
}

NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT driver_object, _In_ PUNICODE_STRING register_path) {
	UNREFERENCED_PARAMETER(register_path);
	InitializeListHead(&(g_global.item_header));
	//��ʵ����û��Ҫ��������ʽ��ʾ����Ƿ�ҳ�ڴ棬windows������ȫ�ֱ���Ĭ�Ͼ��ǷǷ�ҳ�ڴ��ϵ�
	//g_global.mutex = ExAllocatePoolWithTag(NonPagedPool, sizeof(FAST_MUTEX), 123456);
	//if (!g_global.mutex) {
	//	return STATUS_INVALID_ADDRESS;
	//}
	ExInitializeFastMutex(&g_global.mutex);
	PDEVICE_OBJECT dev = NULL;
	UNICODE_STRING dev_name = RTL_CONSTANT_STRING(L"\\Device\\SysMon");
	UNICODE_STRING dev_link_name = RTL_CONSTANT_STRING(L"\\??\\SysMon");
	
	NTSTATUS ret = STATUS_SUCCESS;
	BOOLEAN dev_created = FALSE;
	BOOLEAN link_created = FALSE;
	do {
		ret = IoCreateDevice(driver_object, 0, &dev_name, FILE_DEVICE_UNKNOWN, 0, FALSE, &dev);
		if (!NT_SUCCESS(ret)) {
			KdPrint(("[SysMon] fail to create deivce(0x%08X)\n", ret));
			break;
		}
		dev_created = TRUE;

		dev->Flags |= DO_DIRECT_IO;
		ret = IoCreateSymbolicLink(&dev_link_name, &dev_name);
		if (!NT_SUCCESS(ret)) {
			KdPrint(("[SysMon] fail to create deivce symbol link(0x%08X)\n", ret));
			break;
		}
		link_created = TRUE;

		ret = PsSetCreateProcessNotifyRoutineEx(on_process_create, FALSE);
		if (!NT_SUCCESS(ret)) {
			KdPrint(("[SysMon] fail to set process create notify(0x%08X)\n", ret));
			break;
		}

	} while (FALSE);
	if (!NT_SUCCESS(ret)) {
		if (dev_created) {
			IoDeleteDevice(dev);
		}
		if (link_created) {
			IoDeleteSymbolicLink(&dev_link_name);
		}
	}
	driver_object->DriverUnload = driver_unload;
	driver_object->MajorFunction[IRP_MJ_CREATE] = driver_object->MajorFunction[IRP_MJ_CLOSE] = driver_create_close;
	driver_object->MajorFunction[IRP_MJ_READ] = driver_read;
	return ret;
}