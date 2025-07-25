#include "timer.h"
#include "../lib/kernel/io.h"
#include "../lib/kernel/print.h"
#include "../thread/thread.h"
#include "../kernel/debug.h"
#include "../device/timer.h"
#include "../kernel/interrupt.h"

#define IRQ0_FREQUENCY  100
#define INPUT_REQUENCY  1193180
#define COUNTER0_VALUE  INPUT_REQUENCY / IRQ0_FREQUENCY
#define COUNTER0_PORT   0x40
#define COUNTER0_NO     0
#define COUNTER_MODE    2
#define READ_WRITE_LATCH    3
#define PIT_CONTROL_PORT    0x43

uint32_t ticks; //紫中断开启后,总刻数

static void intr_timer_handler(void){
    
    struct task_struct* cur_thread = running_thread();
    // D_put_str("Now in timer interrupt: cur_thread=");
    // D_put_str(cur_thread->name);
    // D_put_char('\n');
    ASSERT(cur_thread->stack_magic == 0x20070515);  //检查栈溢出
    cur_thread->elapsed_ticks++;
    ticks++;
    if(cur_thread->ticks == 0)
        schedule(); //当前线程时间片用完, 需要调度新的线程
    else
        cur_thread->ticks--;
}

static void frequency_set(uint8_t counter_port, \
                          uint8_t counter_no, \
                          uint8_t rwl, \
                          uint8_t counter_mode, \
                          uint16_t counter_value){
    outb(PIT_CONTROL_PORT, \
         (uint8_t)(counter_no << 6 | rwl << 4 | counter_mode << 1));
    outb(counter_port, (uint8_t)counter_value);
    outb(counter_port, (uint8_t)(counter_value>>8));
    put_str("   pit_init done\n");
}  

void timer_init(){
    put_str("timer_init start\n");
    frequency_set(COUNTER0_PORT, \
                  COUNTER0_NO, \
                  READ_WRITE_LATCH, \
                  COUNTER_MODE, \
                  COUNTER0_VALUE);
    register_handler(0x20, intr_timer_handler);
    put_str("timer_init done\n");
    
}
                        