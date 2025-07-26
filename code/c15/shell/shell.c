#include "shell.h"
#include "../lib/kernel/stdint.h"
#include "../lib/stdio.h"
#include "../kernel/debug.h"
#include "../lib/user/syscall.h"
#include "../lib/string.h"
#include "buildin_cmd.h"

#define cmd_len 128
#define MAX_ARG_NR 16

/*存储输入的命令*/
static char cmd_line[cmd_len] = {0};

/*当前目录的缓存*/
char cwd_cache[64] = {0};
char final_path[MAX_PATH_LEN] = {0};
char* argv[MAX_ARG_NR];
int32_t argc = -1;


void print_prompt(void){
    printf("[qiao@localhost %s]$", cwd_cache);
}

/*从键盘缓冲区最多读入count个字节到buf*/
static void readline(char* buf, int32_t count){
    ASSERT(buf != NULL && count > 0);
    char* pos = buf;
    while(read(stdin_no, pos, 1) != -1 && (pos - buf) < count){
        switch(*pos){
            case '\n':
            case '\r':
                *pos = 0;   //cmd_line的终止字符0
                putchar('\n');
                return;
            case '\b':
                //不能删除非本次输入的信息
                if(buf[0] != '\b'){
                    --pos;
                    putchar('\b');
                }
                break;
            case '\t':
                break;   //暂不支持tab
            case 'l' - 'a':
                *pos = 0;
                clear();
                print_prompt();
                printf("%s",buf);
                break;
            case 'u' - 'a':
                while(buf != pos){
                    putchar('\b');
                    *(pos--) = 0;
                }
                break;
            default:
                putchar(*pos);
                pos++;
        }
    }
    printf("readline: can't find enter_key in cmd_line, max num of char is 128\n");
}

static int32_t cmd_parse(char* cmd_str, char** argv, char token){
    ASSERT(cmd_str != NULL);
    int32_t arg_idx = 0;
    while(arg_idx < MAX_ARG_NR){
        argv[arg_idx] = NULL;
        arg_idx++;
    }
    char* next = cmd_str;
    int32_t argc = 0;
    while(*next){
        //去除空格
        while(*next == token)
            next++;
        if(*next == 0)
            break;

        argv[argc] = next;

        while(*next && *next != token)
            next++;

        if(*next)
            *next++ = 0;    //将token替换为结束符0
        
        if(argc > MAX_ARG_NR)
            return -1;
        
        argc++;        
    }
    return argc;    //返回参数个数
}



void my_shell(void){
    cwd_cache[0] = '/';
    cwd_cache[1] = 0;

    while(1){
        print_prompt();
        memset(final_path, 0, MAX_PATH_LEN);
        memset(cmd_line, 0, cmd_len);
        readline(cmd_line, cmd_len);
        if(cmd_line[0] == 0){
            continue;   //输入了一个回车
        }
        argc = -1;
        argc = cmd_parse(cmd_line, argv, ' ');
        if(argc == -1){
            printf("num of arguments exceed %d\n", MAX_ARG_NR);
            continue;
        }

        if(!strcmp("ls", argv[0])){
            buildin_ls(argc, argv);
        }
        else if(!strcmp("cd", argv[0])){
            if(buildin_cd(argc, argv) != NULL){
                memset(cwd_cache, 0, MAX_PATH_LEN);
                strcpy(cwd_cache, final_path);
            }
        }
        else if(!strcmp("pwd", argv[0])){
            buildin_pwd(argc, argv);
        }
        else if(!strcmp("clear", argv[0])){
            buildin_clear(argc, argv);
        }
        else if(!strcmp("mkdir", argv[0])){
            buildin_mkdir(argc, argv);
        }
        else if(!strcmp("rmdir", argv[0])){
            buildin_rmdir(argc, argv);
        }
        else if(!strcmp("rm", argv[0])){
            buildin_rm(argc, argv);
        }
        else {
            printf("external command\n");
        }
    }
    PANIC("my_shell: Unexpected error\n");

}
