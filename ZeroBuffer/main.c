#include "ntddk.h"

NTSTATUS unload(_In_ PDRIVER_OBJECT driver_object) {


	UNICODE_STRING symbol_link = RTL_CONSTANT_STRING(L"\\??\\ZeroBuffer");
	IoDeleteSymbolicLink(&symbol_link);
	IoDeleteDevice(driver_object->DeviceObject);
	return STATUS_SUCCESS;
}

NTSTATUS complete_irp(PIRP irp, NTSTATUS status, ULONG info) {
	irp->IoStatus.Status = status;
	irp->IoStatus.Information = info;
	IoCompleteRequest(irp, 0);
	return status;
}

NTSTATUS create_or_close(_In_ PDEVICE_OBJECT device, _In_ PIRP irp) {
	UNREFERENCED_PARAMETER(device);
	return complete_irp(irp, STATUS_SUCCESS, 0);
}

NTSTATUS driver_write(_In_ PDEVICE_OBJECT device, _In_ PIRP irp) {
	UNREFERENCED_PARAMETER(device);
	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(irp);
	ULONG len = stack->Parameters.Write.Length;

	return complete_irp(irp, STATUS_SUCCESS, len);
}

NTSTATUS driver_read(_In_ PDEVICE_OBJECT device, _In_ PIRP irp) {
	UNREFERENCED_PARAMETER(device);
	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(irp);
	ULONG len = stack->Parameters.Read.Length;
	
	if (len == 0) {
		return complete_irp(irp, STATUS_INVALID_BUFFER_SIZE, 0);
	}
	PVOID buffer= MmGetSystemAddressForMdlSafe(irp->MdlAddress, NormalPagePriority);
	if (!buffer) {
		return complete_irp(irp, STATUS_INSUFFICIENT_RESOURCES,0);
	}
	memset(buffer, 0, len);
	return complete_irp(irp, STATUS_SUCCESS, 0);
}

NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT driver_object, _In_ PUNICODE_STRING register_path) {
	UNREFERENCED_PARAMETER(register_path);
	driver_object->DriverUnload = unload;
	driver_object->MajorFunction[IRP_MJ_CREATE] = create_or_close;
	driver_object->MajorFunction[IRP_MJ_CLOSE] = create_or_close;
	driver_object->MajorFunction[IRP_MJ_WRITE] = driver_write;
	driver_object->MajorFunction[IRP_MJ_READ] = driver_read;

	UNICODE_STRING dev_name = RTL_CONSTANT_STRING(L"\\Device\\ZeroBuffer");
	UNICODE_STRING symbol_link = RTL_CONSTANT_STRING(L"\\??\\ZeroBuffer");
	PDEVICE_OBJECT device = NULL;
	NTSTATUS ret = STATUS_SUCCESS;
	do {//����Ϊ�˱���ʹ��goto����̫��if else
		ret = IoCreateDevice(driver_object, 0, &dev_name, FILE_DEVICE_UNKNOWN, 0, FALSE, &device);
		if (!NT_SUCCESS(ret)) {
			KdPrint(("[ZeroBuffer]: can't create device (0x%08X)\n", ret));
			break;
		}
		device->Flags |= DO_DIRECT_IO;//ʹ��ֱ��io�������ƣ������û��ṩ���ڴ��Ӧ�������ڴ��ӳ�䣬ӳ�䵽ϵͳ�ռ䣩
		//ʹ��ֱ��io���߻���io���Ǳ�֤���û����ڴ�ķ��ʺϷ�����֤�䲻����ǰ�ͷţ������ڴ�Ƿ���
		ret = IoCreateSymbolicLink(&symbol_link, &dev_name);
		if (!NT_SUCCESS(ret)) {
			KdPrint(("[ZeroBuffer]: can't create device symbol link (0x%08X)\n", ret));
			break;
		}


	} while (FALSE);
	if (!NT_SUCCESS(ret)) {
		if (device) {
			IoDeleteDevice(device);
		}
	}

	return ret;
}