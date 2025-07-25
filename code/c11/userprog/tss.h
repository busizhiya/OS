#ifndef __USERPORG_TSS_H
#define __USERPORG_TSS_H
#include "../thread/thread.h"

void tss_init();
void update_tss_esp(struct task_struct* pthread);

#endif