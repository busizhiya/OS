#ifndef __THREAD_THREAD_H
#define __THREAD_THREAD_H
#include "../lib/kernel/stdint.h"
#include "../lib/kernel/list.h"
#include "../kernel/memory.h"
#include "../lib/kernel/bitmap.h"

typedef void thread_func(void*);
typedef int16_t pid_t;
enum task_status{
    TASK_RUNNING,
    TASK_READY,
    TASK_BLOCKED,
    TASK_WAITING,
    TASK_HANGING,
    TASK_DIED
};

struct intr_stack{
    uint32_t vec_no;    //push %1

    uint32_t edi;       //pushad;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp_dummy;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;

    uint32_t gs;
    uint32_t fs;
    uint32_t es;
    uint32_t ds;

    /*cpu从低特权级进入高特权级压入*/
    uint32_t err_code;
    void (*eip) (void);
    uint32_t cs;
    uint32_t eflags;
    void* esp;
    uint32_t ss;
};  //实际上不一定有下面内容, 故数据与内存使用不一定刚好对应

/*用于switch_to时保存线程环境*/
struct thread_stack{
    //self_kstack地址存储的数据结构
    //只是按照如下顺序存储
    uint32_t ebp;
    uint32_t ebx;
    uint32_t edi;
    uint32_t esi;
    /*第一次执行时, eip指向kernel_thread, 其他时候指向switch_to返回地址*/
    void (*eip) (thread_func* func, void* func_arg);
    void (*unused_retaddr); //占位
    thread_func* function;
    void* func_arg;
};
//PCB页内数据
struct task_struct{
    uint32_t* self_kstack;  //指向栈顶
    pid_t pid;
    enum task_status status;
    uint8_t priority;
    char name[16];
    uint8_t ticks;
    uint32_t elasped_ticks;
    struct list_elem general_tag;
    struct list_elem all_list_tag;
    uint32_t* pgdir;
    struct virtual_addr userprog_vaddr;
    struct mem_block_desc u_block_descs[DESC_CNT];
    uint32_t stack_magic;   //栈边界,判断溢出

};

struct task_struct* thread_start(char* name, int prio, thread_func function, void* func_arg);
static void make_main_thread(void);
struct task_struct* running_thread(void);
void schedule(void);
void thread_init(void);
void init_thread(struct task_struct* pthread, char* name, int prio);
void thread_unblock(struct task_struct* pthread);
void thread_block(enum task_status stat);
void thread_create(struct task_struct* pthread, thread_func function, void* func_arg);
void thread_yield(void);
#endif