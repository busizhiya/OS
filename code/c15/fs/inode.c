#include "../lib/kernel/stdint.h"
#include "inode.h"
#include "fs.h"
#include "file.h"
#include "../device/ide.h"
#include "../kernel/debug.h"
#include "super_block.h"
#include "../lib/string.h"
#include "../kernel/interrupt.h"

/*存储inode位置*/
struct inode_position{
    bool two_sec;   //跨扇区
    uint32_t sec_lba;   //inode所在扇区号
    uint32_t off_size;  //该扇区中的字节偏移量
};
extern struct partition* cur_part;

/*获得inode所在扇区和扇区内的偏移量*/
static void inode_locate(struct partition* part, uint32_t inode_no, struct inode_position* inode_pos){
    ASSERT(inode_no < 4096);
    uint32_t inode_table_lba = part->sb->inode_table_lba;
    uint32_t inode_size = sizeof(struct inode);
    uint32_t off_size = inode_no * inode_size;
    uint32_t off_sec = off_size / 512;
    uint32_t off_size_in_sec = off_size % 512;
    uint32_t left_in_sec = 512 - off_size_in_sec;
    //剩余空间不足以容纳一个inode, 则跨扇区
    if(left_in_sec < inode_size){
        inode_pos->two_sec = true;
    } else{
        inode_pos->two_sec = false;
    }
    inode_pos->sec_lba = inode_table_lba + off_sec;
    inode_pos->off_size = off_size_in_sec;
}

/*将inode写入到分区part*/
void inode_sync(struct partition* part, struct inode* inode, void* io_buf){
    uint8_t inode_no = inode->i_no;
    struct inode_position inode_pos;
    inode_locate(part, inode_no, &inode_pos);
    ASSERT(inode_pos.sec_lba <= (part->start_lba + part->sec_cnt));

    /*硬盘中inode中的成员inode_tag和i_open_cnt是不需要的*/
    struct inode pure_inode;
    memcpy(&pure_inode, inode, sizeof(struct inode));

    //以下三个成员只存在于内存中, 将inode同步至内存时将这三项清空
    pure_inode.i_open_cnt = 0;
    pure_inode.write_deny = false;
    pure_inode.inode_tag.prev = pure_inode.inode_tag.next = NULL;

    char* inode_buf = (char*)io_buf;
    if(inode_pos.two_sec){
        /*若跨扇区, 就需要读入两个扇区并写入两个扇区*/
        ide_read(part->my_disk, inode_pos.sec_lba ,inode_buf, 2);
        memcpy((inode_buf + inode_pos.off_size), &pure_inode, sizeof(struct inode));
        ide_write(part->my_disk, inode_pos.sec_lba, inode_buf, 2);
    }else{
        ide_read(part->my_disk, inode_pos.sec_lba ,inode_buf, 1);
        memcpy((inode_buf + inode_pos.off_size), &pure_inode, sizeof(struct inode));
        ide_write(part->my_disk, inode_pos.sec_lba, inode_buf, 1);
    }
}

/*根据inode节点号返回相应的inode节点*/
struct inode* inode_open(struct partition* part, uint32_t inode_no){
    /*先在已打开的inode链表中找inode (高速缓冲区)*/
    struct list_elem* elem = part->open_inodes.head.next;
    struct inode* inode_found;
    while(elem != &part->open_inodes.tail){
        inode_found = elem2entry(struct inode, inode_tag, elem);
        if(inode_found->i_no == inode_no){
            inode_found->i_open_cnt++;
            return inode_found;
        }
        elem = elem->next;
    }
    
    /*链表中未找到, 接下来从硬盘中读取inode, 并加入到此链表*/
    struct inode_position inode_pos;
    inode_locate(part, inode_no, &inode_pos);

    /* 为了使sys_malloc创建的新inode被所有任务共享
    需要将inode置于内核空间,故需要临时将cur->pgdir置为NULL */
    struct task_struct* cur = running_thread();
    uint32_t* cur_pagedir_bak = cur->pgdir;
    cur->pgdir = NULL;

    //inode_found所申请的的内存位于内核区域
    inode_found = (struct inode*)sys_malloc(sizeof(struct inode));
    cur->pgdir = cur_pagedir_bak;
    
