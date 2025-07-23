#include "../lib/kernel/print.h"
#include "init.h"
#include "debug.h"
#include "memory.h"
#include "../thread/thread.h"
#include "interrupt.h"
#include "../device/console.h"
#include "../device/ioqueue.h"
#include "../device/keyboard.h"
#include "../userprog/process.h"
#include "../lib/user/syscall.h"
#include "../lib/stdio.h"
#include "../fs/fs.h"
#include "../lib/string.h"
#include "../fs/dir.h"

void k_thread_a(void* arg);
void k_thread_b(void* arg);
void u_prog_a(void);
void u_prog_b(void);
int test_var_a = 0, test_var_b = 0;

int main(void)
{
    put_str("\nKernel 2.0 by Q\n");
    init_all();
    //ASSERT(1==2);
    //thread_start("k_thread_a",31,k_thread_a,"argA ");
    //thread_start("k_thread_b",31,k_thread_b,"argB ");
    //process_execute(u_prog_a,"user_prog_a");
    //process_execute(u_prog_b,"user_prog_b");
    //printf("printf: main's pid = %x\n",getpid());
    //printf("I am %s, my pid is %x%c",running_thread()->name,getpid(),'\n');
    struct dir* p_dir = sys_opendir("/dir1");
    if(p_dir){
        printf("/dir1 opened!\n");
        printf("content: \n");
        char* type = NULL;
        struct dir_entry* dir_e = NULL;
        while((dir_e = sys_readdir(p_dir))){
            if(dir_e->f_type == FT_REGULAR){
                type = "regular";
            } else {
                type = "directory";
            }
            printf("\t%s %s\n", type, dir_e->filename);
        }
        printf("try to delete nonempty directory /dir1/subdir1\n");
        if(sys_rmdir("/dir1/subdir1") == -1){
            printf("sys_rmdir: /dir1/subdir1 delete failed\n");
        }

        printf("try to delete file /dir1/subdir1/file2\n");
        if(sys_rmdir("/dir1/subdir1/file2") == -1){
            printf("sys_rmdir: /dir1/subdir1 delete failed\n");
        }
        if(sys_unlink("/dir1/subdir1/file2") == 0){
            printf("sys_unlink: /dir1/subdir1/file2 delete done!\n");
        }

        printf("try to delete empty directory /dir1/subdir1 again\n");
        if(sys_rmdir("/dir1/subdir1") == 0){
            printf("sys_rmdir: /dir1/subdir1 delete done\n");
        }
        printf("dir1 content after delete subdir1\n");
        sys_rewinddir(p_dir);
        while((dir_e = sys_readdir(p_dir))){
            if(dir_e->f_type == FT_REGULAR){
                type = "regular";
            } else {
                type = "directory";
            }
            printf("\t%s %s\n", type, dir_e->filename);
        }
        
        if(sys_closedir(p_dir)==0){
            printf("/dir1/subdir closed!\n");
        }
    }else{
        printf("/dir1/subdir1 open failed\n");
    }

    //intr_enable();
    while(1);
    return 0;
}

void k_thread_a(void* arg){
    char* para = arg;
    void* ptr1 = malloc(1024);
    void* ptr2 = malloc(1024);
    printf("k_thread_a: ptr1, addr is 0x%x\n",(uint32_t)ptr1);
    printf("k_thread_a: ptr2, addr is 0x%x\n",(uint32_t)ptr2);
    free(ptr1);
    void* ptr3 = malloc(1024);
    printf("k_thread_a: ptr3, addr is 0x%x\n",(uint32_t)ptr3);
    void* ptr4 = malloc(1024);
    printf("k_thread_a: ptr4, addr is 0x%x\n",(uint32_t)ptr4);
    while(1){
        //console_put_str("v_a:0x");
        //console_put_int(test_var_a);
    }
}
void k_thread_b(void* arg){
    char* para = arg;
    printf("k_thread_a: sys_malloc(63), addr is 0x%x\n",(uint32_t)sys_malloc(63));
    while(1){
        //console_put_str("v_b:0x"); 
        //console_put_int(test_var_b);
    }
}

void u_prog_a(void){
    while(1){
        D_put_str("u_prog_a running\t");
        //test_var_a++;
    }
}

void u_prog_b(void){
    while(1){
        test_var_b++;
    }
}
