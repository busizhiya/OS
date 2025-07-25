#ifndef __FS_SUPER_BLOCK_H
#define __FS_SUPER_BLOCK_H
#include "../lib/kernel/stdint.h"

struct super_block{
    uint32_t magic;
    uint32_t sec_cnt;
    uint32_t inode_cnt;     
    uint32_t part_lba_base; //本分区起始lba地址

    uint32_t block_bitmap_lba;
    uint32_t block_bitmap_sects;    //扇区位图本身占用的扇区数量
    
    uint32_t inode_bitmap_lba;
    uint32_t inode_bitmap_sects;

    uint32_t inode_table_lba;
    uint32_t inode_table_sects;

    uint32_t data_start_lba;    //数据区开始的第一个扇区号
    uint32_t root_inode_no;     //根目录所在的inode号
    uint32_t dir_entry_size;    //目录项大小

    uint8_t pad[460];
}__attribute__((packed));


#endif