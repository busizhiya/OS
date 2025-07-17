#ifndef __LIB_USER_SYSCALL_H
#define __LIB_USER_SYSCALL_H
#include "../kernel/stdint.h"
enum SYSCALL_NR{
    SYS_GETPID
};
void syscall_init(void);

uint32_t getpid(void);
#endif
