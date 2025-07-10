;loader 2.0版本, 构建GDT, 进入保护模式

%include "boot.inc"

SECTION loader vstart=LOADER_BASE_ADDR
;栈顶定义到loader起始地址上(why?)
LOADER_STACK_TOP equ LOADER_BASE_ADDR
jmp loader_start

;构建GDT
GDT_BASE:
    ;第一个置空
    dd 0x00000000
    dd 0x00000000
    CODE_DESC:
        dd 0x0000ffff
        dd DESC_CODE_HIGH4
    DATA_DESC:
        dd 0x0000ffff
        dd DESC_DATA_HIGH4
    VIDEO_DESC:
        ;显存起始地址0xb8000,段基址后16位在此处设为前16位中的8000,
        ;(段界限 + 1) * 粒度  = 段范围大小
        ;由于显存地址范围为0xb8000~0xbffff,段范围大小为0x8000,粒度为4K
        ;故段界限=2^15/2^5-1=8-1=7
        dd 0x80000007
        dd DESC_VIDEO_HIGH4

GDT_SIZE equ $-GDT_BASE
;GDT_LIMIT=gdt界限值=表长(字节)-1
;原理:CPU查询GDT时检查地址是否在[GDT_BASE,GDT_BASE+GDT_LIMIT]内,防止越界
GDT_LIMIT equ GDT_SIZE-1

times 60 dq 0    ;预留60个描述符空位


;定义选择子
SELECTOR_CODE equ (0x0001<<3) + TI_GDT + RPL0
SELECTOR_DATA equ (0x0002<<3) + TI_GDT + RPL0
SELECTOR_VIDEO equ (0x0003<<3) + TI_GDT + RPL0

;lgdt 48位数值
;前16位 界限值 ,后32位 GDT基址
gdt_ptr dw GDT_LIMIT
        dd GDT_BASE

loader_message db '3 loader in real by Q' ;21

loader_start:
    ;利用BIOS中断打印loader信息
    mov sp, LOADER_BASE_ADDR
    mov bp, loader_message
    mov cx, 21
    mov ax, 0x1301
    mov bx, 0x001f
    mov dx, 0x1800
    int 0x10
    
    ;;;接下来准备进入保护模式;;;
    ;由于尚未设置IDT,任何的中断都会导致错误
    ;此处用cli先关闭所有中断,防止报错
    ;分析debug后的经验之谈:)
    cli

;Step 1:  打开A20
    in al,0x92
    or al,0000_0010B
    out 0x92,al

    
;Step 2:  加载GDT
    lgdt [gdt_ptr]; 注意, lgdt的参数是该地址的数据值
    

;Step 3:  CR0第0位置设置为1
    mov eax, cr0
    or eax, 0x00000001
    mov cr0, eax
    ;刷新流水线
    jmp dword SELECTOR_CODE:p_mode_start

[bits 32]
p_mode_start:
    
    ;设置各段寄存器为选择子
    mov ax,SELECTOR_DATA
    mov ds,ax
    mov es,ax
    mov ss,ax
    mov esp,LOADER_STACK_TOP
    mov ax,SELECTOR_VIDEO
    mov gs,ax
    mov byte [gs:160], 'P'
    mov byte [gs:161], 0xA4
    jmp $

