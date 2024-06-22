#include <ntddk.h>
#include "vmm.h"
#include "tool.h"
/// <summary>
/// �������Է���
/// </summary>
/// <param name="driver"></param>
/// <param name="register_path"></param>
/// <returns></returns>
NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING register_path) {
	UNREFERENCED_PARAMETER(driver);
	UNREFERENCED_PARAMETER(register_path);
	ExInitializeDriverRuntime(0);
	Log("init vmm start");
	if (init_vmm()) {
		Log("init vmm success");
	}
	else {
		Log("init vmm fail");
	}
	if (vmm_call(CALL_TEST, 0, 0, 0) != CALL_RET_SUCCESS) {
		Log("call fail");
	}
	
	return STATUS_SUCCESS;
}