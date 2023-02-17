// mem_x64.c : ʵ����x64/IA32e/��ģʽ��ҳ/�ڴ�ģ�͡�.
// ��      ��: ѧ�����򶹶�
//
#include "MemX64.h"
#include "VmWareApp.h"
#include "vad.h"

#define PTE_SWIZZLE_MASK                               0x0         //Todo:��ʱ��֧��
#define PTE_SWIZZLE_BIT                                0x10        // nt!_MMPTE_SOFTWARE.SwizzleBit
#define MEM_X64_PTE_IS_HARDWARE(pte)                  (pte & 0x01)
#define MEM_X64_MEMMAP_DISPLAYBUFFER_LINE_LENGTH      89

#define MEM_X64_PTE_PAGE_FILE_NUMBER(pte)          ((pte >> 12) & 0x0f)
#define MEM_X64_PTE_PAGE_FILE_OFFSET(pte)          ((pte >> 32) ^ (!(pte & PTE_SWIZZLE_BIT) ? PTE_SWIZZLE_MASK : 0))

#define MEM_X64_PTE_IS_FILE_BACKED(pte)            (((pte & 0xffff80000000000f) == 0xffff800000000000))

/*
nt!_MMPTE_PROTOTYPE
+ 0x000 Valid            : Pos 0, 1 Bit
+ 0x000 DemandFillProto : Pos 1, 1 Bit
+ 0x000 HiberVerifyConverted : Pos 2, 1 Bit
+ 0x000 ReadOnly : Pos 3, 1 Bit
+ 0x000 SwizzleBit : Pos 4, 1 Bit
+ 0x000 Protection : Pos 5, 5 Bits
+ 0x000 Prototype : Pos 10, 1 Bit
+ 0x000 Combined : Pos 11, 1 Bit
+ 0x000 Unused1 : Pos 12, 4 Bits
+ 0x000 ProtoAddress : Pos 16, 48 Bits
*/
#define MEM_X64_PTE_PROTOTYPE(pte)                    (((pte & 0x8000000000070401) == 0x8000000000000400) ? ((pte >> 16) | 0xffff000000000000) : 0)

/*
nt!_MMPTE_TRANSITION
+ 0x000 Valid            : Pos 0, 1 Bit
+ 0x000 Write : Pos 1, 1 Bit
+ 0x000 Spare : Pos 2, 1 Bit
+ 0x000 IoTracker : Pos 3, 1 Bit
+ 0x000 SwizzleBit : Pos 4, 1 Bit
+ 0x000 Protection : Pos 5, 5 Bits
+ 0x000 Prototype : Pos 10, 1 Bit
+ 0x000 Transition : Pos 11, 1 Bit
+ 0x000 PageFrameNumber : Pos 12, 36 Bits
+ 0x000 Unused : Pos 48, 16 Bits
*/
#define MEM_X64_PTE_TRANSITION(pte)                   (((pte & 0x0c01) == 0x0800) ? ((pte & 0xffffdffffffff000) | 0x005) : 0)
#define MEM_X64_PTE_IS_TRANSITION(H, pte, iPML)       ((((pte & 0x0c01) == 0x0800) && (iPML == 1) && (H->vmm.tpSystem == VMM_SYSTEM_WINDOWS_X64)) ? ((pte & 0xffffdffffffff000) | 0x005) : 0)
#define MEM_X64_PTE_IS_VALID(pte, iPML)               (pte & 0x01)


DWORD64 MemX64Prototype(_In_ DWORD64 pte,_In_ DWORD64 DirectoryTableBase)
{
	DWORD64 PtePage = 0;
	do
	{
		if (!pte || !DirectoryTableBase){
			break;
		}
		if (!VMReadVmVirtualAddr(&PtePage, DirectoryTableBase, MEM_X64_PTE_PROTOTYPE(pte), 8)) {
			break;
		}

		if (!PtePage) {
			break;
		}

		//�����ԭ��PTE������Subsection Pte,������ʱû�д���
		//�������PTE��Hard/Transition/PageFile ��������ȥ����
		if (MEM_X64_PTE_PROTOTYPE(PtePage)) {
			PtePage = 0;
			break;
		}

	} while (FALSE);
	
	return PtePage;
}

