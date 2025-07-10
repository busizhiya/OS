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
    ;清屏
    
    mov ax,0x600
    mov bx,0x700
    mov cx,0
    mov dx,0x184f
    int 0x10

    
    ;获取光标
    mov ah,3
    mov bh,0
    int 0x10

;;; 打印开始
    mov ax,message
    mov bp,ax   ;   es:bp为串首地址


    mov cx,5    ;   cx为串长度
    mov ax, 0x1301
    mov bx, 0x2
    int 0x10

;;; 打印结束

    ;死循环
    jmp $


    message db "1 MBR"
    times 510-($-$$) db 0
    db 0x55,0xaa

