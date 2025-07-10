;loader 1.0版本, 仅供调试, 打印版本信息
%include "boot.inc"

SECTION loader vstart=0x900
    mov byte [gs:0x00],'1'
    mov byte [gs:0x01],0xA4
    mov byte [gs:0x02],' '
    mov byte [gs:0x03],0xA4
    mov byte [gs:0x04],'L'
    mov byte [gs:0x05],0xA4
    mov byte [gs:0x06],'O'
    mov byte [gs:0x07],0xA4
    mov byte [gs:0x08],'A'
    mov byte [gs:0x09],0xA4
    mov byte [gs:0x0a],'D'
    mov byte [gs:0x0b],0xA4
    mov byte [gs:0x0c],'E'
    mov byte [gs:0x0d],0xA4
    mov byte [gs:0x0e],'R'
    mov byte [gs:0x0f],0xA4
    mov byte [gs:0x10],' '
    mov byte [gs:0x11],0xA4
    mov byte [gs:0x12],'B'
    mov byte [gs:0x13],0xA4
    mov byte [gs:0x14],'Y'
    mov byte [gs:0x15],0xA4
    mov byte [gs:0x16],' '
    mov byte [gs:0x17],0xA4
    mov byte [gs:0x18],'Q'
    mov byte [gs:0x19],0xA4

    jmp $