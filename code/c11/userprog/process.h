#ifndef __USERPROG__PROCESS_H
#define __USERPROG__PROCESS_H
#include "../thread/thread.h"
#define USER_VADDR_START 0x8048000
#define default_prio 31
#define USER_STACK3_VADDR (0xc0000000 - 0x1000)

void process_activate(struct task_struct* p_thread);
void start_process(void* filename_);
void process_execute(void* filename, char* name);

#endif