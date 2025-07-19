#include "stdio.h"
#include "kernel/stdint.h"
#include "string.h"
#include "user/syscall.h"

#define va_start(ap, v) ap=(va_list)&v  //ap指向第一个固定参数v
#define va_arg(ap, t) *((t*)(ap += 4))  //ap指向下一个参数并返回其值
#define va_end(ap) ap = NULL

/*integer to ascii, 可指定进制*/
static void itoa(uint32_t value, char** buf_ptr_addr, uint8_t base){
    uint32_t m = value % base;
    uint32_t i = value / base;
    if(i){
        itoa(i, buf_ptr_addr, base);
    }
    if(m<10){
        *((*buf_ptr_addr)++) = m + '0';
    }else{
        *((*buf_ptr_addr)++) = m - 10 + 'A';
    }
}
/*将ap按照format格式输出到字符串str, 返回替换后的str长度*/
uint32_t vsprintf(char* str, const char* format, va_list ap){
    char* buf_ptr = str;
    const char* index_ptr = format;
    char index_char = *index_ptr;
    int32_t arg_int;
    char* arg_str;
    while(index_char){
        if(index_char != '%'){
            *(buf_ptr++) = index_char;
            index_char = *(++index_ptr);
            continue;
        }
        index_char = *(++index_ptr);
        switch(index_char){
            case 'x':   //hex
                arg_int = va_arg(ap, int);
                itoa(arg_int, &buf_ptr, 16);
                index_char = *(++index_ptr);
                break;
            case 's':   //string
                arg_str = va_arg(ap, char*);
                strcpy(buf_ptr, arg_str);
                buf_ptr += strlen(arg_str);
                index_char = *(++index_ptr);
                break;
            case 'c':   //char
                *(buf_ptr++) = va_arg(ap, char);
                index_char = *(++index_ptr);
                break;
            case 'd':   //decimal
                arg_int = va_arg(ap, int);
                if(arg_int<0){
                    arg_int = -arg_int;
                    *buf_ptr++ = '-';
                }
                itoa(arg_int,&buf_ptr, 10);
                index_char = *(++index_ptr);
                break;
        }
    }
    return strlen(str);
}

/*格式化输出字符串*/
uint32_t printf(const char* format, ...){
    va_list args;
    va_start(args, format);
    char buf[1024] = {0};
    vsprintf(buf,format,args);
    va_end(args);
    return write(buf);
}

/*把字符串格式化写入buf中*/
uint32_t sprintf(char* buf, const char* format, ...){
    va_list args;
    va_start(args, format);
    uint32_t retval = vsprintf(buf,format,args);
    va_end(args);
    return retval;
}