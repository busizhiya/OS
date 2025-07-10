SECTION MBR vstart=0x7c00
    ;初始化sreg
    ;初始跳转为:cs:ip=0x0000:0x7c00
    mov ax,cs
    mov ds,ax
    mov es,ax
    mov fs,ax
    mov ss,ax
    ;初始化栈针
    mov sp,0x7c00
    ;初始化gs段寄存器,为显卡段基址
    mov ax,0xb800
    mov gs,ax

    ;清屏
    mov ax,0x600
    mov bx,0x700
    mov cx,0
    mov dx,0x184f
    int 0x10
    ;直接向显存0xb800:xxxx写入信息
    mov byte [gs:0x00],'2'
    mov byte [gs:0x01],0xA4
    mov byte [gs:0x02],' '
    mov byte [gs:0x03],0xA4
    mov byte [gs:0x04],'M'
    mov byte [gs:0x05],0xA4
    mov byte [gs:0x06],'B'
    mov byte [gs:0x07],0xA4
    mov byte [gs:0x08],'R'
    mov byte [gs:0x09],0xA4


    ;死循环
    jmp $

    ;填空
    times 510-($-$$) db 0
    ;MBR末尾标识
    db 0x55,0xaa

