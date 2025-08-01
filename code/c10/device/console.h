#ifndef __DEVICE_CONSOLE_H
#define __DEVICE_CONSOLE_H
#include "../lib/kernel/stdint.h"

void console_init();
void console_put_str(char* str);
void console_put_char(uint8_t char_ascii);
void console_put_int(uint32_t num);
#endif