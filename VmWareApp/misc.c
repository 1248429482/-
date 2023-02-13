#include "VmWareApp.h"
#include "misc.h"


#define STATUS_SUCCESS 0

typedef LONG(__stdcall* fn_RtlGetVersion)(PRTL_OSVERSIONINFOW lpVersionInformation);

DWORD MiscGetCurrentVersion()
{
	//���û��
	RTL_OSVERSIONINFOW OSVersionInfoEx = {0};
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    fn_RtlGetVersion pRtlGetVersion = (fn_RtlGetVersion)GetProcAddress(hNtdll, "RtlGetVersion");
    OSVersionInfoEx.dwOSVersionInfoSize= sizeof(RTL_OSVERSIONINFOW); 
	return pRtlGetVersion(&OSVersionInfoEx) == STATUS_SUCCESS ? OSVersionInfoEx.dwBuildNumber : 0;

}

DWORD MiscVmGetCurrentVersion()
{
	DWORD Ret = 0;
	//��'smss.exe'�����м�������ϵͳ�汾��Ϣ
	do
	{
		/*if (!VMFindVmProcessData("smss.exe"))
		{
			break;
		}*/

	} while (FALSE);

	return Ret;
	
}

int
WcharToChar(
	LPCWSTR lpWideCharStr,
	LPSTR* lppMultiByteStr)
{
	size_t cWchar = 0;
	int bRet = 0;
	do
	{
		if (!lpWideCharStr)
		{
			break;
		}
		cWchar = wcslen(lpWideCharStr) + 1;
		*lppMultiByteStr = malloc(cWchar * sizeof(CHAR));
		if (!*lppMultiByteStr)
		{
			break;
		}
		bRet = WideCharToMultiByte(
			CP_ACP,			// code page
			0,					// character-type options
			lpWideCharStr,		// address of string to map
			-1,					// number of bytes in string
			*lppMultiByteStr,	// address of wide-character buffer
			(int)(cWchar * sizeof(CHAR)),// size of buffer
			NULL,
			NULL
		);

	} while (FALSE);

	if (!bRet)
	{
		if (*lppMultiByteStr)
		{
			free(*lppMultiByteStr);
		}
	}

	return bRet;
}

BOOLEAN UnicodeToChar(UNICODE_STRING_EX* UnicodeStr, LPSTR* CharStr)
{
	BOOLEAN bRet = FALSE;
	//Ŀǰֻ����260��wchar��ȣ����ھ���Ҫmalloc�ռ䣬Ŀǰ��ʱ����Ҫ
	WCHAR WcharStr[MAX_PATH] = { 0 }; 
	do
	{
		if (NULL == UnicodeStr || NULL == CharStr)
		{
			break;
		}
		if (UnicodeStr->Length > MAX_PATH)
		{
			break;
		}
		UnicodeStringToWchar(UnicodeStr, WcharStr);

		bRet = (BOOLEAN)WcharToChar(WcharStr, CharStr);

	} while (FALSE);

	return bRet;
}