#include "../lib/kernel/print.h"
#include "init.h"
#include "debug.h"
#include "memory.h"
#include "../thread/thread.h"
#include "interrupt.h"
#include "../device/console.h"
#include "../device/ioqueue.h"
#include "../device/keyboard.h"

void k_thread_a(void* arg);
void k_thread_b(void* arg);


int main(void)
{
    put_str("\nKernel 2.0 by Q\n");
    init_all();
    //ASSERT(1==2);
    thread_start("consumer_a",31,k_thread_a,"A_");
    thread_start("consumer_b",16,k_thread_b,"B_");
    intr_enable();
    while(1);
    return 0;
}

void k_thread_a(void* arg){
    while(1){
        enum intr_status old_status = intr_disable();
        if(!ioq_empty(&kbd_buf)){
            console_put_str(arg);
            char byte = ioq_getchar(&kbd_buf);
            console_put_char(byte);
        }
        intr_set_status(old_status);
    }
}
void k_thread_b(void* arg){
    while(1){
        enum intr_status old_status = intr_disable();
        if(!ioq_empty(&kbd_buf)){
            console_put_str(arg);
            char byte = ioq_getchar(&kbd_buf);
            console_put_char(byte);
        }
        intr_set_status(old_status);
    }
}