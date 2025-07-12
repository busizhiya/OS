#include "../lib/kernel/print.h"
#include "init.h"
#include "debug.h"
#include "memory.h"
#include "../thread/thread.h"
void k_thread_a(void* args);
int main(void)
{
    put_str("\nKernel 2.0 by Q\n");
    init_all();
    //asm volatile("sti");
    //ASSERT(1==2);
    thread_start("sub_thread",1,k_thread_a,"argA ");
    while(1) ;

    return 0;
}

void k_thread_a(void* args){
    char* para = args;
    put_str(para);
}