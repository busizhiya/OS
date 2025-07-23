#ifndef __FS_FS_H
#define __FS_FS_H
#include "../lib/kernel/stdint.h"
#include "../lib/kernel/list.h"

#define MAX_FILES_PER_PART  4096 //每个分区最大文件数
#define MAX_PATH_LEN 512    //路径最大长度

#define BITS_PER_SECTOR 4096
#define SECTOR_SIZE 512
#define BLOCK_SIZE SECTOR_SIZE

enum file_types{
    FT_UNKNOWN,
    FT_REGULAR,
    FT_DIRECTORY
};
enum oflags{    //按位定义
    O_RDONLY = 0,   //只读
    O_WRONLY = 1,   //只写
    O_RDWR = 2,     //读写
    O_CREAT = 4 //创建
};
enum whence{    //参照
    SEEK_SET = 1,   //文件开头
    SEEK_CUR,       //当前位置
    SEEK_END        //文件末尾
};
struct path_search_record{
    char searched_path[MAX_PATH_LEN];   //查找过程中的父路径
    struct dir* parent_dir; //直接父目录
    enum file_types file_type;  //文件类型
};

void filesys_init();
static bool mount_partition(struct list_elem* pelem, int arg);
static int search_file(const char* pathname, struct path_search_record* searched_record);
int32_t path_depth_cnt(char* pathname);
static char* path_parse(char* pathname, char* name_store);
int32_t sys_open(const char* pathname, uint8_t flag);
int32_t sys_close(int32_t fd);
int32_t sys_write(int32_t fd,const void* buf, uint32_t count);
int32_t sys_read(int32_t fd, void* buf, uint32_t count);
int32_t sys_lseek(int32_t fd, int32_t offset, uint8_t whence);
int32_t sys_unlink(const char* pathname);

#endif