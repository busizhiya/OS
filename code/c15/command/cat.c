#include "../lib/user/syscall.h"
#include "../lib/stdio.h"
#include "../lib/string.h"

int main(int argc, char **argv){
    if(argc > 2 || argc == 1){
        printf("cat only support one argument\n");
        exit(-2);
    }
    int buf_size = 1024;
    char abs_path[512];
    memset(abs_path, 0, 512);
    void* buf = malloc(buf_size);
    if(buf == NULL){
        printf("cat: malloc failed\n");
        return -1;
    }
    if(argv[1][0] != '/'){
        getcwd(abs_path, 512);
        if(strcmp(abs_path, "/"))
            strcat(abs_path, "/");
        strcat(abs_path, argv[1]);
    }else{
        strcpy(abs_path, argv[1]);
    }
    int fd = open(abs_path, O_RDONLY);
    if(fd == -1){
        printf("cat: open: open %s failed\n", argv[1]);
        return -1;
    }
    int read_bytes = 0;
    while(1){
        read_bytes = read(fd, buf, buf_size);
        if(read_bytes == -1)
            break;
        write(stdout_no, buf, read_bytes);
    }
    free(buf);
    close(fd);
    return 66;
}
