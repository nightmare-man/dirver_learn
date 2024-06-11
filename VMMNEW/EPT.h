#pragma once
#include <ntddk.h>
#define DRIVER_TAG 0x123456u
union EXTENDED_PAGE_TABLE_POINTER {
	ULONG64 all;
	struct {
		ULONG64 ept_paging_struct_cache_type : 3;
		ULONG64 ept_page_walk_length : 3;//ҳ��ּ����ȣ�4��Ϊ3�� 5��Ϊ4
		ULONG64 ept_dirty_flag_enable : 1;
		ULONG64 reversed1 : 5;
		ULONG64 pml4_pa : 36;
		ULONG64 reversed2 : 16;
	}fields ;
};

union PAGE_MAP_LEVEL_4_ENTRY {
	ULONG64 all;
	struct {
		ULONG64 read_access : 1;
		ULONG64 write_access : 1;
		ULONG64 execute_access : 1;
		ULONG64 reversed1 : 5;
		ULONG64 dirty_flag : 1;//���ҳ�����������������ڴ��Ƿ񱻷��ʹ�
		ULONG64 reversed2 : 1;
		ULONG64 user_mode_execute_access : 1;//��������Եģ����mode based execute control
		//�������ˣ�������������ں�ִ���Ƿ����� ��������û�ִ���Ƿ��������򣬶��������
		//��Ϊִ���Ƿ������bit
		ULONG64 reversed3 : 1;
		ULONG64 referenced_pa : 36;
		ULONG64 reversed4 : 16;//must be zero
	}fields;
};
//������ʵ���������ֶ��壬һ��������ʵ��1GB��ҳ���,ֻ��PML4��PML3
//�������ַ��47��30λ����ҳ�棬��ҳ���С�� 2^30 1GB��С
//����һ���Ǳ�׼PML3����ָ��PML2
union PAGE_MAP_LEVEL_3_ENTRY_NORMAL {
	ULONG64 all;
	struct {
		ULONG64 read_access : 1;
		ULONG64 write_access : 1;
		ULONG64 execute_access : 1;
		ULONG64 reversed1 : 5;
		ULONG64 dirty_flag : 1;//���Ƶ�1GB�ڴ������Ƿ񱻷��ʣ�Ҫ��EPTP bit6 1
		ULONG64 reversed2 : 1;
		ULONG64 user_mode_execute_access : 1;//ͬ��
		ULONG64 reversed3 : 1;
		ULONG64 referenced_pa : 36;
		ULONG64 reversed4 : 16;
	}fields;
};

//ͬ��PML2Ҳ�����֣������Ǳ�׼�涨��
union PAGE_MAP_LEVEL_2_ENTRY_NORMAL {
	ULONG64 all;
	struct {
		ULONG64 read_access : 1;
		ULONG64 write_access : 1;
		ULONG64 execute_access : 1;
		ULONG64 reversed1 : 5;
		ULONG64 dirty_flag : 1;//���Ƶ�2MB�ڴ������Ƿ񱻷��ʣ�Ҫ��EPTP bit6 1
		ULONG64 reversed2 : 1;
		ULONG64 user_mode_execute_access : 1;//ͬ��
		ULONG64 reversed3 : 1;
		ULONG64 referenced_pa : 36;
		ULONG64 reversed4 : 16;
	}fields;
};
union PAGE_MAP_LEVEL_1_ENTRY {
	ULONG64 all;
	struct {
		ULONG64 read_access : 1;
		ULONG64 write_access : 1;
		ULONG64 execute_access : 1;
		ULONG64 memory_cache_type : 3;//��������1KB�ڴ�Ļ��淽ʽ
		ULONG64 ignore_pat_cache_type : 1;
		ULONG64 reversed1 : 1;
		ULONG64 dirty_flag : 1;//�Ƿ񱻷��ʹ���Ҫ��EPTP bit 6 1
		ULONG64 write_dirty_flag : 1;//�Ƿ�д���,ͬҪ��
		ULONG64 user_mode_execute_access : 1;
		//���VMCS�� mode-based execute control for ept������
		//��������û�ģʽ��ִ��Ȩ�ޣ� bit2 �����ں�,���� bit2�ȿ���
		//�û�ִ��Ȩ�ޣ�Ҳ�����ں�
		ULONG64 reversed2 : 1;
		ULONG64 referenced_pa : 36;
		ULONG64 reversed3 : 1;
	}fields;
};

ULONG64 init_eptp();
