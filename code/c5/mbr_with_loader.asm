;MBR 3.0版本 跳转到LOADER_BASE_ADDR + 0x300,便于loader数据对齐
%include "boot.inc"

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
    mov byte [gs:0x00],'3'
    mov byte [gs:0x01],0xA4
    mov byte [gs:0x02],' '
    mov byte [gs:0x03],0xA4
    mov byte [gs:0x04],'M'
    mov byte [gs:0x05],0xA4
    mov byte [gs:0x06],'B'
    mov byte [gs:0x07],0xA4
    mov byte [gs:0x08],'R'
    mov byte [gs:0x09],0xA4


    mov eax,LOADER_START_SECTOR     ;起始扇区lba地址
    mov bx,LOADER_BASE_ADDR         ;写入的地址
    ;修改: 读取4个扇区,loader比较大
    mov cx,4                        ;待读取扇区数
    call read_disk_m_16             

    jmp LOADER_BASE_ADDR + 0x300

    ;读取硬盘n个扇区
read_disk_m_16:
    mov esi,eax ;备份eax
    mov di,cx   ;备份cx

;读写硬盘
;Step 1:设置要读取的扇区数
    ;0x1f2 Sector Count
    mov dx,0x1f2    ;dx是out操作的操作数寄存器
    mov al,cl
    out dx,al

    mov eax,esi     ;恢复ax

;Step 2:将LBA地址存入0x1f3 ~ 0x1f6,并设置device flag打开LBA
    ;0~7位
    ;0x1f3 LBA low
    mov dx,0x1f3
    out dx,al

    ;8~15
    ;0x1f4 LBA mid
    mov cl,8
    shr eax,cl  ;shift right,右移8位
    mov dx,0x1f4
    out dx,al

    ;16~23
    ;0x1f5 LBA high
    shr eax,cl
    mov dx,0x1f5
    out dx,al

    ;0x1f6 device flag
    ;1 1 1 0 x x x x
    ;lba模式 ;LDA24~27位
    shr eax,cl
    and al,0x0f ;4~7位设置为0, 0~3位与LDA地址24~27位相同
    or al,0xe0  ;4~7位设置为1110,表示开启lba模式
    mov dx,0x1f6
    out dx,al

;Step 3:向0x1f7端口写入读命令,0x20
    ;0x1f7 (write) command
    mov dx,0x1f7
    mov al,0x20
    out dx,al

;Step 4:检测硬盘状态
    .not_ready:
        nop     ;相当于sleep
        ;0x1f7 (read) status
        in al,dx
        ;第3位1=数据已经准备好; 第7位1=硬盘正忙
        and al,0x88 ;仅考虑这两位
        cmp al,0x08 ;减法运算,cmp仅设置标志位,不改变操作数的值
        jnz .not_ready  ;第四位不是1,即未准备好数据,继续等待

;Step 5:从0x1f0端口读数据
    ;一个扇区512B, data寄存器一次读取16位,即2B
    mov ax,di
    mov dx,256
    mul dx  ; 计算ax*dx,结果的高16位在dx,低16位在ax
    mov cx,ax   ;执行次数=di(扇区数)*512/2
    mov dx,0x1f0
    
    .go_on_read:
        ;0x1f0 data
        in ax,dx
        mov [bx],ax
        add bx,2
        loop .go_on_read
    
    ret
    

    ;填空
    times 510-($-$$) db 0
    ;MBR末尾标识
    db 0x55,0xaa

