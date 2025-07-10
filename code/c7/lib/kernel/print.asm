;;;;打印函数;;;;

TI_GDT equ 0
RPL0 equ 0
SELECTOR_VIDEO equ (0x0003<<3) + TI_GDT + RPL0


[bits 32]
section .data
put_int_buffer dq 0

section .text
;---------- put_char ---------
;功能: 把栈中的一个字符写入光标位置
;输入: char ascii
;输出: void
global put_char
put_char:
    pushad  ;备份32位寄存器环境; 8个exx寄存器
    mov ax, SELECTOR_VIDEO
    mov gs, ax

    ;-----获取当前光标位置-----
    mov dx, 0x03d4  ;索引寄存器的端口号
    mov al, 0x0e    ;要读取的寄存器索引
    out dx,al   
    mov dx, 0x03d5  ;读写数据端口
    in al, dx   ;得到高8位
    mov ah, al
    ;下同理,获取低8位
    mov dx, 0x03d4  
    mov al, 0x0f    ;指令:得到光标低8位
    out dx,al   
    mov dx, 0x03d5
    in al, dx

    ;将光标存入bx
    mov bx, ax
    mov ecx, [esp + 36] ;获取待打印的字符
    ;pushad压入4*8=32个字节, 函数返回地址4字节, 故参数在36字节处
    
    ;处理特殊字符
    cmp cl, 0x0d    ;CR=0x0d   ,LF=0x0a, backspace=0x08
    jz .is_carriage_return ;回车
    cmp cl, 0x0a
    jz .is_line_feed    ;换行符
    cmp cl, 0x08
    jz  .is_backspace   ;退格符

    jmp .put_other


.is_backspace:
    dec bx
    shl bx,1 ;等价于*2
    mov byte [gs:bx], 0x20
    inc bx
    mov byte [gs:bx], 0x07
    shr bx, 1
    jmp .set_cursor

.put_other:
    shl bx, 1
    mov [gs:bx], cl
    inc bx
    mov byte [gs:bx], 0x07
    shr bx, 1
    inc bx
    cmp bx, 2000
    jl .set_cursor  ;小于2000, 未写完, 继续设置新光标
    ;若超出2000, 则换行处理

.is_line_feed:
.is_carriage_return:
    ;\r 回到行首
    xor dx,dx   ;dx是被除数的高16位,清零
    mov ax, bx  ;ax是被除数的低16位
    mov si, 80
    div si
    sub bx, dx  ;光标值 - 除80的余数=取整

.is_carriage_return_end:
    add bx, 80  ;光标移至下一行
    cmp bx, 2000    
.is_line_feed_end:
    jl .set_cursor  ;无需分页

.roll_screen:  
.roll_up_loop:
    mov ecx, 960
    mov esi, 0xc00b80a0
    mov edi, 0xc00b8000
    rep movsd   

    mov ebx, 3840
    mov ecx, 80
.cls:
    mov word [gs:ebx], 0x0720   ;黑底白字空格
    add ebx, 2
    loop .cls
    mov bx, 1920    ;最后一行首字母

.set_cursor:
    mov dx, 0x03d4
    mov al, 0x0e
    out dx, al
    mov dx, 0x03d5
    mov al, bh
    out dx, al
    mov dx, 0x03d4
    mov al, 0x0f
    out dx, al
    mov dx, 0x03d5
    mov al, bl
    out dx, al

.put_char_done:
    popad
    ret
    
;------ put_str -------
;功能: 以字符串首地址为参数, 打印字符串, 遇到\0停止
;输入: char* str
;输出: void
global put_str
put_str:
    push ebx
    push ecx
    xor ecx, ecx
    mov ebx, [esp + 12]
.go_on:
    mov cl, [ebx]
    cmp cl, 0
    jz .str_over
    push ecx
    call put_char
    add esp, 4
    inc ebx
    jmp .go_on
.str_over:
    pop ecx
    pop ebx
    ret

;------- put_int ------
;功能: 接收32位数字,打印成16进制数
;输入: uint32_t num
;输出: void
global put_int
put_int:
    pushad
    mov ebp, esp
    mov eax, [ebp + 36]
    mov edx, eax
    mov edi, 7  ;put_int_buffer中初始偏移量(最后一个)
    mov ecx, 8  ;待处理位数(hex)
    mov ebx, put_int_buffer

.16based_4bits:
    and edx, 0x0000000F
    cmp edx, 9
    jg .is_A2F
    add edx, '0'    ;0~9
    jmp .store
.is_A2F:
    sub edx, 10
    add edx, 'A'
.store:
    mov [ebx+edi], dl   ;存储缓冲区(地址从高到低)
    dec edi
    shr eax, 4
    mov edx, eax
    loop .16based_4bits

.ready_to_print:
    inc edi
.skip_prefix_0:
    cmp edi, 8
    je .full0
.go_on_skip:
    mov cl, [put_int_buffer+edi]
    inc edi
    cmp cl, '0'
    je .skip_prefix_0
    dec edi
    jmp .put_each_num
.full0:
    mov cl, '0'
.put_each_num:
    push ecx
    call put_char
    add esp, 4
    inc edi
    mov cl, [put_int_buffer+edi]
    cmp edi, 8
    jl .put_each_num
    popad
    ret
    

