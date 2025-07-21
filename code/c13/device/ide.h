#ifndef __DEVICE_IDE_H
#define __DEVICE_IDE_H
#include "../lib/kernel/stdint.h"
#include "../lib/kernel/list.h"
#include "../lib/kernel/bitmap.h"
#include "../thread/sync.h"

struct partition{
    uint32_t start_lba; //起始扇区
    uint32_t sec_cnt;   //扇区数
    struct disk* my_disk;   //分区所属的硬盘
    struct list_elem part_tag;
    char name[8];
    struct super_block* sb;
    struct bitmap block_bitmap;
    struct bitmap inode_bitmap;
    struct list open_inodes;
};

struct disk{
    char name[8];
    struct ide_channel* my_channel;
    uint8_t dev_no;
    struct partition prim_parts[4];
    struct partition logic_parts[8];
};

struct ide_channel{
    char name[8];
    uint16_t port_base;
    uint8_t irq_no;
    struct lock lock;
    bool expecting_intr;
    struct semaphore disk_done;
    struct disk devices[2];
};
void ide_init();



#endif
