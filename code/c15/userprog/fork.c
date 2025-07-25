#include "../lib/kernel/stdint.h"
#include "../thread/thread.h"
#include "../lib/string.h"
#include "process.h"
#include "../kernel/debug.h"
#include "../kernel/memory.h"
#include "../fs/file.h"
#include "../kernel/interrupt.h"


extern void intr_exit(void);
extern struct file file_table[MAX_FILE_OPEN];

/*将父进程的pbc拷贝给子进程*/
static int32_t copy_pcb_vaddrbitmap_stack0(struct task_struct* child_thread, struct task_struct* parent_thread){
    /*1. 复制pcb所在的整个页*/
    memcpy(child_thread, parent_thread, PG_SIZE);
    child_thread->pid = fork_pid();
    child_thread->elasped_ticks = 0;
    child_thread->status = TASK_READY;
    child_thread->ticks = child_thread->priority;
    child_thread->parent_pid = parent_thread->pid;
    child_thread->general_tag.prev = child_thread->general_tag.next = NULL;
    child_thread->all_list_tag.prev = child_thread->all_list_tag.next = NULL;
    
    block_desc_init(child_thread->u_block_descs);
    uint32_t bitmap_pg_cnt = DIV_ROUND_UP((0xc0000000 - USER_VADDR_START/PG_SIZE/8), PG_SIZE);
    void* vaddr_btmp = get_kernel_pages(bitmap_pg_cnt);
    //将child_thread的位图换成自己的
    memcpy(vaddr_btmp, child_thread->userprog_vaddr.vaddr_bitmap.bits, bitmap_pg_cnt*PG_SIZE);
    child_thread->userprog_vaddr.vaddr_bitmap.bits = vaddr_btmp;
    ASSERT(strlen(child_thread->name) < 11);    //避免越界
    strcat(child_thread->name,"_fork");
    return 0;
}
/*复制子进程的进程体(代码和数据)和用户栈*/
static void copy_body_stack3(struct task_struct* child_thread, struct task_struct* parent_thread, void* buf_page){
    uint8_t* vaddr_btmp = parent_thread->userprog_vaddr.vaddr_bitmap.bits;
    uint32_t btmp_bytes_len = parent_thread->userprog_vaddr.vaddr_bitmap.btmp_bytes_len;
    uint32_t vaddr_start = parent_thread->userprog_vaddr.vaddr_start;
    uint32_t idx_byte = 0;
    uint32_t idx_bit = 0;
    uint32_t prog_vaddr = 0;
    /*在父进程的用户空间中查找已有数据的页*/
    while(idx_byte < btmp_bytes_len){
        if(vaddr_btmp[idx_byte]){
            idx_bit = 0;
            while(idx_bit < 8){
                if((BITMAP_MASK<<idx_bit) & vaddr_btmp[idx_byte]){
                    prog_vaddr = (idx_byte * 8 + idx_bit) * PG_SIZE + vaddr_start;
                    /*用内核空间做中转,下面才能访问到该数据*/
                    memcpy(buf_page, (void*)prog_vaddr, PG_SIZE);
                    //切换到子进程页表
                    page_dir_activate(child_thread);
                    get_a_page_without_opvaddrbitmap(PF_USER, prog_vaddr);
                    memcpy((void*)prog_vaddr, buf_page, PG_SIZE);
                    page_dir_activate(parent_thread);
                }
                idx_bit++;
            }
        }
        idx_byte++;
    }
}

/*为子进程构建thread_stack和修改返回值*/
static int32_t build_child_stack(struct task_struct* child_thread){
    struct intr_stack* intr_0_stack = (struct intr_stack*)\
    ((uint32_t)child_thread+PG_SIZE - sizeof(struct intr_stack));
    intr_0_stack->eax = 0;
    uint32_t* ret_addr_in_thread_stack = (uint32_t*)intr_0_stack - 1;
    uint32_t* ebp_ptr_in_thread_stack = (uint32_t*)intr_0_stack - 5;
    *ret_addr_in_thread_stack = (uint32_t)intr_exit;
    child_thread->self_kstack = ebp_ptr_in_thread_stack;
    return 0;
}

/*更新inode打开数*/
static void update_inode_open_cnts(struct task_struct* thread){
    int32_t local_fd = 3, global_fd = 0;
    while(local_fd < MAX_FILES_PER_PROC){
        global_fd = thread->fd_table[local_fd];
        ASSERT(global_fd < MAX_FILE_OPEN);
        if(global_fd != -1){
            file_table[global_fd].fd_inode->i_open_cnt++;
        }
        local_fd++;
    }
}

/*拷贝父进程本身所占资源给子进程*/
static int32_t copy_process(struct task_struct* child_thread, struct task_struct* parent_thread){
    void* buf_page = get_kernel_pages(1);
    if(buf_page == NULL){
        return -1;
    }
    //🚩不需要sys_free吗??
    if(copy_pcb_vaddrbitmap_stack0(child_thread, parent_thread) == -1){
        return -1;
    }
    //为子进程创建页表
    child_thread->pgdir = create_page_dir();
    if(child_thread->pgdir == NULL){
        return -1;
    }

    //复制父进程体机用户栈给子进程
    copy_body_stack3(child_thread, parent_thread, buf_page);

    /*构建子进程thread_stack和修改返回值pid*/
    build_child_stack(child_thread);

    /*更新文件inode的打开数*/
    update_inode_open_cnts(child_thread);

    mfree_page(PF_KERNEL, buf_page, 1);
    return 0;
}
/*fork子进程, 内核进程不可直接调用*/

pid_t sys_fork(void){
    struct task_struct* parent_thread = running_thread();
    struct task_struct* child_thread = get_kernel_pages(1);
    if(child_thread == NULL)
        return 0-1;
    ASSERT(INTR_OFF == intr_get_status() && parent_thread->pgdir != NULL);
    if(copy_process(child_thread, parent_thread) == -1){
        return -1;
    }
    aq
}