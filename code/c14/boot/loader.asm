;loader 3.3版本, 加载内核修正版

%include "boot.inc"
%include "elf.inc"
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

loader_message db '3.3 loader in real by Q' ;21
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
    ;0xce1
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

;;;;加载kernel.bin到内存;;;;;

    mov eax, KERNEL_START_SECTOR
    mov ebx, KERNEL_BIN_BASE_ADDR
    mov ecx, 200
    call read_disk_m_32 ;0x0d15
    
;;;;开启内存分页;;;;;
;Step 1: 设置PTE、PDE
    call setup_page ;0x0d1a
    
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
    mov eax, cr0    ;0d4b
    or eax, 0x80000000
    mov cr0, eax

    lgdt [gdt_ptr]
    mov byte [gs:162],'V'
    mov byte [gs:163],0xA4    
    jmp SELECTOR_CODE:enter_kernel  ;刷新流水线,更新gdt

enter_kernel:
    call kernel_init    ;0x0d74
    mov esp, 0xc009f000 ;设置栈顶
    jmp KERNEL_ENTRY_POINT ;0x0d7ew




kernel_init:
    xor eax,eax
    xor ebx,ebx ;ebx记录程序头表地址
    xor ecx,ecx ;cx记录program header数量
    xor edx,edx ;dx记录program header大小   ;0x0d8b

    mov dx,  [KERNEL_BIN_BASE_ADDR + e_phentsize_offset] ;e_phentsize
    mov ebx, [KERNEL_BIN_BASE_ADDR + e_phoff_offset] ;e_phoff
    add ebx, KERNEL_BIN_BASE_ADDR
    mov cx,  [KERNEL_BIN_BASE_ADDR + e_phnum_offset] ;e_phnum
.each_segment:
    cmp byte [ebx + p_type_offset], PT_NULL ;p_type=0,说明此程序头未使用 ;0x0da7
    je .PTNULL
;调用memcpy(dst, src, size),下进行压栈传参操作
    push dword [ebx + p_filesz_offset]   ;p_filesz
    mov eax, [ebx + p_offset_offset]      ;p_offset
    add eax, KERNEL_BIN_BASE_ADDR
    push eax    ;src
    push dword [ebx + p_vaddr_offset]    ;p_vaddr
    call mem_cpy    ;0x0dbb
    add esp, 12 ;清理参数
    
.PTNULL:
    add ebx, edx
    loop .each_segment
    ret

mem_cpy:
    cld     ;clean direction.  movsb时各下标自增1
    push ebp
    mov ebp, esp
    push ecx
    mov edi, [ebp + 8]  ;dst
    mov esi, [ebp + 12] ;src
    mov ecx, [ebp + 16] ;size   ;0x0dd3
    rep movsb  ;逐字节拷贝
;.my_movsb:
    ;mov byte al, [esi]
    ;mov byte [edi], al
    ;loop .my_movsb

    pop ecx
    pop ebp
    ret

read_disk_m_32:
    mov esi,eax ;备份eax
    mov edi,ecx   ;备份cx
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
    mov ecx,eax   ;执行次数=di(扇区数)*512/2
    mov dx,0x1f0
    
    .go_on_read:
        ;0x1f0 data
        in ax,dx
        mov [ebx],ax
        add ebx,2
        loop .go_on_read
    ret


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
;创建PDE就是在PAGE_DIR_TABLE_POS~PAGE_DIR_TABLE_POS+4K这个页中, 填充页目录项所映射的页表地址
;每个页目录项所指代总地址是4M = 4K * (4K/4)(页大小4K/页表项大小4B) = 4M
;此处仅映射了0x00000000和0xc0000000这两个页
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
;意思就是在第一个页表内(逻辑地址0~4M)映射物理地址
;此处只为1M低端内存分配 1M/4K=256页
    mov ecx, 256
    mov esi, 0
    mov edx, PG_US_U | PG_RW_W | PG_P
.create_pte:
    ;ebx现在是第一个页表的基址
    ;向页表中映射实际的物理地址
    mov [ebx+esi*4],edx ;edx基址对应物理地址0x0000, 属性位如上
    ;ebx在.create_pde中已经设置为第一个页表的地址
    add edx, 4096   ;物理地址向下一个页
    inc esi ;页表中下标+1, 即下一个页表项, 而每一个页表项指代一个页
    loop .create_pte

;创建内核其他表的PDE
    mov eax, PAGE_DIR_TABLE_POS
    add eax, 0x2000
    ;现在eax是第二个页表的地址
    or eax, PG_US_U | PG_RW_W | PG_P
    mov ebx, PAGE_DIR_TABLE_POS
    ;ebx是页目录表基址
    mov ecx, 254
    mov esi, 769
.create_kernel_pte:
    ;填充内核所占用的内存的页目录项769~1022
    ;相当于是将3G~4G(逻辑地址)的页目录项指向后面的页表(从第二个页表开始, 因为第一个页表给了低端4M)
    ;可以发现, 实际分配个内核的空间是1G-4M
    ;因为最后一个页目录项指向页表自己, 而一个页目录项(一个页表)中有1024个页, 共4M
    ;即1024个页表项里面, 最后四分之一(3G~4G逻辑地址] aka. 第[769,1022]页目录项中, 少了最后两个第1023和1024项
    ;又因为物理低端4M内存是补给后面3G~4G的, 故相当于总共少了4M
    mov [ebx+esi*4], eax
    inc esi
    add eax, 0x1000
    loop .create_kernel_pte
    ret
    
