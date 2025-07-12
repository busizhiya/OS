#include "thread.h"
#include "../lib/kernel/stdint.h"
#include "../lib/string.h"
#include "../kernel/global.h"
#include "../kernel/memory.h"

#define PG_SIZE 4096

static void kernel_thread(thread_func* function, void* func_arg){
    function(func_arg);
}
/*初始化thread_stack*/
void thread_create(struct task_struct* pthread, thread_func function, void* func_arg){
    //预留中断栈的空间
    pthread->self_kstack -= sizeof(struct intr_stack);
    //预留线程栈空间
    pthread->self_kstack -= sizeof(struct thread_stack);

    struct thread_stack* kthread_stack = (struct thread_stack*)pthread->self_kstack;
    kthread_stack->eip = kernel_thread;
    kthread_stack->function = function;
    kthread_stack->func_arg = func_arg;
    //全部为0
    kthread_stack->ebp = kthread_stack->ebp = \
    kthread_stack->esi = kthread_stack->edi = 0;
}
/*初始化线程基本信息*/
void init_thread(struct task_struct* pthread, char* name, int prio){
    memset(pthread, 0, sizeof(*pthread));
    strcpy(pthread->name, name);
    pthread->status = TASK_RUNNING;
    pthread->priority = prio;

    pthread->self_kstack = (uint32_t*)((uint32_t)pthread + PG_SIZE);
    pthread->stack_magic = 0x20070515;
}

struct task_struct* thread_start(char* name, int prio, thread_func function, void* func_arg){
    //申请一页空间给pcb
    struct task_struct* thread = get_kernel_pages(1);
    init_thread(thread, name, prio);
    thread_create(thread, function, func_arg);

    asm volatile("movl %0, %%esp; \
        pop %%ebp; pop %%ebx; pop %%edi; pop %%esi; \
        ret" : : "g"(thread->self_kstack) : "memory");
    return thread;
}

