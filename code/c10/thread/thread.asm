[bits 32]

section .text
global switch_to
switch_to:
    ;;;备份上下文环境
    ;;维护储存上一个线程的环境
    push esi
    push edi
    push ebx
    push ebp
    ;4*(4+1) = 20
    mov eax, [esp + 20]   ;cur参数
    mov [eax], esp  ;栈顶指针esp到self_kstack

    ;;恢复下一个线程的环境
    mov eax, [esp + 24]   ;next
    mov esp, [eax]  ;从self_kstack栈顶读取esp
    pop ebp
    pop ebx
    pop edi
    pop esi
    ret