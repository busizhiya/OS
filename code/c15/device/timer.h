#ifndef __DEVICE__TIMER_H
#define __DEVICE__TIMER_H
#include "../lib/kernel/stdint.h"

void timer_init(void);
void mtime_sleep(uint32_t m_seconds);
static void ticks_to_sleep(uint32_t sleep_ticks);
#endif