#include "../lib/kernel/print.h"
#include "init.h"
#include "debug.h"
#include "memory.h"
#include "../thread/thread.h"
#include "interrupt.h"
void k_thread_a(void* args);
void k_thread_b(void* args);

int main(void)
{
    put_str("\nKernel 2.0 by Q\n");
    init_all();
    //ASSERT(1==2);
    
    
    thread_start("sub_thread_a",31,k_thread_a,"argA ");
    thread_start("sub_thread_b",8,k_thread_b,"argB ");
    intr_enable();

    while(1){
        put_str("Main ");
    }

    return 0;
}

void k_thread_a(void* args){
    char* para = args;
    while(1)
        put_str(para);
}
void k_thread_b(void* args){
    char* para = args;
    while(1)
        put_str(para);
}