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
VECTOR 0x20,ZERO
