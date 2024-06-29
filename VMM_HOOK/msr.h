#pragma once

#define IA32_VMX_TRUE_PINBASED_CTLS_MSR 0x48d
#define IA32_VMX_PINBASED_CTLS_MSR 0x481
#define IA32_VMX_BASIC_MSR 0x480

#define IA32_VMX_CR0_FIXED0_MSR          0x486
#define IA32_VMX_CR0_FIXED1_MSR          0x487
#define IA32_VMX_CR4_FIXED0_MSR          0x488
#define IA32_VMX_CR4_FIXED1_MSR          0x489

#define IA32_VMX_PROCBASED_CTLS_MSR 0x482
#define IA32_VMX_TRUE_PROCBASED_CTLS_MSR 0x48e

#define IA32_VMX_PROCBASED_CTLS2_MSR 0x48b

#define IA32_VMX_EXIT_CTLS_MSR 0x483
#define IA32_VMX_TRUE_EXIT_CTLS_MSR 0x48f

#define IA32_VMX_ENTRY_CTLS_MSR 0x484
#define IA32_VMX_TRUE_ENTRY_CTLS_MSR 0x490

#define IA32_FEATURE_CONTROL_MSR 0x3a 
#define IA32_SYSENTER_CS_MSR  0x174
#define IA32_SYSENTER_ESP_MSR 0x175
#define IA32_SYSENTER_EIP_MSR 0x176
#define IA32_DEBUGCTL_MSR   0x1D9
#define MSR_FS_BASE_MSR        0xC0000100
#define MSR_GS_BASE_MSR        0xC0000101
#define MSR_SHADOW_GS_BASE_MSR 0xC0000102

#define IA32_MTRR_DEF_TYPE_MSR 0x2FF
#define IA32_MTRRCAP_MSR 0xFE
#define IA32_MTRR_FIX64K_00000_MSR 0x250
#define IA32_MTRR_FIX16K_80000_MSR 0x258
#define IA32_MTRR_FIX4K_C0000_MSR 0x268
#define IA32_MTRR_PHYSBASE0_MSR 0x200
#define IA32_MTRR_PHYSMASK0_MSR 0x201