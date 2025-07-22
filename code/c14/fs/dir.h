#ifndef __FS_DIR_H
#define __FS_DIR_H
#include "../lib/kernel/stdint.h"
#include "inode.h"
#include "fs.h"
#include "../device/ide.h"

#define MAX_FILE_NAME_LEN 16

//并不在磁盘上存在, 仅在内存中创建使用
struct dir{
    struct inode* inode;
    uint32_t dir_pos;   //记录在目录内的偏移
    uint8_t dir_buf[512];   //目录的数据缓存
};

//目录项结构, 可能指向目录文件或普通文件
struct dir_entry{
    char filename[MAX_FILE_NAME_LEN];
    uint32_t i_no;
    enum file_types f_type;
};
bool sync_dir_entry(struct dir* parent_dir, struct dir_entry* p_de, void* io_buf);
void create_dir_entry(char* filename, uint32_t inode_no, uint8_t file_type, struct dir_entry* p_de);
void dir_close(struct dir* dir);
bool search_dir_entry(struct partition* part, struct dir* pdir, const char* name, struct dir_entry* dir_e);
struct dir* dir_open(struct partition* part, uint32_t inode_no);
void open_root_dir(struct partition* part);


#endif