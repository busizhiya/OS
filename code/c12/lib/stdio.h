#ifndef __LIB_STDIO_H
#define __LIB_STDIO_H
#include "kernel/stdint.h"

typedef char* va_list;
uint32_t printf(const char* format, ...);
#endif