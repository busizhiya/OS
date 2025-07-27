#include "../lib/kernel/print.h"
#include "init.h"
#include "interrupt.h"
#include "../device/timer.h"
#include "memory.h"
#include "../thread/thread.h"
#include "../device/console.h"
#include "../device/keyboard.h"
#include "../userprog/tss.h"
#include "../lib/user/syscall.h"
#include "../device/ide.h"
#include "../fs/fs.h"
#include "../shell/shell.h"
#include "../lib/stdio.h"

void init_all()
{
    put_str("init_all\n");
    idt_init();
    timer_init();
    mem_init();
    thread_init();
    console_init();
    keyboard_init();
    tss_init();
    syscall_init();
    ide_init();
    filesys_init();
}

/*init进程*/
void init(void){
    uint32_t ret_pid = fork();
    if(ret_pid){
        int status;
        int child_pid;
        while(1){
            child_pid = wait(&status);
            printf("I'm pid, I recieve a child, whose pid is %d, status is %d\n" ,child_pid,status);
        }
    } else {
        my_shell();
    }
}