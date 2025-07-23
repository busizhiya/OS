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
    
    printf("/file1 delete %s\n",sys_unlink("/file1") == 0 ? "done" : "failed" );
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
