[bits 32]
extern main
extern exit
section .text
global _start
_start:
    push ebx    ;argv
    push ecx    ;argc
    call main

    push eax
    call exit
