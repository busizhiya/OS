#ifndef __KERNEL_DEBUG_H
#define __KERNEL_DEBUG_H

//#define D_PRINT

void panic_spin(char* filename, int line, const char* func, const char* condition);

#define PANIC(...) panic_spin(__FILE__, __LINE__, __func__, __VA_ARGS__)
#ifdef NDEBUG
    #define ASSERT(CONDITION) ((void)0)
#else
#define ASSERT(CONDITION) \
if(CONDITION) {} else{ \
    PANIC(#CONDITION); \
}
#ifdef D_PRINT
#define D_put_str(STR) put_str(STR);
#define D_put_int(INT) put_int(INT);
#define D_put_char(CHR) put_char(CHR);
#else
#define D_put_str(STR) ((void)0)
#define D_put_int(INT) ((void)0)
#define D_put_char(CHR) ((void)0)
#endif
#endif /*NDEBUG*/
#endif /*__KERNEL_DEBUG_H*/
