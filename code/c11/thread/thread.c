#include "thread.h"
#include "../lib/kernel/stdint.h"
#include "../lib/string.h"
#include "../kernel/global.h"
#include "../kernel/memory.h"
#include "../kernel/debug.h"
#include "../lib/kernel/list.h"
#include "../kernel/interrupt.h"
#include "../lib/kernel/print.h"
#include "../userprog/process.h"
#include "../device/console.h"
#define PG_SIZE 4096

struct task_struct* main_thread;//主线程PCB
struct list thread_ready_list;//就绪队列
struct list thread_all_list;//所有任务队列
static struct list_elem* thread_tag;    //保存队列中的线程节点

extern void switch_to(struct task_struct* cur, struct task_struct* next);
//返回PCB页起始地址
struct task_struct* running_thread(){
    uint32_t esp;
    asm("mov %%esp, %0" : "=g"(esp));
    //取esp整数部分, 即pcb起始地址(某个页)
    return (struct task_struct*)(esp & 0xfffff000);
}
static void kernel_thread(thread_func* function, void* func_arg){
    intr_enable();
    function(func_arg);
}

/*初始化thread_stack*/
void thread_create(struct task_struct* pthread, thread_func function, void* func_arg){
    //预留中断栈的空间
    pthread->self_kstack -= sizeof(struct intr_stack);
    //预留线程栈空间
    pthread->self_kstack -= sizeof(struct thread_stack);

    struct thread_stack* kthread_stack = (struct thread_stack*)pthread->self_kstack;
    //->等同于加偏移量
    //下面代码将线程栈的空间进行填充初始化
    kthread_stack->eip = kernel_thread;
    kthread_stack->function = function;
    kthread_stack->func_arg = func_arg;
    //全部为0
    kthread_stack->ebp = kthread_stack->ebx = \
    kthread_stack->esi = kthread_stack->edi = 0;
}
/*初始化线程基本信息*/
void init_thread(struct task_struct* pthread, char* name, int prio){
    memset(pthread, 0, sizeof(*pthread));
    strcpy(pthread->name, name);
    if(pthread == main_thread){
        pthread->status = TASK_RUNNING;
    }else{
        pthread->status = TASK_READY;
    }
    pthread->priority = prio;
    pthread->ticks = prio;
    pthread->elapsed_ticks = 0;
    pthread->pgdir = NULL;
    pthread->self_kstack = (uint32_t*)((uint32_t)pthread + PG_SIZE);
    pthread->stack_magic = 0x20070515;
}

struct task_struct* thread_start(char* name, int prio, thread_func function, void* func_arg){
    //申请一页空间给pcb

    struct task_struct* thread = get_kernel_pages(1);
    init_thread(thread, name, prio);
    thread_create(thread, function, func_arg);
    //确保之前不在队列中
    ASSERT(!elem_find(&thread_ready_list,&thread->general_tag));
    list_append(&thread_ready_list,&thread->general_tag);
    ASSERT(!elem_find(&thread_all_list,&thread->all_list_tag));
    list_append(&thread_all_list,&thread->all_list_tag);
    
    return thread;
}

static void make_main_thread(void){
    /*main线程的pcb已经预留在0xc009e000, 不需要额外申请一页内存*/
    main_thread = running_thread();
    init_thread(main_thread, "main", 3);
    ASSERT(!elem_find(&thread_all_list, &main_thread->all_list_tag));
    list_append(&thread_all_list,&main_thread->all_list_tag);
}


void schedule(){

    ASSERT(intr_get_status() == INTR_OFF);
    
    
    struct task_struct* cur = running_thread();
    D_put_str("Schedule: ");
    D_put_str(cur->name);
    D_put_char('\n');
    if(cur->status == TASK_RUNNING){
        //时间片用完了
        ASSERT(!elem_find(&thread_ready_list, &cur->general_tag));
        list_append(&thread_ready_list, &cur->general_tag);
        cur->ticks = cur->priority;
        cur->status = TASK_READY;
    }else{
        //需要某些事件发生后才能上cpu
        //未就绪
    }
    //尚未实现idle线程,有可能线程池为空
    ASSERT(!list_empty(&thread_ready_list));

    thread_tag = NULL;
    thread_tag = list_pop(&thread_ready_list);
    struct task_struct* next = elem2entry(struct task_struct, general_tag, thread_tag);
    next->status = TASK_RUNNING;
    process_activate(next);
    D_put_str("Schedule: ready to enter switch_to\n");
    D_put_char('\n');
    switch_to(cur, next);   //0xc0004417, switch_to: 0xc0004000
   
}
void thread_init(void){
    put_str("thread_init start\n");
    list_init(&thread_ready_list);
    list_init(&thread_all_list);
    //为当前main函数创建线程
    make_main_thread();
    put_str("thread_init done\n");
}

/*当前线程主动将自己阻塞*/
void thread_block(enum task_status stat){
    ASSERT(((stat == TASK_BLOCKED) || (stat == TASK_WAITING) || (stat == TASK_HANGING)));
    enum intr_status old_status = intr_disable();
    struct task_struct* cur_thread = running_thread();
    cur_thread->status = stat;
    schedule(); //将当前线程换下处理器
    intr_set_status(old_status);    //已被唤醒, 恢复之前中断状态 
}
/*将线程pthread唤醒(解除阻塞)*/
void thread_unblock(struct task_struct* pthread){
    enum intr_status old_status = intr_disable();
    enum task_status stat = pthread->status;
    ASSERT(((stat == TASK_BLOCKED) || (stat == TASK_WAITING) || (stat == TASK_HANGING)));
    if(stat != TASK_READY){
        ASSERT(!elem_find(&thread_ready_list,&pthread->general_tag));
        if(elem_find(&thread_ready_list,&pthread->general_tag))
        {
            PANIC("thread_unblock: blocked thread in ready_list");
        }
        //自动置于等待队列最前端
        list_push(&thread_ready_list,&pthread->general_tag);
        pthread->status = TASK_READY;
    }  
    intr_set_status(old_status);
}