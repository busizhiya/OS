#include "file.h"
#include "../lib/kernel/stdio_kernel.h"
#include "../thread/thread.h"
#include "../device/ide.h"
#include "super_block.h"
#include "fs.h"
#include "inode.h"
#include "dir.h"
#include "../lib/string.h"
#include "../kernel/interrupt.h"
#include "../kernel/debug.h"

/*全局文件表*/
struct file file_table[MAX_FILE_OPEN];

extern struct partition* cur_part;


/*从文件表中获取一个空闲位, 成功返回下标, 失败返回-1*/
int32_t get_free_slot_in_global(void){
    uint32_t fd_idx = 3;
    while(fd_idx < MAX_FILE_OPEN){
        if(file_table[fd_idx].fd_inode == NULL)
            break;
        fd_idx++;
    }
    if(fd_idx == MAX_FILE_OPEN){
        printk("exceed max open files\n");
        return -1;
    }
    return fd_idx;
}

/*将全局描述符下标安装到进程/线程自己的文件描述符数组fd_table中, 返回其下标*/
int32_t pcb_fd_install(int32_t global_fd_idx){
    struct task_struct* cur = running_thread();
    uint8_t local_fd_idx = 3;
    while(local_fd_idx < MAX_FILES_PER_PROC){
        if(cur->fd_table[local_fd_idx] == -1){
            cur->fd_table[local_fd_idx] = global_fd_idx;
            break;
        }
        local_fd_idx++;
    }
    if(local_fd_idx == MAX_FILES_PER_PROC){
        printk("exceed max open files_per_proc\n");
        return -1;
    }
    return local_fd_idx;
}

/*分配一个inode节点, 返回其节点号*/
int32_t inode_bitmap_alloc(struct partition* part){
    int32_t bit_idx = bitmap_scan(&part->inode_bitmap, 1);
    if(bit_idx == -1)
        return -1;
    bitmap_set(&part->inode_bitmap, bit_idx, 1);
    return bit_idx;
}

/*分配一个扇区, 返回具体的扇区地址*/
int32_t block_bitmap_alloc(struct partition* part){
    int32_t bit_idx = bitmap_scan(&part->block_bitmap, 1);
    if(bit_idx == -1)
        return -1;
    bitmap_set(&part->block_bitmap, bit_idx, 1);
    return (part->sb->data_start_lba + bit_idx);
}

/*将内存中bitmap第bit_idx所在的512字节同步到硬盘*/
void bitmap_sync(struct partition* part, uint32_t bit_idx, uint8_t btmp){
    //扇区偏移量
    uint32_t off_sec = bit_idx / 4096;
    //字节偏移量
    uint32_t off_size = off_sec * BLOCK_SIZE;
    uint32_t sec_lba;
    uint8_t* bitmap_off;

    /*需要被同步的位图只有inode_bitmap和block_bitmap*/
    switch(btmp){
        case INODE_BITMAP:
            sec_lba = part->sb->inode_bitmap_lba + off_sec;
            bitmap_off = part->inode_bitmap.bits + off_size;
            break;
        case BLOCK_BITMAP:
            sec_lba = part->sb->block_bitmap_lba + off_sec;
            bitmap_off = part->block_bitmap.bits + off_size;
            break;
    }
    ide_write(part->my_disk, sec_lba, bitmap_off, 1);
}

