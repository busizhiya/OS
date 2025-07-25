#include "process.h"
#include "../thread/thread.h"
#include "../lib/kernel/stdint.h"
#include "../kernel/memory.h"
#include "../kernel/debug.h"
#include "tss.h"
#include "../device/console.h"
#include "../lib/string.h"
#include "../kernel/interrupt.h"
#include "../lib/kernel/print.h"


extern void intr_exit(void);
extern struct list thread_ready_list;//就绪队列
extern struct list thread_all_list;//所有任务队列
/*构建用户进程初始上下文信息*/
void start_process(void* filename_){
    D_put_str("start process");
    void* function = filename_;
    struct task_struct* cur = running_thread();
    cur->self_kstack += sizeof(struct thread_stack);
    struct intr_stack* proc_stack = (struct intr_stack*)cur->self_kstack;
    proc_stack->edi = proc_stack->esi = \
    proc_stack->ebp = proc_stack->esp_dummy = 0;

    proc_stack->ebx = proc_stack->edx = \
    proc_stack->ecx = proc_stack->eax = 0;

    proc_stack->gs = 0;
    proc_stack->ds = proc_stack->es = \
    proc_stack->fs = SELECTOR_U_DATA;

    proc_stack->eip = function;
    
    proc_stack->cs = SELECTOR_U_CODE;
    proc_stack->eflags = (EFLAGS_IOPL_0 | EFLAGS_MBS | EFLAGS_IF_1);
    proc_stack->esp = (void*)((uint32_t)get_a_page(PF_USER, \
    USER_STACK3_VADDR) + PG_SIZE);
    proc_stack->ss = SELECTOR_U_DATA;
    asm volatile ("movl %0, %%esp; jmp intr_exit" :: "g"(proc_stack) : "memory");

}

/*激活页表*/
void page_dir_activate(struct task_struct* p_thread){
    /*当前任务可以是线程, 也可以是进程*/
    D_put_str("Now in page_dir_activate\n");
    //内核线程页表地址
    uint32_t pagedir_phy_addr = 0x100000;
    if(p_thread->pgdir != NULL){    //用户态进程有自己的页目录表
        D_put_str("   pgdir!=NULL, thus getting phy addr(v2p)\n");
        pagedir_phy_addr = addr_v2p((uint32_t)p_thread->pgdir);
        D_put_int(pagedir_phy_addr);
        D_put_char('\n');
    }
    //更新页目录寄存器
    asm volatile("movl %0, %%cr3" :: "r"(pagedir_phy_addr):"memory");
}

void process_activate(struct task_struct* p_thread){
    D_put_str("Now in process_activate\n");
    D_put_str(p_thread->name);
    D_put_str(" pgdir=");
    D_put_int((uint32_t)p_thread->pgdir);
    D_put_char('\n');
    D_put_str("name addr:");
    D_put_int((uint32_t)p_thread->name);
    D_put_char('\n');
    
    ASSERT(p_thread!=NULL);
    /*问题定位!!! page_dir_activate*/
    /*切换完页表后, 无法访问原先内核中的数据了*/
    page_dir_activate(p_thread);
    D_put_str("Finish page_dir_activate\n");
    D_put_int((uint32_t)p_thread->name);
    //D_put_str(p_thread->name);
    //D_put_int((uint32_t)p_thread->pgdir);
    //如果是用户态进程,需要更新esp0
    if(p_thread->pgdir){
        D_put_str("Ready to enter update_ess_esp\n");
        update_ess_esp(p_thread);
        D_put_str("Finished update_ess_esp\n");
    }
    D_put_str("At the end of process_activate\n");
}
/*创建用户进程自己的页目录表, 返回页目录表的虚拟地址*/
uint32_t* create_page_dir(void){
    uint32_t* page_dir_vaddr = get_kernel_pages(1);
    if(page_dir_vaddr==NULL){
        console_put_str("create_page_dir: get_kernel_page failed");
        return NULL;
    }
    /*先复制页目录项, 0x300是第768项*/
    /*保证用户进程也能访问到内核空间, 用于系统调用*/
    memcpy((uint32_t*)((uint32_t)page_dir_vaddr+0x300*4), \
    (uint32_t*)(0xfffff000+0x300*4), \
    1024);
    
    /*问题解决: 低端1G也加上*/
    // memcpy((uint32_t*)((uint32_t)page_dir_vaddr), \
    // (uint32_t*)(0xfffff000), \
    // 1024);
    
    /*更新页目录地址*/
    uint32_t new_page_dir_phyaddr = addr_v2p((uint32_t)page_dir_vaddr);
    D_put_str("new_page_dir_phyaddr = ");
    D_put_int(new_page_dir_phyaddr);
    D_put_char('\n');
    /*更新页目录表最后一项为页目录表地址*/
    page_dir_vaddr[1023] = new_page_dir_phyaddr | PG_US_U | PG_RW_W | PG_P_1;
    return page_dir_vaddr;
}

/*为用户进程创建虚拟地址位图*/
void create_user_vaddr_bitmap(struct task_struct* user_prog){
    user_prog->userprog_vaddr.vaddr_start = USER_VADDR_START;
    uint32_t bitmap_pg_cnt = DIV_ROUND_UP((0xc0000000-USER_VADDR_START)/PG_SIZE/8, PG_SIZE);
    user_prog->userprog_vaddr.vaddr_bitmap.bits = \
    get_kernel_pages(bitmap_pg_cnt);
    user_prog->userprog_vaddr.vaddr_bitmap.btmp_bytes_len = \
    (0xc0000000-USER_VADDR_START)/PG_SIZE/8;
    bitmap_init(&user_prog->userprog_vaddr.vaddr_bitmap);
}


void process_execute(void* filename, char* name){
    D_put_str("process_execute start\n");
    struct task_struct* thread = get_kernel_pages(1);
    init_thread(thread, name, default_prio);
    create_user_vaddr_bitmap(thread);
    thread_create(thread, start_process, filename);
    thread->pgdir = create_page_dir();
    block_desc_init(thread->u_block_descs);
    D_put_str("pgdir_vaddr = ");
    D_put_int((uint32_t)thread->pgdir);
    D_put_char('\n');
    enum intr_status old_status = intr_disable();
    ASSERT(!elem_find(&thread_ready_list, &thread->general_tag));
    list_append(&thread_ready_list,&thread->general_tag);
    ASSERT(!elem_find(&thread_all_list,&thread->all_list_tag));
    list_append(&thread_all_list,&thread->all_list_tag);
    intr_set_status(old_status);
    D_put_str("process_execute finished\n");
}
