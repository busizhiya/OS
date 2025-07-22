#ifndef __FS_FILE_H
#define __FS_FILE_H
#include "../lib/kernel/stdint.h"
#include "../device/ide.h"
#include "dir.h"

struct file{
    uint32_t fd_pos;    //记录当前文件操作的偏移地址
    uint32_t fd_flag;   //文件操作标识
    struct inode* fd_inode; //指向inode队列中的inode
};
enum std_fd{
    stdin_no,   //0标准输入
    stdout_no,  //1标准输出
    stderr_no   //2标准错误
}; 
enum bitmap_type{
    INODE_BITMAP,   
    BLOCK_BITMAP
};
#define MAX_FILE_OPEN 32

int32_t block_bitmap_alloc(struct partition* part);
int32_t inode_bitmap_alloc(struct partition* part);
void bitmap_sync(struct partition* part, uint32_t bit_idx, uint8_t btmp);
int32_t pcb_fd_install(int32_t global_fd_idx);
int32_t get_free_slot_in_global(void);
int32_t file_create(struct dir* parent_dir, char* filename, uint8_t flag);

#endif