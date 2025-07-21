;中断进入
;idt构建
[bits 32]
%define ERROR_CODE nop  ;
%define ZERO push 0     ;手动占位错误码, 便于返回时统一格式
extern put_str
extern put_int
extern put_char
extern idt_table
section .data
intr_str db "interrupt occur! code=", 0
global intr_entry_table

intr_entry_table:

%macro VECTOR 2
section .text
intr%1entry:

    %2  ;错误码
    ;保护上下文环境
    push ds
    push es
    push fs
    push gs
    pushad

    mov al, 0x20    ;手动发送EOI: 0010_0000
    out 0xa0, al    ;从片
    out 0x20, al    ;主片

    push %1
    call [idt_table + %1*4]
    jmp intr_exit

section .data       ;整合在intr_entry_table数组中
    dd intr%1entry   ;写入中断入口程序的地址
%endmacro

section .text
global intr_exit
intr_exit:
    add esp, 4      ;中断号
    popad
    pop gs
    pop fs
    pop es
    pop ds
    add esp, 4      ;error_code
    iret

VECTOR 0x00,ZERO
VECTOR 0x01,ZERO
VECTOR 0x02,ZERO
VECTOR 0x03,ZERO
VECTOR 0x04,ZERO
VECTOR 0x05,ZERO
VECTOR 0x06,ZERO
VECTOR 0x07,ZERO
VECTOR 0x08,ERROR_CODE
VECTOR 0x09,ZERO
VECTOR 0x0a,ERROR_CODE
VECTOR 0x0b,ERROR_CODE
VECTOR 0x0c,ERROR_CODE
VECTOR 0x0d,ERROR_CODE
VECTOR 0x0e,ERROR_CODE
VECTOR 0x0f,ZERO
VECTOR 0x10,ZERO
VECTOR 0x11,ERROR_CODE
VECTOR 0x12,ZERO
VECTOR 0x13,ZERO
VECTOR 0x14,ZERO
VECTOR 0x15,ZERO
VECTOR 0x16,ZERO
VECTOR 0x17,ZERO
VECTOR 0x18,ZERO
VECTOR 0x19,ZERO
VECTOR 0x1a,ZERO
VECTOR 0x1b,ZERO
VECTOR 0x1c,ZERO
VECTOR 0x1d,ZERO
VECTOR 0x1e,ERROR_CODE
VECTOR 0x1f,ZERO
VECTOR 0x20,ZERO    ;时钟中断
VECTOR 0x21,ZERO    ;键盘中断
VECTOR 0x22,ZERO    ;级联
VECTOR 0x23,ZERO    ;串口2
VECTOR 0x24,ZERO    ;串口1
VECTOR 0x25,ZERO    ;并口2
VECTOR 0x26,ZERO    ;软盘
VECTOR 0x27,ZERO    ;并口1
VECTOR 0x28,ZERO    ;定时时钟
VECTOR 0x29,ZERO    ;重定向
VECTOR 0x2a,ZERO    ;保留
VECTOR 0x2b,ZERO    ;保留
VECTOR 0x2c,ZERO    ;ps/2鼠标
VECTOR 0x2d,ZERO    ;fpu浮点单元异常
VECTOR 0x2e,ZERO    ;硬盘
VECTOR 0x2f,ZERO    ;保留



;;;;;;;;;;0x80中断;;;;;;;;;;
[bits 32]
extern syscall_table
section .text
global syscall_handler
syscall_handler:
;1.保存上下文环境
    push 0  ;使栈中格式统一
    push ds
    push es
    push fs
    push gs
    pushad
    push 0x80   ;同理, 保持栈格式统一

;2.传入参数
    push edx
    push ecx
    push ebx

;3.调用子功能处理函数
    call [syscall_table + eax*4]
    add esp, 12 ;跨过第二步所传参数

;4.返回值保存到内核栈中eax位置
    mov [esp + 8*4], eax    ;(7+1)*4
    jmp intr_exit


