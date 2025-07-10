;loader 3.1版本, 开启分页

%include "boot.inc"

SECTION loader vstart=LOADER_BASE_ADDR
;栈顶定义到loader起始地址上(why?)
LOADER_STACK_TOP equ LOADER_BASE_ADDR
;jmp loader_start

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

;如上共定义了64个段描述符, 64*8=2^9, 共0x200字节

;;;;0x900 + 0x200 = 0xb000;;;;
;我们定义关键地址0xb00, 存储可用物理内存
total_mem_bytes dd 0


;定义选择子
SELECTOR_CODE equ (0x0001<<3) + TI_GDT + RPL0
SELECTOR_DATA equ (0x0002<<3) + TI_GDT + RPL0
SELECTOR_VIDEO equ (0x0003<<3) + TI_GDT + RPL0

;lgdt 48位数值
;前16位 界限值 ,后32位 GDT基址
gdt_ptr dw GDT_LIMIT
        dd GDT_BASE

loader_message db '3.1 loader in real by Q' ;21
;4+6+21=31字节,接下来定义ards缓冲区与ards数量,并人工对齐
    ards_buf times 221 db 0
    ards_nr dw 0

;;;;---0x200+ 4 + 6 + 21 + 223 + 2 = 0x300;;;;
;;;;0x900 + 0x300 = 0xc00;;;;

loader_start:
    ;利用BIOS中断打印loader信息
    mov sp, LOADER_BASE_ADDR
    mov bp, loader_message
    mov cx, 23
    mov ax, 0x1301
    mov bx, 0x001f
    mov dx, 0x1800
    int 0x10
    
    ;debug: jmp .e820_failed_try_e801
    ;debug: jmp .e801_failed_try_88
    ;测试是否三个方式都健全可用
;;;; int 15h  E820
    xor ebx, ebx
    mov edx, 0x534d4150
    mov di, ards_buf

    .e820_mem_get_loop:
        mov eax, 0x0000e820  ;每次读取后都要重置
        mov ecx, 20 ;一次20字节(ards结构20字节)
        int 0x15
        ; 若CF=1,则发生错误, 下跳转e801子方法
        jc .e820_failed_try_e801
        add di,cx   ;缓冲区向下20字节
        inc word [ards_nr]  ;已读取ARDS数加一
        cmp ebx, 0  ; 若读取完,则ebx为0,下句不跳转
        jnz .e820_mem_get_loop
    ;;;接下来找出(base_add_low + length_low)的最大值,为内存容量
    mov cx, [ards_nr]
    mov ebx,ards_buf
    xor edx,edx     ;接下来以edx作为num_max进行冒泡排序
    .find_max_mem_area:
        mov eax,[ebx]
        add eax,[ebx+8]
        add ebx,20
        cmp edx,eax
        jge .next_ards
        mov edx,eax
        .next_ards:
            loop .find_max_mem_area ;cx计数器自递减并判断
        jmp .mem_get_ok


    .e820_failed_try_e801:
        mov eax,0x0000e801
        int 0x15
        jc .e801_failed_try_88
        ;先算出低15M的内存
        mov cx,0x400    ;1024,用于单位转化  
        mul cx  ;高16位在dx中,低16位在ax中
        shl edx,16  ;左移16位
        and eax,0x0000ffff
        or edx,eax
        add edx, 0x100000   ;加1M,15~16未计入
        mov esi,edx ;将15M以下的内存先存入esi中备份

        ;再算出16M以上的内存
        xor eax,eax
        mov ax,bx
        mov ecx, 0x10000    ;64 KB,同理单位转换
        mul ecx     ;32位乘法,高32位存入edx,低32位存入eax
        ;此方法只能测出4G以内的内存,故eax容量足够,edx定为0
        add esi,eax
        mov edx,esi
        jmp .mem_get_ok    
        
    
    .e801_failed_try_88:
        mov ah, 0x88
        int 0x15
        jc .error_hlt
        and eax,0x0000ffff
        mov cx,0x400
        mul cx  
        shl edx,16
        or edx, eax
        add edx, 0x100000   ;+1M
        ;自然顺承到.mem_get_ok

    .mem_get_ok:
        mov [total_mem_bytes], edx

    .error_hlt:
        ;尚未设置
        nop
    
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
    

;;;;开启内存分页;;;;;
;Step 1: 设置PTE、PDE
    call setup_page
    ;Step 1.2: 一些处理
    sgdt [gdt_ptr]  ;先保存下来
    mov ebx, [gdt_ptr + 2]  ;GDT基址
    or dword [ebx + 0x18 + 4], 0xc0000000   ;第三个可用段(视频段),高四位字节
    add dword [gdt_ptr + 2], 0xc0000000 ;GDT基址映射到内核的高地址区    
    add esp, 0xc0000000 ;栈指针
;Step 2:页目录地址赋值到cr3
    mov eax, PAGE_DIR_TABLE_POS
    mov cr3, eax
    ;实际上, cr3的后12位为标志位, 但此处恰好均为0

;Step 3:打开cr0的PG位(第31位)
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax

    lgdt [gdt_ptr]
    mov byte [gs:162],'V'
    mov byte [gs:163],0xA4    
    
    jmp $

;--------创建页目录&页表---------
setup_page:

;将PTE内存清空
    mov ecx, 4096
    mov esi, 0
.clear_page_dir:
    mov byte [PAGE_DIR_TABLE_POS + esi], 0
    inc esi
    loop .clear_page_dir

;创建PDE
.create_pde:
    mov eax, PAGE_DIR_TABLE_POS
    add eax, 0x1000 ;现在eax是第一个页表的地址
    mov ebx, eax    ;ebx备份eax

    ;将内核3G以上区域(0xc0000000)与真实4M以下地址的目录项都映射到第一页表(0~4M物理地址)
    or eax, PG_US_U | PG_RW_W | PG_P
    mov [PAGE_DIR_TABLE_POS + 0x0], eax ;第一个目录项
    mov [PAGE_DIR_TABLE_POS + 0xc00], eax
    ;解释一下为什么是0xc00: 由于物理页大小4K对齐,且一个页表有1024个页,且每个页目录项大小为4B,下标需要乘4
    ;故0xc0000000 = 0xc00/4*4K*1K = 0xc00 * 1M = 0xc0000000(末尾加上五个0)
    sub eax,0x1000
    ;1024*4 = 4092
    mov [PAGE_DIR_TABLE_POS + 4092], eax    ;末尾目录项指向自己(页目录)

;创建PTE
;此处只为1M低端内存分配 1M/4K=256页
    mov ecx, 256
    mov esi, 0
    mov edx, PG_US_U | PG_RW_W | PG_P
.create_pte:
    mov [ebx+esi*4],edx ;edx基址对应物理地址0x0000, 属性位如上
    ;ebx在.create_pde中已经设置为第一个页表的地址
    add edx, 4096
    inc esi
    loop .create_pte

;创建内核其他表的PDE
    mov eax, PAGE_DIR_TABLE_POS
    add eax, 0x2000
    or eax, PG_US_U | PG_RW_W | PG_P
    mov ebx, PAGE_DIR_TABLE_POS
    mov ecx, 254
    mov esi, 769
.create_kernel_pde:
    mov [ebx+esi*4], eax
    inc esi
    add eax, 0x1000
    loop .create_kernel_pde

    ret
    
