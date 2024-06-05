#pragma once
#include <ntddk.h>
#include <intrin.h>
#include "VMM.h"
#include "tool.h"
#include "msr.h"
static BOOLEAN need_off = FALSE;
BOOLEAN allocate_vmxon(struct VMM_STATE* vmm_state) {
	//����IRQL
	if (KeGetCurrentIrql() > DISPATCH_LEVEL) {
		KeRaiseIrqlToDpcLevel();
	}
	int vmxon_region_size = 2 * VMXON_SIZE;
	PHYSICAL_ADDRESS allowed_max = { .QuadPart = MAXULONG64 };
	PVOID buffer = MmAllocateContiguousMemory((SIZE_T)vmxon_region_size, allowed_max);
	if (buffer == NULL) {
		KdPrint(("[prefix]can't allocate vmxon region\n"));
		return FALSE;
	}
	RtlSecureZeroMemory(buffer, vmxon_region_size);
	ULONG64 aligned_buffer = ((ULONG64)buffer + ALIGNED_SIZE) & (~(ALIGNED_SIZE - 1));
	ULONG64 aligned_pa = vitual_to_physical(aligned_buffer);
	KdPrint(("[prefix] vmxon va:%lu\n", buffer));
	KdPrint(("[prefix] vmxon aligned va:%lu\n", aligned_buffer));
	KdPrint(("[prefix] vmxon aligned pa:%lu\n", aligned_pa));
	union VMX_BASE_MSR base_msr;
	base_msr.all = __readmsr(IA32_VMX_BASIC_MSR);
	KdPrint(("[prefix] base msr revision: %u\n", base_msr.fields.revision));
	KdPrint(("[prefix] base msr size require is: %u\n", base_msr.fields.RegionSize));
	*((ULONG32*)aligned_buffer) = base_msr.fields.revision;
	unsigned char ret=__vmx_on(&aligned_pa);
	if (ret) {
		KdPrint(("[prefix]vmx on execute error\n"));
		return FALSE;
	}
	vmm_state->vmxon_region_pa = aligned_pa;
	vmm_state->vmxon_region_va_unaligned = buffer;
	KdPrint(("[prefix]vmx on success\n"));
	return TRUE;
}
BOOLEAN allocate_vmcs(struct VMM_STATE* vmm_state) {
	if (KeGetCurrentIrql() > DISPATCH_LEVEL) {
		KeRaiseIrqlToDpcLevel();
	}
	int vmxon_region_size = 2 * VMXON_SIZE;
	PHYSICAL_ADDRESS allowed_max = { .QuadPart = MAXULONG64 };
	PVOID buffer = MmAllocateContiguousMemory((SIZE_T)vmxon_region_size, allowed_max);
	if (buffer == NULL) {
		KdPrint(("[prefix]can't allocate vmcs region\n"));
		return FALSE;
	}
	RtlSecureZeroMemory(buffer, vmxon_region_size);
	ULONG64 aligned_buffer = ((ULONG64)buffer + ALIGNED_SIZE) & (~(ALIGNED_SIZE - 1));
	ULONG64 aligned_pa = vitual_to_physical(aligned_buffer);
	KdPrint(("[prefix] vmcs va:%lu\n", buffer));
	KdPrint(("[prefix] vmcs aligned va:%lu\n", aligned_buffer));
	KdPrint(("[prefix] vmcs aligned pa:%lu\n", aligned_pa));
	union VMX_BASE_MSR base_msr;
	base_msr.all = __readmsr(IA32_VMX_BASIC_MSR);
	KdPrint(("[prefix] base msr revision: %u\n", base_msr.fields.revision));
	*((ULONG32*)aligned_buffer) = base_msr.fields.revision;
	vmm_state->vmcs_pa = aligned_pa;
	vmm_state->vmcs_va_unaligned = buffer;
	KdPrint(("[prefix]vmptrld success\n"));
	return TRUE;
}
BOOLEAN enable_vmx() {
	ULONG64 cr4_origin = __readcr4();
	__writecr4(cr4_origin | (0b1 << 13));
	union FEATURE_CONTROL_MSR feature_control = { .all = 0 };
	feature_control.all=__readmsr(IA32_FEATURE_CONTROL_MSR);
	feature_control.fields.lock = 1;
	feature_control.fields.enable_vmx_in_smx = 1;
	feature_control.fields.enable_vmx_out_smx = 1;
	__writemsr(IA32_FEATURE_CONTROL_MSR, feature_control.all);
	return TRUE;
}

BOOLEAN init_vmx() {
	ULONG32 cpu_count = KeQueryActiveProcessorCount(NULL);
	if (!is_vmx_support()) {
		KdPrint(("[prefix] cpu not support vmx\n"));
		return FALSE;
	}
	g_vmm_state_ptr = (struct VMM_STATE*)ExAllocatePoolWithTag(NonPagedPool, sizeof(struct VMM_STATE) * cpu_count, DRIVER_TAG);
	if ((g_vmm_state_ptr) == NULL) {
		KdPrint(("[prefix] allocate g_vmm_state fail\n"));
		return FALSE;
	}
	KAFFINITY ka;
	for (ULONG32 i = 0; i < cpu_count; i++) {
		ka = (KAFFINITY)2 ^ i;//ÿ��������������
		KeSetSystemAffinityThread(ka);
		enable_vmx();
		KdPrint(("[prefix] enable vmx success on process %u\n", i));
		if (!allocate_vmxon(&(g_vmm_state_ptr[i]))) {
			continue;
		}
		if (!allocate_vmcs(&(g_vmm_state_ptr[i]))) {
			continue;
		}
	}
	need_off = TRUE;
	return TRUE;
}

