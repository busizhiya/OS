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
#include "../shell/shell.h"


int main(void)
{
    put_str("\nKernel 2.0 by Q\n");
    init_all();
    printf("Finish init_all\n");
    intr_enable();
    clear();
    print_prompt();
    while(1);
    return 0;
}

/*init进程*/
void init(void){
    uint32_t ret_pid = fork();
    if(ret_pid){
        while(1);
    } else {
        my_shell();
    }
    while(1);
}