/*创建文件, 成功则返回文件描述符, 否则返回-1*/
int32_t file_create(struct dir* parent_dir, char* filename, uint8_t flag){
    /*后续操作的公共缓冲区*/
    void* io_buf = sys_malloc(1024);
    if(io_buf == NULL){
        printk("in file_create: sys_malloc for io_buf failed\n");
        return -1;
    }
    
    uint8_t rollback_step = 0; //用于失败时回滚各资源状态

    /*为新文件分配inode*/
    int32_t inode_no = inode_bitmap_alloc(cur_part);
    if(inode_no == -1){
        printk("in file_create: allocate inode failed\n");
        sys_free(io_buf);
        return -1;
    }

    /*此inode要从堆中申请内存, 不可生成函数局部变量*/
    struct inode* new_file_inode = (struct inode*)sys_malloc(sizeof(struct inode));
    if(new_file_inode == NULL){
        printk("file_create: sys_malloc for inode failed\n");
        rollback_step = 1;
        goto rollback;
    }
    inode_init(inode_no, new_file_inode);

    /*返回file_table数组的下标*/
    int fd_idx = get_free_slot_in_global();
    if(fd_idx == -1){
        printk("exceed max open files\n");
        rollback_step = 2;
        goto rollback;
    }
    file_table[fd_idx].fd_inode = new_file_inode;
    file_table[fd_idx].fd_pos = 0;
    file_table[fd_idx].fd_flag = flag;
    file_table[fd_idx].fd_inode->write_deny = false;

    struct dir_entry new_dir_entry;
    memset(&new_dir_entry, 0, sizeof(struct dir_entry));

    create_dir_entry(filename, inode_no, FT_REGULAR, &new_dir_entry);

/*同步数据到硬盘*/
    /*1.在目录parent_dir下安装目录项new_dir_entry*/
    if(!sync_dir_entry(parent_dir, &new_dir_entry, io_buf)){
        printk("sync dir_entry to disk failed\n");
        rollback_step = 3;
        goto rollback;
    }
    memset(io_buf, 0, 1024);

    /*2.将父目录inode节点的内容同步到硬盘*/
    inode_sync(cur_part, parent_dir->inode, io_buf);
    memset(io_buf, 0, 1024);

    /*3.将新创建的inode节点内容同步到硬盘*/
    inode_sync(cur_part, new_file_inode, io_buf);
    memset(io_buf, 0, 1024);

    /*4.将inode_bitmap同步到硬盘*/
    bitmap_sync(cur_part, inode_no, INODE_BITMAP);

    /*5.将创建的文件inode节点加入到open_inodes链表*/
    list_push(&cur_part->open_inodes, &new_file_inode->inode_tag);
    new_file_inode->i_open_cnt = 1;

    sys_free(io_buf);
    return pcb_fd_install(fd_idx);
rollback:
    switch(rollback_step){
        case 3:
            memset(&file_table[fd_idx], 0, sizeof(struct file));
        case 2:
            sys_free(new_file_inode);
        case 1:
            bitmap_set(&cur_part->inode_bitmap, inode_no, 0);
            break;
    }
    sys_free(io_buf);
    return -1;
}

/*打开编号为inode_no的文件, 若成功则返回文件描述符, 否则返回-1*/
int32_t file_open(uint32_t inode_no, uint8_t flag){
    int fd_idx = get_free_slot_in_global();
    if(fd_idx == -1){
        printk("exceed max open files\n");
        return -1;
    }
    file_table[fd_idx].fd_inode = inode_open(cur_part, inode_no);
    file_table[fd_idx].fd_pos = 0;
    file_table[fd_idx].fd_flag = flag;
    bool* write_deny = &file_table[fd_idx].fd_inode->write_deny;

    //与写文件有关
    if(flag & O_WRONLY || flag & O_RDWR){
        enum intr_status old_status = intr_disable();
        if(!(*write_deny)){
            *write_deny = true;
            intr_set_status(old_status);
        } else{
            intr_set_status(old_status);
            printk("file can't be written now, try again now\n");
            return -1;
        }
    }
    return pcb_fd_install(fd_idx);
}

/*关闭文件*/
int32_t file_close(struct file* file){
    if(file == NULL)
        return -1;
    file->fd_inode->write_deny = false;
    inode_close(file->fd_inode);
    file->fd_inode = NULL;
    return 0;
}

