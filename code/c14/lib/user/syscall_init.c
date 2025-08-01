#include "syscall.h"
#include "../kernel/stdint.h"
#include "../../thread/thread.h"
#include "../kernel/print.h"
#include "../../device/console.h"
#include "../string.h"
#include "../../fs/fs.h"

#define syscall_nr 32
typedef void* syscall;
syscall syscall_table[syscall_nr];

uint32_t sys_getpid(void){
    return running_thread()->pid;

}

/*尚未实现文件系统*/
// uint32_t sys_write(char* str){
//     console_put_str(str);
//     return strlen(str);
// }

void syscall_init(void){
    put_str("syscall_init start\n");
    syscall_table[SYS_GETPID] = sys_getpid;
    syscall_table[SYS_WRITE] = sys_write;
    syscall_table[SYS_MALLOC] = sys_malloc;
    syscall_table[SYS_FREE] = sys_free;
    put_str("syscall_init done\n");
}
