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
char cwd_cache[MAX_PATH_LEN] = {0};
char final_path[MAX_PATH_LEN];
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


static void cmd_execute(uint32_t argc, char** argv){
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
    else if(!strcmp("ps", argv[0])){
        buildin_ps(argc, argv);
    }
    else {
        int32_t pid = fork();
        if(pid){
            int32_t status;
            int32_t child_pid = wait(&status);
            if(child_pid == -1){
                PANIC("myshell: no child\n");
            }
        } else {
            make_clear_abs_path(argv[0], final_path);
            argv[0] = final_path;
            struct stat file_stat;
            memset(&file_stat, 0, sizeof(struct stat));
            if(stat(argv[0], &file_stat) == -1){
                printf("my_shell: cannot access %s: No such file or directory\n", argv[0]);
            }else {
                execv(argv[0], (const char**)argv);
            }
            exit(0);
        }
    }
    int32_t arg_idx = 0;
    while(arg_idx < MAX_ARG_NR){
        argv[arg_idx] = NULL;
        arg_idx++;
    }
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
        char* pipe_symbol = strchr(cmd_line, '|');
        if(pipe_symbol){
            int32_t fd[2] = {-1};
            pipe(fd);
            //标准输出重定向到fd[1]
            fd_redirect(stdout_no, fd[1]);
            char* each_cmd = cmd_line;
            pipe_symbol = strchr(each_cmd, '|');
            *pipe_symbol = 0;
            argc = -1;
            argc = cmd_parse(cmd_line, argv, ' ');
            if(argc == -1){
                fd_redirect(stdout_no, stdout_no);
                printf("num of arguments exceed %d\n", MAX_ARG_NR);
                continue;
            }
            cmd_execute(argc, argv);
            each_cmd = pipe_symbol + 1;
            fd_redirect(stdin_no, fd[0]);
            while((pipe_symbol = strchr(each_cmd, '|'))){
                *pipe_symbol = 0;
                argc = -1;
                argc = cmd_parse(cmd_line, argv, ' ');
                if(argc == -1){
                    fd_redirect(stdin_no, stdin_no);
                    fd_redirect(stdout_no, stdout_no);
                    printf("num of arguments exceed %d\n", MAX_ARG_NR);
                    continue;
                }
                cmd_execute(argc, argv);
                each_cmd = pipe_symbol + 1;
            }
            fd_redirect(stdout_no, stdout_no);
            argc = -1;
            argc = cmd_parse(each_cmd, argv, ' ');
            if(argc == -1){
                fd_redirect(stdin_no, stdin_no);
                printf("num of arguments exceed %d\n", MAX_ARG_NR);
                continue;
            }
            cmd_execute(argc, argv);
            fd_redirect(stdin_no, stdin_no);
            close(fd[0]);
            close(fd[1]);
        } else {
            argc = -1;
            argc = cmd_parse(cmd_line, argv, ' ');
            if(argc == -1){
                printf("num of arguments exceed %d\n", MAX_ARG_NR);
                continue;
            }
            cmd_execute(argc, argv);
        }
    }
    PANIC("my_shell: Unexpected error\n");
}
