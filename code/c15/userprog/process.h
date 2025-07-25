#ifndef __USERPROG__PROCESS_H
#define __USERPROG__PROCESS_H
#include "../thread/thread.h"
#define USER_VADDR_START 0x8048000
#define default_prio 31

void process_activate(struct task_struct* p_thread);
void start_process(void* filename_);
void process_execute(void* filename, char* name);
void page_dir_activate(struct task_struct* p_thread);
uint32_t* create_page_dir(void);

#endif