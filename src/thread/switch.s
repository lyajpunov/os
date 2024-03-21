[bits 32]
section .text
global switch_to
switch_to:
    ;栈中此处是返回地址
    push esi
    push edi
    push ebx
    push ebp

    mov eax, [esp + 20]		; 得到栈中的参数cur, cur = [esp+20]
    mov [eax], esp          ; 保存栈顶指针esp. task_struct的self_kstack字段,
                            ; self_kstack在task_struct中的偏移为0,
                            ; 所以直接往thread开头处存4字节便可。
;------------------  以上是备份当前线程的环境，下面是恢复下一个线程的环境  ----------------
    mov eax, [esp + 24]  ; 得到栈中的参数next, next = [esp+24]
    mov esp, [eax]		 ; pcb的第一个成员是self_kstack成员,用来记录0级栈顶指针,
                         ; 用来上cpu时恢复0级栈,0级栈中保存了进程或线程所有信息,包括3级栈指针
    pop ebp
    pop ebx
    pop edi
    pop esi
    ret				; ret从栈顶返回，这里栈顶就是返回地址
                    ; 
