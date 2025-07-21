#ifndef __FS_INODE_H
#define __FS_INODE_H
#include "../lib/kernel/stdint.h"
#include "../lib/kernel/list.h"

struct inode{
    uint32_t i_no;  //inode编号
    uint32_t i_size;    ///*若inode为文件, i_size指文件大小; 若inode为目录, i_size指目录项大小之和*/
    
    uint32_t i_open_cnt;    //此文件被打开的次数
    bool write_deny;    //写文件不能并行
    
    uint32_t i_sectors[13]; //0~11是直接块, 12存储一级间接块指针
    struct list_elem inode_tag;
};

#endif