//�������ڴ��ȡ����ҳ��ҳ��
VMM_PTE_TP MemX64TransitionPaged(_In_ DWORD64 va, _In_ DWORD64 pte, _In_ DWORD64 DirectoryTableBase, _Out_ PDWORD64 ppa)
{
	DWORD PfNumber = 0;
	DWORD PfOffset = 0;
	DWORD PteTp = VMM_PTE_TP_NA;
	do
	{
		/*
		��ЧPTE(Ӳ��PTE)
		PTE��������Чλ��MMU�ͷ��������ã���ִ�е������ַ��ת��,����Ͳ����Ƿ�ҳ�ڴ�
		*/
		if (MEM_X64_PTE_IS_HARDWARE(pte))
		{
			PteTp = VMM_PTE_TP_HARDWARE;
			*ppa = pte & 0x0000fffffffff000;
			break;
		}
		/*��Чpte(���PTE)--------------------------------------------------------------------------------------------------------------------------------------------
		4����ЧPTE��1��������ЧPTE(ԭ��PTE)

		����ڵ�ַת��������������PTE����ЧλΪ�㣬��PTE��ʾ��Ч��ҳ��������ʱ�������ڴ�����쳣��ҳ����
		MMU����PTE��ʣ��λ����˲���ϵͳ����ʹ����Щλ���洢����ҳ�����Ϣ���⽫�����ڽ��ҳ�����
		�����г���������Чpte����ṹ����Щͨ������Ϊ���pte����Ϊ���������ڴ������������MMU���͵�*/
		/*��ЧPTE�����֣�
		1���ڷ�ҳ�ļ���
		2��Ҫ��0ҳ��
		3��ҳ��ת����
	    4��δ֪����Ҫ���VAD��
		*/	

		/* ԭ��״̬
           1����ͬ������ҳ���������಻ͬ�Ľ���֮�乲�����������������Ϊ������ֻ�ö�� PTE ����ͬһ������ҳ�档����ϵͳ�����������Э������ҳ����޼���
		����������ͬһ����ҳ������ã��������ϵͳ��Ҫ���罫����ҳ�����¶�λ��ҳ���ļ��У�������Ҫ�����͸���������Щ���á�
		����Ч�ʷǳ��ͣ���� Windows ���������ʹ��һ�֡��������ӡ�PTE ������ҳ�涨����һ�� PTE - ��Ϊԭ�� PTE��
		��ˣ�����ֻ��Ҫ����ԭ�� PTE���������ù����ڴ�� PTE �����Զ����¡�

		  2����Ӳ��PTE ��ͬ��ԭ�� PTE ���ڲ��ڴ���������ݽṹ��CPU �Ӳ�ʹ������ִ�����⵽�����ַת�������ǽ��� Windows ҳ�������������ڽ������ҳ���ϵ�ҳ�����

		ԭ��PTE:ԭ��PTE����4KB��ҳ�棬
		����ԭ��PTE������,����ҳ��ɴ�������6��״̬֮һ��
		��l�������Ч��active��valid����Ϊ�������̿��Է���, ���Ը�ҳ��λ�������ڴ���
		��2��ת����transltionĿ��ҳ��λ���ڴ��еĴ����б���޸��б���λ������ ���б��е��Ρ������ڡ�
		��3�����޸Ĳ�д����modihed no write��Ŀ��ҳ��λ���ڴ���,��λ�����޸Ĳ�д���б��ڣ���
		��4��Ҫ���㣨demandzero��Ŀ��ҳ��Ӧ����һ����ҳ�������㡣
		��5��ҳ���ļ���pageFile��Ŀ��ҳ��פ����ҳ���ļ���
		��6��ӳ���ļ���mappedFile����Ŀ��ҳ��פ����ӳ���ļ���
		*/
		if (MEM_X64_PTE_PROTOTYPE(pte)) {
			PteTp = VMM_PTE_TP_PROTOTYPE;
			pte = MemX64Prototype(pte, DirectoryTableBase);
			if (MEM_X64_PTE_IS_HARDWARE(pte)) {
				*ppa = pte & 0x0000fffffffff000;
				break;
			}
		}

	     /*����״̬
		   Windows��һ������������������ - һ���ӽ��̵Ĺ�������ɾ��ҳ����������������POSIX�и���Ϊ��֪����פ���������������ǽ��̿��Է��ʵ�����ҳ��ļ��ϣ����������
		���������Խ�ҳ��ɾ����ҳ���ļ��У�������ϵͳ�п�������ҳ������������ǣ�����������ҳ��д��ҳ���ļ����������Ƚ�ҳ������ת��״̬��
		�������Ժ�ҳ��д��ҳ���ļ���ͬʱ���ڴ����԰�����Ч���ݣ��Է������Ժ���Ҫ��ҳ�棨�����Կ��ٴ���ط��ص��������У���
		��ˣ�ת���е�ҳ�������Ч���ݣ����ǵ����̷�����ʱ��Ӳ���Ὣpageerror��ҳ������ϵͳ��������У��ô�����򽫼򵥵ؽ�ҳ����Ϊ��Ч��
		�����ת������־���ڴ�״̬������ԭ�͡���־���ڹر�״̬����ҳ�洦�ڡ�ת����״̬
		*/
		if (MEM_X64_PTE_TRANSITION(pte)) {
			PteTp = VMM_PTE_TP_TRANSITION;
			pte = MEM_X64_PTE_TRANSITION(pte);
			if ((pte & 0x0000fffffffff000)) {
				*ppa = pte & 0x0000fffffffff000;
			}
			break;
		}

		//Todo:Ŀǰû��ʵ�ֶ�дRead/Write pagefile.sys����������д��
		PfNumber = MEM_X64_PTE_PAGE_FILE_NUMBER(pte);
		PfOffset = MEM_X64_PTE_PAGE_FILE_OFFSET(pte);

		/*VADԭ��PTE��VADӲ��PTE

		 VADԭ��PTE
		���Ӳ�� PTE ��������ԭ�� PTE�������� Valid = 0��Prototype = 1�������� ProtoAddress ��������ֵ0xFFFFFFFF0000���� VAD ԭ�͡�
		����������£����Ǳ����ҵ��������۵������ַ���Ӧ��VAD����Ȼ��MMVAD �ṹ����һϵ�������� VAD ��Χ���Ӧ�� PTE��
		Ȼ�����Ǽ���ԭʼ�����ַ��VAD��������ƫ���������ҵ�����Ӧ��PTE��

		���磬�������ǳ��Խ�����ַ0x10000��
		����ҳ������ȷ��VADԭ��PTE����ProtoAddress = 0xFFFFFFFF0000����
		�����ڹ���VAD����������Ȥ�����򡣼��������ҵ�һ����0x8000��0x20000������
		�������_MMVAD������һ�� FirstPrototypePte ��Ա��������ָ�� 0xFFFF1000000����
		��ˣ�������Ҫ�� PTE λ�� ��0x10000 - 0x8000�� / 0x1000 + 0xFFFF1000000
		���ǴӸ� PTE ���������ַ��

		 VadӲ��PTE
		��� PTE ��ȫΪ 0�����ʾӦ��ѯ VAD����ָ�� PTE �� PDE ��Ч��������ҳ����Ч��ʱ����������ƺ�Ҳ����ˡ�����������£�������Ҫ��������VADԭ��PTE��ͬ�ķ�ʽ���VAD��
		����״̬�ƺ�������״̬��ͬ��*/

		if (va && !MEM_IS_KERNEL_ADDR_X64(va) && (!pte || (PfOffset == 0xffffffff))) {
			//Ŀǰ��δʵ��
			PteTp = VMM_PTE_TP_PROTOTYPE;
			VADFindVadByPte();
			break;
		}

		//������ں˵�ַ,����PTEΪ����ֱ���˳�
		if (!pte) {
			break;
		}

		// Ҫ���㣨demandzero��Ŀ��ҳ��Ӧ����һ����ҳ��������[ nt!_MMPTE_SOFTWARE ]
		//����ЧPTE�ĵ�2�������ԭ��PTE�ĵ�4�������
		if (!PfNumber && !PfOffset) {
			PteTp = VMM_PTE_TP_DEMANDZERO;
			break;
		}

		//WIN11�ļ�֧�ֵ��ڴ�(Ŀǰ��֧��)
		if (MEM_X64_PTE_IS_FILE_BACKED(pte)) {
			PteTp = VMM_PTE_TP_FILE_BACKED;
			break;
		}

		//����������win10��ѹ���ڴ�������ڴ�(PageFile.sys)
	} while (FALSE);

	return PteTp;
}