/*把buf中的count个字节写入file, 成功返回写入的字节数, 失败返回-1*/
int32_t file_write(struct file* file, const void* buf, uint32_t count){
    if((file->fd_inode->i_size + count) > BLOCK_SIZE*140){
        printk("Exceed max file_size 71680 bytes, write file failed\n");
        return -1;
    }
    uint8_t* io_buf = sys_malloc(512);
    if(io_buf==NULL){
        printk("file_write: sys_malloc for io_buf failed\n");
        return -1;
    }
    uint32_t* all_blocks = (uint32_t*)sys_malloc(BLOCK_SIZE+48);
    memset(all_blocks, 0, BLOCK_SIZE+48);
    if(all_blocks == NULL){
        printk("file_write: sys_malloc for all_blocks failed\n");
        return -1;
    }
    const uint8_t* src = buf;
    uint32_t bytes_written = 0;
    uint32_t size_left = count;
    int32_t block_lba = -1; //块地址
    uint32_t block_bitmap_idx = 0;

    uint32_t sec_idx;
    uint32_t sec_lba;
    uint32_t sec_off_bytes;
    uint32_t sec_left_bytes;
    uint32_t chunk_size;    //每次写入硬盘的数据块大小
    int32_t indirect_block_table;   //一级间接表地址
    uint32_t block_idx; //块索引

    /*判断文件是否是第一次写*/
    if(file->fd_inode->i_sectors[0] == 0){
        block_lba = block_bitmap_alloc(cur_part);
        if(block_lba == -1){
            printk("file_write: block_bitmap_alloc failed\n");
            return -1;
        }
        file->fd_inode->i_sectors[0] = block_lba;
        block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
        bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
    }

    /*本次写入前, 该文件已经占用的块数*/
    uint32_t file_has_used_blocks = file->fd_inode->i_size / BLOCK_SIZE + 1;
    
    /*本次写入后, 该文件将占用的块数*/
    uint32_t file_will_use_blocks = (file->fd_inode->i_size + count) / BLOCK_SIZE + 1;
    ASSERT(file_will_use_blocks <= 140);

    /*通过增量判断是否需要分配扇区*/
    uint32_t add_blocks = file_will_use_blocks - file_has_used_blocks;
    if(add_blocks == 0){
        if(file_will_use_blocks <= 12){
            block_idx = file_has_used_blocks - 1;
            all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
        } else {    //增量为0, 故原文件已经占用的间接块
            ASSERT(file->fd_inode->i_sectors[12] != 0);
            indirect_block_table = file->fd_inode->i_sectors[12];
            ide_read(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);
        }
    }else{
        if(file_will_use_blocks <= 12){/* 情况一.12个直接块够用*/
            block_idx = file_has_used_blocks - 1;   //最后一个可用块(从零开始计数)
            ASSERT(file->fd_inode->i_sectors[block_idx] != 0);
            all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
            
            block_idx = file_has_used_blocks;   //第一个要分配的新扇区
            while(block_idx < file_will_use_blocks){
                block_lba = block_bitmap_alloc(cur_part);
                if(block_lba == -1){
                    printk("file_write: block_bitmap_alloc for situation 1 failed\n");
                    return -1;
                }
                ASSERT(file->fd_inode->i_sectors[block_idx] == 0);
                file->fd_inode->i_sectors[block_idx] = all_blocks[block_idx] = block_lba;
                block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
                bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
                block_idx++;
            }
        }else if(file_has_used_blocks<=12 && file_will_use_blocks > 12){  
        //情况二:旧数据在12个直接块内, 新数据将使用间接块
            block_idx = file_has_used_blocks - 1;
            all_blocks[block_idx]  =file->fd_inode->i_sectors[block_idx];
            
            //创建一级间接块表
            block_lba = block_bitmap_alloc(cur_part);
            if(block_lba == -1){
                printk("file_write: block_bitmap_alloc for situation 2 failed\n");
                return -1;
            }
            ASSERT(file->fd_inode->i_sectors[12] == 0);
            indirect_block_table = file->fd_inode->i_sectors[12] = block_lba;

            block_idx = file_has_used_blocks;
            while(block_idx < file_will_use_blocks){
                block_lba = block_bitmap_alloc(cur_part);
                if(block_lba == -1){
                    printk("file_write: block_bitmap_alloc for situation 2 failed\n");
                    return -1;
                }
                if(block_idx < 12){ //新创建的0~11块直接存入all_blocks
                    ASSERT(file->fd_inode->i_sectors[block_idx] == 0);
                    /*❌block_lba写成block_idx了*/
                    file->fd_inode->i_sectors[block_idx] = all_blocks[block_idx] = block_lba;
                }else{  //间接块只写入all_block中, 待全部分配后一次性同步到硬盘中
                    all_blocks[block_idx] = block_lba;
                }
                block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
                bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
                block_idx++;
            }//同步一级间接表块到硬盘
            ide_write(cur_part->my_disk, indirect_block_table, all_blocks+12, 1);            
        }else if(file_has_used_blocks > 12){
            //第三种情况: 新数据占据间接块
            ASSERT(file->fd_inode->i_sectors[12]!=0);
            indirect_block_table = file->fd_inode->i_sectors[12];
            ide_read(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);
            block_idx = file_has_used_blocks;
            while(block_idx < file_will_use_blocks){
                block_lba = block_bitmap_alloc(cur_part);
                if(block_lba == -1){
                    printk("file_write: block_bitmap_alloc for situation 3 failed\n");
                    return -1;
                }
                all_blocks[block_idx++] = block_lba;
                block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
                bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
            }
            ide_write(cur_part->my_disk, indirect_block_table, all_blocks+12, 1);
        }
    }  
    bool first_write_block = true;
    file->fd_pos = file->fd_inode->i_size - 1;
    while(bytes_written < count){
        memset(io_buf, 0, BLOCK_SIZE);
        sec_idx = file->fd_inode->i_size / BLOCK_SIZE;
        sec_lba = all_blocks[sec_idx];
        sec_off_bytes = file->fd_inode->i_size % BLOCK_SIZE;
        sec_left_bytes = BLOCK_SIZE - sec_off_bytes;

        chunk_size = size_left<sec_left_bytes ? size_left : sec_left_bytes;
        if(first_write_block){  //第一次先读
            ide_read(cur_part->my_disk, sec_lba, io_buf, 1);
            first_write_block = false;
        }
        memcpy(io_buf+sec_off_bytes, src, chunk_size);
        ide_write(cur_part->my_disk, sec_lba, io_buf, 1);
        //printk("file write at lba 0x%x\n",sec_lba);
        src += chunk_size;
        file->fd_inode->i_size += chunk_size;
        file->fd_pos += chunk_size;
        bytes_written += chunk_size;
        size_left -= chunk_size;
    }
    inode_sync(cur_part, file->fd_inode, io_buf);
    sys_free(all_blocks);
    sys_free(io_buf);
    return bytes_written; 
}

