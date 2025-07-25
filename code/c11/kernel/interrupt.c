#include "interrupt.h"
#include "global.h"
#include "../lib/kernel/stdint.h"
#include "../lib/kernel/io.h"
#include "../lib/kernel/print.h"

#define IDT_DESC_CNT 0x30   //48个
#define PIC_M_CRTL 0x20
#define PIC_M_DATA 0x21
#define PIC_S_CRTL 0xa0
#define PIC_S_DATA 0xa1
#define EFLAGS_IF 0x00000200
#define GET_EFLAGS(EFLAG_VAR) asm volatile("pushfl; popl %0":"=g"(EFLAG_VAR))

/*中断们描述符 */
struct gate_desc {
    uint16_t    func_offset_low_word;
    uint16_t    selector;
    uint8_t     dcount;
    uint8_t     attribute;
    uint16_t    func_offset_high_word;
};


static struct gate_desc idt[IDT_DESC_CNT];
char* intr_name[IDT_DESC_CNT];
intr_handler idt_table[IDT_DESC_CNT];



//静态函数声明
static void make_idt_desc(struct gate_desc* p_gdesc,uint8_t attr,intr_handler function);
static void general_inir_handler(uint8_t vec_nr);

//外部引用,定义在kernel.asm
extern intr_handler intr_entry_table[IDT_DESC_CNT];
/*初始化8259A*/
static void pic_init(void){
    //主片
    outb(PIC_M_CRTL, 0x11); //ICW1: 边沿触发,级联,需ICW4
    outb(PIC_M_DATA, 0x20); //ICW2: 起始中断向量号0x20
    outb(PIC_M_DATA, 0x04); //ICW3: IR2接从片
    outb(PIC_M_DATA, 0x01); //ICW4: 8086模式, 正常EOI
    //从片
    outb(PIC_S_CRTL, 0x11);
    outb(PIC_S_DATA, 0x28); //起始中断号0x28
    outb(PIC_S_DATA, 0x02); //设置从片连接到住片的IC2引脚
    outb(PIC_S_DATA, 0x01);

    //打开主片的IR0, 即仅接受时钟中断
    //值测试硬盘中断
    outb(PIC_M_DATA, 0xfc);
    outb(PIC_S_DATA, 0xff);
    put_str("   pic_init done\n");
}

/*创建中断门描述符*/
static void make_idt_desc(struct gate_desc* p_gdesc,uint8_t attr,intr_handler function){
    p_gdesc->func_offset_low_word = (uint32_t)function & 0x0000FFFF;
    p_gdesc->selector = SELECTOR_K_CODE;
    p_gdesc->dcount = 0;    //固定值
    p_gdesc->attribute = attr;
    //发现错误❌!!!
    p_gdesc->func_offset_high_word = ((uint32_t)function & 0xFFFF0000)>>16;
}

/*注册安装中断处理程序*/
void register_handler(uint8_t vector_no, intr_handler function){
    idt_table[vector_no] = function;
}

/*初始化描述符表 */
static void idt_desc_init(void){
    for(int i = 0; i < IDT_DESC_CNT; i++)
    {
        make_idt_desc(&idt[i], IDT_DESC_ATTR_DPL0, intr_entry_table[i]);
    }
}
static void general_inir_handler(uint8_t vec_nr){
    if(vec_nr == 0x27 || vec_nr == 0x2f) return;
    set_cursor(0);
    int cursor_pos = 0;
    while(cursor_pos < 320){
        put_char(' ');
        cursor_pos++;
    }
    set_cursor(0);
    put_str("!!!!!  exception message begin !!!!\n");
    set_cursor(88);
    put_str(intr_name[vec_nr]);
    put_int(vec_nr);
    if(vec_nr == 14){   //PAGE FAULT
        int page_fault_vaddr = 0;
        asm ("movl %%cr2, %0" : "=r"(page_fault_vaddr));
        put_str("\n page fault addr is ");
        put_int(page_fault_vaddr);
    }
    put_str("\n!!!!!    exception message end   !!!!!\n");
    while(1);
}
/*一般中断处理函数注册&异常名称注册*/
static void exception_init(void){
    for(int i = 0; i < IDT_DESC_CNT; i++){
        idt_table[i] = general_inir_handler;
        intr_name[i] = "unknown";
    }
    intr_name[0] = "#DE";
    intr_name[1] = "#DB";
    intr_name[2] = "NMI";
    intr_name[3] = "#BP";
    intr_name[4] = "#OF";
    intr_name[5] = "#BR";
    intr_name[6] = "#UD";
    intr_name[7] = "#NM";
    intr_name[8] = "#DF";
    intr_name[9] = "Coprecessor Segment Overrun";
    intr_name[10] = "#TS";
    intr_name[11] = "#NP";
    intr_name[12] = "#SS";
    intr_name[13] = "#GP";
    intr_name[14] = "#PF";
    //intr_name[15] = "";
    intr_name[16] = "#MF";
    intr_name[17] = "#AC";
    intr_name[18] = "#MC";
    intr_name[19] = "#XF";
}
enum intr_status intr_get_status(){
    uint32_t eflags = 0;
    GET_EFLAGS(eflags);
    return (EFLAGS_IF & eflags) ? INTR_ON : INTR_OFF;
}

enum intr_status intr_enable(){
    enum intr_status old_status;
    if(INTR_ON == intr_get_status()){
        old_status = INTR_ON;
        return old_status;
    }else{
        old_status = INTR_OFF;
        asm volatile("sti");
        return old_status;
    }
}

enum intr_status intr_disable(){
    enum intr_status old_status;
    if(INTR_ON == intr_get_status()){
        old_status = INTR_ON;
        asm volatile("cli":::"memory");
        return old_status;
    }else{
        old_status = INTR_OFF;
        return old_status;
    }
}

enum intr_status intr_set_status(enum intr_status status){
    return status & INTR_ON ? intr_enable() : intr_disable(); 
}

void idt_init()
{
    put_str("idt_init start\n");
    idt_desc_init();
    exception_init();
    pic_init();
    uint64_t idt_operand = ((sizeof(idt)-1)) | ((uint64_t)((uint32_t)idt) << 16);
    asm volatile("lidt %0": : "m"(idt_operand));
    put_str("idt_init done\n");
}
