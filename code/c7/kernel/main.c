#include "../lib/kernel/print.h"
#include "init.h"

int main(void)
{
    put_str("\nKernel 2.0 by Q\n");
    init_all();
    put_str("ready to start interuption\n");
    asm volatile("sti");

    while(1) ;

    return 0;
}