    char* inode_buf;
    if(inode_pos.two_sec){  //跨扇区, 需要读两个扇区
        inode_buf = (char*)sys_malloc(1024);
        ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 2);
    }else{
        inode_buf = (char*)sys_malloc(512);
        ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 1);
    }
    memcpy(inode_found, inode_buf + inode_pos.off_size, sizeof(struct inode));
    list_push(&part->open_inodes, &inode_found->inode_tag);
    inode_found->i_open_cnt = 1;
    sys_free(inode_buf);
    return inode_found;
}       

/*关闭inode或减少inode的打开数*/
void inode_close(struct inode* inode){
    /*如果没有进程再打开此文件, 将此inode去掉并释放空间*/
    enum intr_status old_status = intr_disable();
    if(--inode->i_open_cnt == 0){
        list_remove(&inode->inode_tag);
        /*位于内核内存池*/
        struct task_struct* cur = running_thread();
        uint32_t* cur_pagedir_bak = cur->pgdir;
        cur->pgdir = NULL;
        sys_free(inode);
        cur->pgdir = cur_pagedir_bak;
    }
    intr_set_status(old_status);
}

/*将硬盘分区part上的inode_no对应的inode清空*/
void inode_delete(struct partition* part, uint32_t inode_no, void* io_buf){
    ASSERT(inode_no < 4096);
    struct inode_position inode_pos;
    inode_locate(part, inode_no, &inode_pos);
    ASSERT(inode_pos.sec_lba <= (part->start_lba + part->sec_cnt));

    char* inode_buf = (char*)io_buf;
    if(inode_pos.two_sec){  //跨扇区, 先读出来
        ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 2);
        memset((inode_buf + inode_pos.off_size), 0, sizeof(struct inode));
        ide_write(part->my_disk, inode_pos.sec_lba, inode_buf, 2);
    }else{
        ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 1);
        memset((inode_buf + inode_pos.off_size), 0, sizeof(struct inode));
        ide_write(part->my_disk, inode_pos.sec_lba, inode_buf, 1);
    }
}

/*税收inode的数据块和inode本身*/
void inode_release(struct partition* part, uint32_t inode_no){
    struct inode* inode_to_del = inode_open(part, inode_no);
    ASSERT(inode_to_del->i_no == inode_no);

    /*回收inode占用的所有块*/
    uint8_t block_idx = 0, block_cnt = 12;
    uint32_t block_bitmap_idx;
    uint32_t all_block[140] = {0};
    while(block_idx < 12){
        all_block[block_idx] = inode_to_del->i_sectors[block_idx];
        block_idx++;
    }
    /*若一级间接表存在, 将128各间接块读到all_blocks中, 并释放以及间接块表占用的扇区*/
    if(inode_to_del->i_sectors[12] != 0){
        ide_read(part->my_disk, inode_to_del->i_sectors[12], all_block + 12, 1);
        block_cnt = 140;
        block_bitmap_idx = inode_to_del->i_sectors[12] - part->sb->data_start_lba;
        ASSERT(block_bitmap_idx > 0);
        bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
        bitmap_sync(cur_part,block_bitmap_idx,BLOCK_BITMAP);
    }

    block_idx = 0;
    /*接下来逐个释放all_blocks中的块地址*/
    while(block_idx < block_cnt){
        if(all_block[block_idx] != 0){
            block_bitmap_idx = 0;
            block_bitmap_idx = all_block[block_idx] - part->sb->data_start_lba;
            ASSERT(block_bitmap_idx > 0);
            bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
            bitmap_sync(cur_part,block_bitmap_idx,BLOCK_BITMAP);
        }
        block_idx++;
    }
    /*回收该inode节点所占用的inode*/
    bitmap_set(&part->inode_bitmap, inode_no, 0);
    bitmap_sync(cur_part, inode_no, INODE_BITMAP);

    //以下仅作调试, 实际上可以不用清空
    void* io_buf = sys_malloc(1024);
    inode_delete(part, inode_no, io_buf);
    sys_free(io_buf);
    inode_close(inode_to_del);
}
/*初始化new_inode*/
void inode_init(uint32_t inode_no, struct inode* new_inode){
    new_inode->i_no = inode_no;
    new_inode->i_size = 0;
    new_inode->i_open_cnt = 0;
    new_inode->write_deny = false;

    /*初始化块索引数组i_sector*/
    uint8_t sec_idx = 0;
    while(sec_idx < 13){
        new_inode->i_sectors[sec_idx++] = 0;
    }
}