/*从文件file中读取count个字节写入buf, 返回独处的字节数, 若读到文件尾则返回-1*/
int32_t file_read(struct file* file, void* buf, uint32_t count){
    uint8_t* buf_dst = (uint8_t*)buf;
    uint32_t size = count, size_left = size;
    if((file->fd_pos + count) > file->fd_inode->i_size){
        size = file->fd_inode->i_size - file->fd_pos;
        size_left = size;
        if(size == 0){
            return -1;
        }
    }
    uint8_t* io_buf = sys_malloc(BLOCK_SIZE);
    if(io_buf == NULL){
        printk("file_read: sys_malloc fot io_buf failed\n");
    }
    uint32_t* all_blocks = (uint32_t*)sys_malloc(BLOCK_SIZE + 48);
    if(all_blocks == NULL){
        sys_free(io_buf);
        printk("file_read: sys_malloc for all_blocks failed\n");
        return -1;
    }
    uint32_t block_read_start_idx = file->fd_pos / BLOCK_SIZE;
    uint32_t block_read_end_idx = (file->fd_pos + size) / BLOCK_SIZE;

    uint32_t read_blocks = block_read_start_idx - block_read_end_idx;
    ASSERT(block_read_start_idx < 139 && block_read_end_idx < 139);
    int indirect_block_table;
    uint32_t block_idx; //待读的块地址
    /*构建all_blocks数组*/
    if(read_blocks == 0){
        ASSERT(block_read_end_idx == block_read_start_idx);
        if(block_read_end_idx < 12){
            block_idx = block_read_end_idx;
            all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
        }else{
            indirect_block_table = file->fd_inode->i_sectors[12];
            ide_read(cur_part->my_disk, indirect_block_table, all_blocks+12,1);
        }
    }else{  //需要读多个块
        /*情况一: 均为直接块*/
        if(block_read_end_idx < 12){
            block_idx = block_read_start_idx;
            while(block_idx <= block_read_end_idx){
                all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
                block_idx++;
            }
        } else if(block_read_start_idx < 12 && block_read_end_idx >= 12){
            //第二种情况, 两种类型都有
            block_idx = block_read_start_idx;
            while(block_idx < 12){
                all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
                block_idx++;
            }
            ASSERT(file->fd_inode->i_sectors[12]!=0);
            indirect_block_table = file->fd_inode->i_sectors[12];
            ide_read(cur_part->my_disk, indirect_block_table, all_blocks+12, 1);
        } else{
            /*第三种情况数据都在间接块中*/
            ASSERT(file->fd_inode->i_sectors[12]!=0);
            indirect_block_table = file->fd_inode->i_sectors[12];
            ide_read(cur_part->my_disk, indirect_block_table, all_blocks+12, 1);
        }
    }
    uint32_t sec_idx, sec_lba, sec_off_bytes, sec_left_bytes, chunk_size;
    uint32_t bytes_read = 0;
    while(bytes_read < size){
        sec_idx = file->fd_pos / BLOCK_SIZE;
        sec_lba = all_blocks[sec_idx];
        sec_off_bytes = file->fd_pos % BLOCK_SIZE;
        sec_left_bytes = BLOCK_SIZE - sec_off_bytes;
        chunk_size = size_left < sec_left_bytes ? size_left : sec_left_bytes;
        memset(io_buf, 0, BLOCK_SIZE);
        ide_read(cur_part->my_disk, sec_lba, io_buf, 1);
        memcpy(buf_dst, io_buf+sec_off_bytes, chunk_size);
        buf_dst += chunk_size;
        file->fd_pos += chunk_size;
        bytes_read += chunk_size;
        size_left -= chunk_size;
    }
    sys_free(all_blocks);
    sys_free(io_buf);
    return bytes_read;


}