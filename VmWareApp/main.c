#include "VmWareApp.h"
#include "KernelModeInject.h"
#include "debug.h"
#include "pe.h"

DWORD WINAPI ThreadProc(LPVOID lpThreadParameter) {
	DWORD pid = *(DWORD*)lpThreadParameter;

	DebugProcess(pid);
	return 0;
}

int main() {

	DWORD pid = 53688;
	if (!VmWareThroughInit(pid))
	{
		printf("main��VmWare Through Initialization Failed.\n");
		return -1;
	}
	////VmWare����Ҫ�����Ľ������ݳ�ʼ��,��ʱ����Ҫ
	//if (!VMFindVmProcessData("notepad.exe", VMGetVmwareDestProcData()))
	//{
	//	printf("main: Find Process Data Failed.\n");
	//	return -1;
	//}

	if (!KMIKernelInject())
	{
		printf("main��Kernel injection failed.\n");
		return -1;
	}
	VMLoadDriver("\\??\\c:\\test\\DriverTest.sys");
	return 0;
}
