#include "../lib/kernel/print.h"
#include "init.h"
#include "debug.h"

int main(void)
{
    put_str("\nKernel 2.0 by Q\n");
    init_all();
    put_str("ready to start interuption\n");
    //asm volatile("sti");
    //ASSERT(1==2);
    while(1) ;

    return 0;
}
