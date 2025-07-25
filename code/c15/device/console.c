#include "../lib/kernel/stdint.h"
#include "../thread/sync.h"
#include "../thread/thread.h"
#include "../lib/kernel/print.h"

static struct lock console_lock;

/*初始化终端*/
void console_init(){
    lock_init(&console_lock);
}

/*获取终端锁*/
void console_acquire(){
    lock_acquire(&console_lock);
}

/*释放终端锁*/
void console_release(){
    lock_release(&console_lock);
}

/*终端中输出字符串*/
void console_put_str(char* str){
    console_acquire();
    put_str(str);
    console_release();
}

void console_put_char(uint8_t char_ascii){
    console_acquire();
    put_char(char_ascii);
    console_release();
}
void console_put_int(uint32_t num){
    console_acquire();
    put_int(num);
    console_release();
}

