#pragma once

// pe.h : �������ַ�ռ��н���PEӳ�����ض��塣
// �� ��: ѧ�����򶹶�
//
#include "windows.h"
#define PE_MAX_SUPPORTED_SIZE           0x20000000

DWORD PEGetSectionNumber(_In_ DWORD64 VmModuleBase);
DWORD64 PEGetSectionsBaseAddr(_In_ DWORD64 VmModuleBase);
