#ifndef __USERPROG_EXEC_H
#define __USERPROG_EXEC_H
#include "../lib/kernel/stdint.h"
int32_t sys_execv(const char* path, const char* argv[]);

#endif