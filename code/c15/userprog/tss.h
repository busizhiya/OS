#ifndef __USERPROG_TSS_H
#define __USERPROG_TSS_H
#include "../thread/thread.h"

void tss_init();
void update_ess_esp(struct task_struct* pthread);

#endif