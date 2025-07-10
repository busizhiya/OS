#include "../lib/kernel/print.h"


int main(void)
{
    char* msg = "\nHello world!1\b3\nkernel version 1.1 by Q\n";
    put_str(msg);
    put_int(0x13145678);
    put_char('\n');
    put_int(0x123);
    put_char('\n');
    put_int(0);
    while(1) ;

    return 0;
}
