#include "../lib/stdio.h"
#include "../lib/user/syscall.h"
#include "../lib/string.h"

int main(int argc, char** argv){
    int32_t fd[2];
    memset(fd, -1, 2);
    pipe(fd);
    int32_t pid = fork();
    if(pid){
        //父进程
        close(fd[0]);//关闭输入
        write(fd[1], "Hi, my son!",12);
        printf("\nI'm father, my pid is %d\n", getpid());
        return 8;
    } else {
        close(fd[1]);
        char buf[32];
        memset(buf, 0, 32);
        read(fd[0],buf, 12);
        printf("\nI'm child, my pid is %d\n",getpid());
        printf("I'm child, my father said: \"%s\"", buf);
        return 9;
    }
}