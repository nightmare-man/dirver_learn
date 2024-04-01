#include "ntddk.h"
#include "ProcessProtect.h"
#include "common.h"


struct Globals global;

BOOLEAN check_in_protect_list(ULONG pid) {
	BOOLEAN ret = FALSE;
	ExAcquireFastMutex(&global.lock);
	for (int i = 0; i < global.pid_count; i++) {
		if (global.pids[i] == pid) {
			ret = TRUE;
			break;
		}
	}
	ExReleaseFastMutex(&global.lock);
	return ret;
}
VOID add_protect_list(ULONG pid) {
	ExAcquireFastMutex(&global.lock);
	BOOLEAN existed = FALSE;
	for (int i = 0; i < global.pid_count; i++) {
		if (global.pids[i] == pid) {
			existed = TRUE;
			break;
		}
	}
	if (!existed) {
		global.pids[global.pid_count] = pid;
		global.pid_count++;
	}
	ExReleaseFastMutex(&global.lock);
}
VOID remove_protect_list(ULONG pid) {
	ExAcquireFastMutex(&global.lock);
	int idx = -1;
	for (int i = 0; i < global.pid_count; i++) {
		if (global.pids[i] == pid) {
			idx = i;
			break;
		}
	}
	if (idx != -1) {
		for (int i = idx; i < global.pid_count - 1; i++) {
			global.pids[i] = global.pids[i + 1];
		}
		global.pid_count--;
	}
	ExReleaseFastMutex(&global.lock);
}
VOID clear_protect_list() {
	ExAcquireFastMutex(&global.lock);
	global.pid_count = 0;
	ExReleaseFastMutex(&global.lock);
}


OB_PREOP_CALLBACK_STATUS on_pre_open(
	_In_ PVOID RegistrationContext,
	_Inout_ POB_PRE_OPERATION_INFORMATION OperationInformation
) {
	UNREFERENCED_PARAMETER(RegistrationContext);
	//�ں˷��� ֱ�ӷ���
	
	PVOID process = OperationInformation->Object;
	ULONG pid =  HandleToULong(PsGetProcessId(process));
	if (check_in_protect_list(pid)) {
		OperationInformation->Parameters->CreateHandleInformation.DesiredAccess &= ~PROCESS_TERMINATE;
		//ȥ����ر�Ȩ��
	}
	return OB_PREOP_SUCCESS;
	
};

NTSTATUS complete_irp(PIRP irp, NTSTATUS status, ULONG information) {
	irp->IoStatus.Status = status;
	irp->IoStatus.Information = information;
	IoCompleteRequest(irp, IO_NO_INCREMENT);
	return status;
}

NTSTATUS driver_create_close(_In_ PDEVICE_OBJECT dev, _Inout_ PIRP irp) {
	UNREFERENCED_PARAMETER(dev);
	return complete_irp(irp, STATUS_SUCCESS, 0);
}

NTSTATUS driver_control(_In_ PDEVICE_OBJECT dev, _Inout_ PIRP irp) {
	
	UNREFERENCED_PARAMETER(dev);
	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(irp);
	NTSTATUS ret = STATUS_SUCCESS;
	ULONG len = stack->Parameters.DeviceIoControl.InputBufferLength;
	KdPrint(("[ProcessProtect] recevice control(0x%08X)", stack->Parameters.DeviceIoControl.IoControlCode));
	switch (stack->Parameters.DeviceIoControl.IoControlCode) {
	case IOCTL_PROCESS_PROTECT_BY_PID: {
		if (len < sizeof(ULONG)) {
			ret = STATUS_INVALID_BUFFER_SIZE;
			break;
		}
		//�������������û��ڴ�����쳣��
		//�����������buffered��ʽ�������ô��˵�ַ
		//Ӧ��ʹ��irp->AssociatedIrp.SystemBuffer
		//�ṩ����io��ֱ��io�������ǣ���ֹ�ں˷����û��ڴ�Υ������������ͱ��ͷ���
		ULONG pid = *((ULONG*)irp->AssociatedIrp.SystemBuffer);
		//ULONG pid = *((ULONG*)(stack->Parameters.DeviceIoControl.Type3InputBuffer));
		add_protect_list(pid);
		KdPrint(("[ProcessProtect] pid(0x%08X)", pid));
		break;
	}
		
	case IOCTL_PROCESS_UNPROTECT_BY_PID: {
		if (len < sizeof(ULONG)) {
			ret = STATUS_INVALID_BUFFER_SIZE;
			break;
		}
		ULONG pid = *((ULONG*)irp->AssociatedIrp.SystemBuffer);
		remove_protect_list(pid);
		KdPrint(("[ProcessProtect] pid(0x%08X)", pid));
		break;
	}
		
	case IOCTL_PROCESS_PROTECT_CLEAR:
		clear_protect_list();
		break;
	default:
		ret = STATUS_DEVICE_INSUFFICIENT_RESOURCES;
		break;
	}
	return complete_irp(irp, ret, 0);
}