BOOLEAN leave_vmx() {
	if (!need_off) {
		return TRUE;
	}
	ULONG32 cpu_count = KeQueryActiveProcessorCount(NULL);
	KAFFINITY ka;
	for (ULONG32 i = 0; i < cpu_count; i++) {
		ka = 2 ^ i;
		KeSetSystemAffinityThread(ka);
		__vmx_off();
		
		
		MmFreeContiguousMemory((PVOID)g_vmm_state_ptr[i].vmxon_region_va_unaligned);
		MmFreeContiguousMemory((PVOID)g_vmm_state_ptr[i].vmcs_va_unaligned);
	}
	ExFreePoolWithTag(g_vmm_state_ptr, DRIVER_TAG);
	return TRUE;
}

ULONG64 vm_ptr_store() {
	ULONG64 ret;
	__vmx_vmptrst(&ret);
	return ret;
}
BOOLEAN clear_vm_state(struct VMM_STATE* vmm_state_ptr) {
	unsigned char ret = __vmx_vmclear((ULONG64*)vmm_state_ptr);
	if (ret != 0) {
		return FALSE;
	}
	return TRUE;
}

BOOLEAN vm_ptr_load(struct VMM_STATE* vmm_state_ptr) {
	unsigned char ret = __vmx_vmptrld((ULONG64*)vmm_state_ptr);
	if (ret != 0) {
		return FALSE;
	}
	return TRUE;
}
BOOLEAN setup_vmcs(struct VMM_STATE* vmm_state_ptr, union EXTENDED_PAGE_TABLE_POINTER* eptp) {
	__vmx_vmwrite(GUEST_CR0, __readcr0());
	__vmx_vmwrite(GUEST_CR3, __readcr3());
	__vmx_vmwrite(GUEST_CR4, __readcr4());
	__vmx_vmwrite(GUEST_DR7, 0x400ULL);//Ӳ�����ԼĴ���
	__vmx_vmwrite(GUEST_RSP, (vmm_state_ptr->vmm_stack + VMM_STACK_SIZE - 1));
	__vmx_vmwrite(GUEST_RIP, (ULONG64)1);//������Ҫ���������ַ����Ϊcr3�п����˷�ҳ
}
BOOLEAN launch_vmx(struct VMM_STATE* vmm_state_ptr, int process_id, union EXTENDED_PAGE_TABLE_POINTER* eptp) {
	KdPrint(("----launch vmx----\n"));
	KAFFINITY ka = 2 ^ process_id;
	KeSetSystemAffinityThread(ka);
	KdPrint(("current thread is executing in %d logical process\n", process_id));
	PAGED_CODE();
	
	//ΪVMM����ջ�������ϣ�����VMXON������VMM�ˣ��õ�ջ�ǵ�ǰ��ss rsp��
	//��������ϣ��VMMʹ����ջ��������·���һ��������vmcs��host data�У�����һ��vm exit��
	//ʹ���·������Ϊrsp
	//��Ҫע����ǣ�������Ҫ��vmm_stack ��vmm��������ַ
	//���ǵ�vmm��windows�����µ������������ʹ�õ�cr3��ϵͳ���̵�cr3
	//�������������ַ����allocate���������ĵ�ַ��
	//�������ǽ���guest�󣬼���ִ��ʣ��Ĵ��룬���ʱ���windows����guest�ˡ�

	ULONG64 vmm_stack_va = ExAllocatePoolWithTag(NonPagedPool, VMM_STACK_SIZE, DRIVER_TAG);
	if (vmm_stack_va==NULL) {
		KdPrint(("allocate vmm stack fail\n"));
		return FALSE;
	}
	RtlZeroMemory(vmm_stack_va, VMM_STACK_SIZE);
	vmm_state_ptr->vmm_stack = vmm_stack_va;

	//����msrbitmap,����intel�ֲᣬ vmcs�ﱣ��һ��msrbitmap�����ڴ�ָ��
	//ָ��һ��4kb������
	ULONG64 msr_bitmap = ExAllocatePoolWithTag(NonPagedPool, PAGE_SIZE, DRIVER_TAG);
	if (msr_bitmap == NULL) {
		KdPrint(("allocate msr bitmaps fail\n"));
		return FALSE;
	}
	RtlZeroMemory(msr_bitmap, PAGE_SIZE);
	vmm_state_ptr->msr_bitmaps_va = msr_bitmap;
	vmm_state_ptr->msr_bitmaps_pa = vitual_to_physical(msr_bitmap);

	if (!clear_vm_state(vmm_state_ptr)) {
		return FALSE;
	}
	if (!vm_ptr_load(vmm_state_ptr)) {
		return FALSE;
	}

}