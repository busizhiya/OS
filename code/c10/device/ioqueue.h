#ifndef __DEVICE_IOQUEUE_H
#define __DEVICE_IOQUEUE_H

#include "../lib/kernel/stdint.h"
#include "../thread/thread.h"
#include "../thread/sync.h"

#define bufsize 64

//生产者
struct ioqueue{
    struct lock lock;
    struct task_struct* producer;
    struct task_struct* consumer;
    char buf[bufsize];
    int32_t head;
    int32_t tail;
};

void ioq_putchar(struct ioqueue* ioq, char byte);
char ioq_getchar(struct ioqueue* ioq);
static void wakeup(struct task_struct** waiter);
static void ioq_wait(struct task_struct** waiter);
bool ioq_empty(struct ioqueue* ioq);
bool ioq_full(struct ioqueue* ioq);
void ioqueue_init(struct ioqueue* ioq);
static int32_t next_pos(int32_t pos);

#endif