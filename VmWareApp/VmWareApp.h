#pragma once
#include <windows.h>
#include <stdio.h>

#define NT_KERNEL_ID  0x1

//��ͬ�汾��ƫ��
typedef struct _NT_PROCESS_OFFSET
{
	DWORD DirectoryTableBaseOffset;
	DWORD ActiveProcessLinksOffset;
	DWORD ImageFileNameOffset;
	DWORD UniqueProcessId;
	DWORD VadRootOffset;

}NT_PROCESS_OFFSET, *PNT_PROCESS_OFFSET;

//��ͬ�汾���ں�����
typedef struct _NT_PROCESS_DATA
{
	DWORD ProcessSize;
	DWORD64 MemoryKernelDirbase; //CR3
	DWORD64 MemoryKernelEntry;   //UEFI KernelEntry:hal!HalpLMStub
	DWORD64 MemoryKernelBase;    //NT
	DWORD64 SystemProcessEprocess; //System���̵�EPROCESS
	PVOID PsLoadedModuleListPtr;

}NT_PROCESS_DATA, *PNT_PROCESS_VERSION_DATA;

//Ŀ����̵�����
typedef struct _VM_PROCESS_DATA
{
	DWORD64 PEB;
	DWORD PEB32;      // WoW64 only
	DWORD64 DestProcessEprocess;
	ULONGLONG  DestProcessCr3;
	DWORD64 VadRoot;

}VM_PROCESS_DATA, *PVM_PROCESS_DATA;

DWORD64 VMGetModuleBaseAddr(int ModuleID);
VOID* VMGetExportsFunAddr(DWORD64 ModuleBaseAddr, CHAR* FunName, BOOLEAN IsFun);
BOOLEAN VMFindVmProcessData(CHAR* ProcessName, VM_PROCESS_DATA* VmProcessData);
NT_PROCESS_OFFSET* VMGetNtProcOffset();
BOOLEAN VmWareThroughInit(DWORD VmWarePid);
BOOLEAN VMWriteVmVirtualAddr(PVOID sourceBuffer, DWORD64 directoryTableBase,
                             DWORD64 virtualAddress, SIZE_T size);
BOOLEAN VMGetWinX64ProcessOffset(_In_ NT_PROCESS_DATA* NtProcessData);
BOOLEAN VMReadVmVirtualAddr(PVOID targetBuffer, DWORD64 directoryTableBase,
                          DWORD64 virtualAddress, SIZE_T size);
BOOL VMWriteHostRegion(PVOID buffer, ULONG64 addr, SIZE_T size);
BOOL VMReadHostRegion(PVOID buffer, ULONG64 addr, SIZE_T size);
NT_PROCESS_DATA* VMGetNtKernelData();
DWORD64 VMTranslatePhyAddress(_In_ DWORD64 directoryTableBase,
                              _In_ DWORD64 virtualAddress,
	                          _Out_ PDWORD64 ppte);
MEMORY_BASIC_INFORMATION* VMGetHostMemBasicInfo();
HANDLE VMGetVmwareProcHandle();
VM_PROCESS_DATA* VMGetVmwareDestProcData();
BOOLEAN VMLoadDriver(CHAR* VmDriverLoadByImagePath);
