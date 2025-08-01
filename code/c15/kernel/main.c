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
#include "../device/ide.h"

int main(void)
{
    put_str("\nKernel 2.0 by Q\n");
    init_all();
    printf("Finish init_all\n");

    uint32_t fd = sys_open("/file1",O_CREAT|O_RDWR);
    if(fd != -1){
        sys_write(fd, "Hello world", 12);
        sys_close(fd);
    }

    
    fd = sys_open("/file2",O_CREAT|O_RDWR);
    if(fd != -1){
        sys_write(fd, "file1", 6);
        sys_close(fd);
    }

    uint32_t file_size = 11060;
    uint32_t sec_cnt = DIV_ROUND_UP(file_size, 512);
    struct disk* sda = &channels[0].devices[0];
    void* prog_buf = sys_malloc(sec_cnt*PG_SIZE);
    ide_read(sda, 300, prog_buf, sec_cnt);
    int32_t fd2= sys_open("/cat", O_CREAT | O_RDWR);
    if(fd2 != -1){
        if(sys_write(fd2, prog_buf, file_size) == -1){
            printf("file write error\n");
            while(1);
        }
        sys_close(fd2);
    }
    sys_free(prog_buf);
    
    intr_enable();
    //clear();
    print_prompt();
    thread_exit(running_thread(), true);
    return 0;
}

