#ifndef __THREAD_SYNC_H
#define __THREAD_SYNC_H
#include "thread.h"
#include "../lib/kernel/list.h"
#include "../lib/kernel/stdint.h"

struct semaphore {
    uint8_t value;
    struct list waiters;
};

struct lock{
    struct task_struct* holder;
    struct semaphore semaphore;//用二元信号量实现锁
    uint32_t holder_repeat_nr;  //所的持有者重复申请所的次数
};
void lock_release(struct lock* plock);
void lock_acquire(struct lock* plock);
void lock_init(struct lock* plock);
void sema_init(struct semaphore* psema, uint8_t value);
void sema_down(struct semaphore* psema);
void sema_up(struct semaphore* psema);

#endif