NTSTATUS driver_unload(_In_ PDRIVER_OBJECT driver_object) {
	UNICODE_STRING link_name = RTL_CONSTANT_STRING(L"\\??\\ProcessProtect");
	IoDeleteSymbolicLink(&link_name);
	IoDeleteDevice(driver_object->DeviceObject);
	ObUnRegisterCallbacks(global.reg_handle);
	return STATUS_SUCCESS;
}

//��DriverEntryû�з���֮ǰ�� ���õ����̶����ܱ�����
NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT driver_object, _In_ PUNICODE_STRING register_path) {
	UNREFERENCED_PARAMETER(register_path);
	ExInitializeFastMutex(&global.lock);
	clear_protect_list();
	PDEVICE_OBJECT dev = NULL;
	UNICODE_STRING dev_name = RTL_CONSTANT_STRING(L"\\Device\\ProcessProtect");
	UNICODE_STRING link_name = RTL_CONSTANT_STRING(L"\\??\\ProcessProtect");
	NTSTATUS ret = STATUS_SUCCESS;
	BOOLEAN dev_created = FALSE;
	BOOLEAN link_created = FALSE;
	do {
		ret = IoCreateDevice(driver_object, 0, &dev_name, FILE_DEVICE_UNKNOWN, 0, FALSE, &dev);
		if (!NT_SUCCESS(ret)) {
			KdPrint(("[ProcessProtect] create device fail(0x%08X)\n", ret));
			break;
		}
		dev_created = TRUE;
		ret = IoCreateSymbolicLink(&link_name, &dev_name);
		if (!NT_SUCCESS(ret)) {
			KdPrint(("[ProcessProtect] create device symbol link fail(0x%08X)\n", ret));
			break;
		}
		link_created = TRUE;
		//�費��Ҫ�����û�IO����ģʽ�أ�����Ҫ ��IRP_MJ_DEVICE_CONTROL�����û�������

		//c����Ҳ֧�����ֽṹ���ʼ��
		OB_OPERATION_REGISTRATION operations[] = {
			{
				.ObjectType = PsProcessType,
				.Operations = OB_OPERATION_HANDLE_CREATE | OB_OPERATION_HANDLE_DUPLICATE,
				.PreOperation = on_pre_open,
				.PostOperation = NULL,
			}
		};
		OB_CALLBACK_REGISTRATION reg = {
			.Altitude = RTL_CONSTANT_STRING(L"12345.678"),
			.Version = OB_FLT_REGISTRATION_VERSION,
			.OperationRegistrationCount = 1,
			.OperationRegistration = operations,
			.RegistrationContext = NULL
		};
		ret=ObRegisterCallbacks(&reg, &global.reg_handle);
		if (!NT_SUCCESS(ret)) {
			KdPrint(("[ProcessProtect] create object callback fail(0x%08X)\n", ret));
			break;
		}

	} while (FALSE);
	if (!NT_SUCCESS(ret)) {
		if (link_created) {
			IoDeleteSymbolicLink(&link_name);
		}
		if (dev_created) {
			IoDeleteDevice(dev);
		}
	}
	driver_object->DriverUnload = driver_unload;
	driver_object->MajorFunction[IRP_MJ_CREATE] = driver_object->MajorFunction[IRP_MJ_CLOSE]= driver_create_close;
	driver_object->MajorFunction[IRP_MJ_DEVICE_CONTROL] = driver_control;
	KdPrint(("[ProcessProtect] driver entry result(0x%08X)", ret));
	return ret;
}