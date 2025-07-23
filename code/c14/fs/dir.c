#include "dir.h"
#include "../device/ide.h"
#include "inode.h"
#include "super_block.h"
#include "../lib/kernel/stdio_kernel.h"
#include "../lib/string.h"
#include "../kernel/debug.h"
#include "file.h"


struct dir root_dir;

extern struct partition* cur_part;

/*打开根目录*/
void open_root_dir(struct partition* part){
    root_dir.inode = inode_open(part, part->sb->root_inode_no);
    root_dir.dir_pos = 0;
}

/*在分区part上打开inode_no节点的目录并返回目录指针*/
struct dir* dir_open(struct partition* part, uint32_t inode_no){
    struct dir* pdir = (struct dir*)sys_malloc(sizeof(struct dir));
    pdir->inode = inode_open(part, inode_no);
    pdir->dir_pos = 0;
    return pdir;
}

/*在分区part内的pgdir目录中寻找名为name的文件或目录, 找到后返回true, 并将目录项存入dir_e,否则返回false*/
bool search_dir_entry(struct partition* part, struct dir* pdir, const char* name, struct dir_entry* dir_e){
    uint32_t block_cnt = 140;   //12个直接块+128个间接块 = 140
    uint32_t* all_blocks = (uint32_t*)sys_malloc(block_cnt*4);
    if(all_blocks == NULL){
        printk("search_dir_entry: sys_malloc for all_blocks failed\n");
        return false;
    }
    uint32_t block_idx = 0;
    while(block_idx < 12){
        all_blocks[block_idx] = pdir->inode->i_sectors[block_idx];
        block_idx++;
    }
    block_idx = 0;

    if(pdir->inode->i_sectors[12] != 0){//若含有一级间接块表
        ide_read(part->my_disk, pdir->inode->i_sectors[12], all_blocks+12, 1);
    }
    //至此, all_blocks中存储的是该文件/目录的所有扇区地址
    
    /*写目录项时已经保证目录项不跨扇区*/
    uint8_t* buf = (uint8_t*)sys_malloc(SECTOR_SIZE);
    struct dir_entry* p_de = (struct dir_entry*)buf;
    uint32_t dir_entry_size = part->sb->dir_entry_size;
    uint32_t dir_entry_cnt = SECTOR_SIZE / dir_entry_size;

    while(block_idx < block_cnt){
        if(all_blocks[block_idx] == 0){
            block_idx++;
            continue;
        }
        ide_read(part->my_disk, all_blocks[block_idx], buf, 1);
        uint32_t dir_entry_idx = 0;
        while(dir_entry_idx < dir_entry_cnt){
            if(!strcmp(p_de->filename, name)){
                memcpy(dir_e, p_de, dir_entry_size);
                sys_free(buf);
                sys_free(all_blocks);
                return true;
            }
            dir_entry_idx++;
            p_de++;
        }
        block_idx++;
        p_de = (struct dir_entry*)buf;
        memset(buf, 0, SECTOR_SIZE);
    }
    sys_free(buf);
    sys_free(all_blocks);
    return false;
}

/*关闭目录*/
void dir_close(struct dir* dir){
    /*根目录不能关闭*/
    if(dir == &root_dir)
        return;
    inode_close(dir->inode);
    sys_free(dir);
}

/*在内存中初始化目录项p_de*/
void create_dir_entry(char* filename, uint32_t inode_no, uint8_t file_type, struct dir_entry* p_de){
    ASSERT(strlen(filename)<= MAX_FILE_NAME_LEN);
    memcpy(p_de->filename, filename, strlen(filename));
    p_de->i_no = inode_no;
    p_de->f_type = file_type;
}

//问题:第一级间接块未读入all_blocks中, 导致i_sectors[12]被重复覆盖, 无法分配更多的内存
/*将目录项p_de写入父目录parent_dir中,io_buf由主调函数提供*/
bool sync_dir_entry(struct dir* parent_dir, struct dir_entry* p_de, void* io_buf){
    struct inode* dir_inode = parent_dir->inode;
    uint32_t dir_size = dir_inode->i_size;
    uint32_t dir_entry_size = cur_part->sb->dir_entry_size;
    ASSERT(dir_size % dir_entry_size == 0);
    uint32_t dir_entries_per_sec = (512 / dir_entry_size);
    int32_t block_lba = -1;
    uint8_t block_idx = 0;
    uint32_t all_blocks[140] = {0};
    while(block_idx<12){
        all_blocks[block_idx] = dir_inode->i_sectors[block_idx];
        block_idx++;
    }
    struct dir_entry* dir_e = (struct dir_entry*)io_buf;

    int32_t block_bitmap_idx = -1;
    block_idx = 0;
    while(block_idx<140){
        block_bitmap_idx = -1;
        if(all_blocks[block_idx] == 0){ //扇区未分配
            block_lba = block_bitmap_alloc(cur_part);
            if(block_lba == -1){
                printk("alloc block bitmap for sync_dir_entry failed\n");
                return false;
            }
            /*每分配一个块就同步一次block_bitmap*/
            block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
            ASSERT(block_bitmap_idx != -1);
            bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);

            block_bitmap_idx = -1;
            if(block_idx<12){
                dir_inode->i_sectors[block_idx] = all_blocks[block_idx] = block_lba;
            }else if(block_idx == 12 && dir_inode->i_sectors[12] == 0){  //第一级间接块
                /*若未分配第一级间接块, 将第一次申请的块给第一级间接块, 同时再申请一块作为直接块*/ 
                dir_inode->i_sectors[12] = block_lba;
                block_lba = -1;
                //尝试分配第零级块
                block_lba = block_bitmap_alloc(cur_part);
                if(block_lba == -1){    //分配失败,回滚
                    block_bitmap_idx = dir_inode->i_sectors[12] - cur_part->sb->data_start_lba;
                    bitmap_set(&cur_part->block_bitmap, block_bitmap_idx, 0);
                    dir_inode->i_sectors[12] = 0;
                    printk("alloc block bitmap for sync_dir_entry failed\n");
                    return false;
                }
                block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
                ASSERT(block_bitmap_idx != -1);
                bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
                all_blocks[12] = block_lba;
                ide_write(cur_part->my_disk, dir_inode->i_sectors[12], all_blocks+12, 1);
            }else{//间接块
                //此时i_sectors[12]是以及间接块扇区号, all_blocks+12就是那由第一级间接块支持的128个第零级间接块地址
                //all_blocks[13~...]未赋值, 此处write会直接覆盖之前经第一级间接块分配的第零级间接块
                /*** 修改*/
                ide_read(cur_part->my_disk, dir_inode->i_sectors[12], all_blocks+12, 1);
                all_blocks[block_idx] = block_lba;
                ide_write(cur_part->my_disk, dir_inode->i_sectors[12], all_blocks+12, 1);
            }
            memset(io_buf, 0, 512);
            memcpy(io_buf, p_de, dir_entry_size);
            ide_write(cur_part->my_disk, all_blocks[block_idx], io_buf, 1);
            dir_inode->i_size += dir_entry_size;
            return true;
        }
        /*block_idx块已经存在, 将其读入内存, 然后在该块中查找空目录项*/
        ide_read(cur_part->my_disk, all_blocks[block_idx], io_buf, 1);
        uint8_t dir_entry_idx = 0;
        while(dir_entry_idx < dir_entries_per_sec){
            if((dir_e + dir_entry_idx)->f_type == FT_UNKNOWN){
                memcpy(dir_e+dir_entry_idx,p_de, dir_entry_size);
                ide_write(cur_part->my_disk, all_blocks[block_idx], io_buf, 1);
                dir_inode->i_size += dir_entry_size;
                return true;
            }
            dir_entry_idx++;
        }
        block_idx++;
    }
    printk("directory is full!\n");
    return false;

}

/*把分区part目录pdir中编号为inode_no的目录项删除*/
bool delete_dir_entry(struct partition* part, struct dir* pdir, uint32_t inode_no, void* io_buf){
    struct inode* dir_inode = pdir->inode;
    uint32_t block_idx = 0, all_blocks[140] = {0};
    while(block_idx < 12){
        all_blocks[block_idx] = dir_inode->i_sectors[block_idx];
        block_idx++;
    }
    if(dir_inode->i_sectors[12]!=0){
        ide_read(part->my_disk, dir_inode->i_sectors[12], all_blocks+12, 1);
    }
    uint32_t dir_entry_size = part->sb->dir_entry_size;
    uint32_t dir_entries_per_sec = (SECTOR_SIZE / dir_entry_size);

    struct dir_entry* dir_e = (struct dir_entry*)io_buf;
    struct dir_entry* dir_entry_found = NULL;
    uint8_t dir_entry_idx, dir_entry_cnt;
    bool is_dir_first_block  = false;

    /*遍历所有块, 寻找目录项*/
    block_idx = 0;
    while(block_idx < 140){
        is_dir_first_block = false;
        if(all_blocks[block_idx] == 0){
            block_idx++;
            continue;
        }
        dir_entry_idx = dir_entry_cnt = 0;
        memset(io_buf, 0, SECTOR_SIZE);
        ide_read(part->my_disk, all_blocks[block_idx], io_buf, 1);

        while(dir_entry_idx < dir_entries_per_sec){
            if((dir_e + dir_entry_idx)->f_type != FT_UNKNOWN){
                if(!strcmp((dir_e + dir_entry_idx)->filename,".")){
                    is_dir_first_block = true;
                } else if(strcmp((dir_e + dir_entry_idx)->filename,".") && strcmp((dir_e + dir_entry_idx)->filename,"..")){
                    dir_entry_cnt++;//统计此扇区内除了.和..的目录项个数, 用于判断是否回收该扇区
                    if((dir_e + dir_entry_idx)->i_no == inode_no){
                        //找到了
                        ASSERT(dir_entry_found == NULL);
                        dir_entry_found = dir_e + dir_entry_idx;
                    }
                }
            }
            dir_entry_idx++;
        }
        if(dir_entry_found == NULL){
            block_idx++;
            continue;
        }
        /*能执行到下面,说明已经找到了*/
        /*在当前扇区找到了目录项, 判断是否回收扇区*/
        if(dir_entry_cnt == 1 && !is_dir_first_block){
            /*1. 在块位图中回收*/
            uint32_t block_bitmap_idx = all_blocks[block_idx] - part->sb->data_start_lba;
            bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
            bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);

            /*将块地址从索引表/i_sectors中去掉*/
            if(block_idx < 12){
                dir_inode->i_sectors[block_idx] = 0;
            }else{
                /*先判断一级间接索表中间接块项的数量, 如果只有一个, 联通间接索引表一同回收*/
                uint32_t indirect_blocks = 0;
                uint32_t indirect_block_idx = 12;
                while(indirect_block_idx < 140){
                    if(all_blocks[indirect_block_idx] != 0)
                        indirect_blocks++;
                }
                ASSERT(indirect_blocks >= 1);   //包含当前块

                if(indirect_blocks > 1){    //仅擦除此项
                    all_blocks[block_idx] = 0;
                    ide_write(part->my_disk, dir_inode->i_sectors[12], all_blocks + 12, 1);
                } else {
                    //直接把间接索引表所在的块回收
                    block_bitmap_idx = dir_inode->i_sectors[12] - part->sb->data_start_lba;
                    bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
                    bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
                    dir_inode->i_sectors[12] = 0;
                }
            }
        } else {    //仅将该目录项清空
            memset(dir_entry_found, 0, dir_entry_size);
            ide_write(part->my_disk, all_blocks[block_idx], io_buf, 1);
        }
        /*更新inode节点并同步到硬盘*/
        ASSERT(dir_inode->i_size >= dir_entry_size);
        dir_inode->i_size -= dir_entry_size;
        memset(io_buf, 0, SECTOR_SIZE * 2);
        inode_sync(part, dir_inode, io_buf);
        
        return true;
    }
    return false;

}

/*读取目录, 成功返回一个目录项, 失败返回NULL*/
struct dir_entry* dir_read(struct dir* dir){
    
    struct dir_entry* dir_e = (struct dir_entry*)dir->dir_buf;
    struct inode* dir_inode = dir->inode;
    uint32_t all_blocks[140] = {0}, block_cnt = 0;
    uint32_t block_idx = 0, dir_entry_idx = 0;
    while(block_idx < 12){
        all_blocks[block_idx] = dir_inode->i_sectors[block_idx];
        block_idx++;
    }
    if(dir_inode->i_sectors[12] != 0){
        ide_read(cur_part->my_disk, dir_inode->i_sectors[12], all_blocks+12, 1);
        block_cnt = 140;
    }
    block_idx = 0;
    uint32_t cur_dir_entry_pos = 0;//当前目录项的偏移, 用来判断是否是之前返回过的目录项
    uint32_t dir_entry_size = cur_part->sb->dir_entry_size;
    uint32_t dir_entries_per_sec = SECTOR_SIZE / dir_entry_size;
    while(dir->dir_pos < dir_inode->i_size){
        if(all_blocks[block_idx] == 0){
            block_idx++;
            continue;
        }
        memset(dir_e, 0, SECTOR_SIZE);
        ide_read(cur_part->my_disk, all_blocks[block_idx], dir_e, 1);
        dir_entry_idx = 0;
        /*遍历该扇区内所有目录项*/
        while(dir_entry_idx < dir_entries_per_sec){
            if((dir_e+dir_entry_idx)->f_type != FT_UNKNOWN){
                if(cur_dir_entry_pos < dir->dir_pos){   //判断是否为最新的目录项
                        cur_dir_entry_pos += dir_entry_size;
                        dir_entry_idx++;
                        continue;
                }
                /*注: dir->dir_pos与cur_dir_entry_pos未计入空目录项, 只计入存在的目录项*/
                ASSERT(cur_dir_entry_pos == dir->dir_pos);
                dir->dir_pos += dir_entry_size; //更新为新位置(下一个返回目录项的地址)
                return dir_e + dir_entry_idx;  //
            }
            dir_entry_idx++;
        }
        block_idx++;
    }
    return NULL;
}

/*判断目录是否为空*/
bool dir_is_empty(struct dir* dir){
    return (dir->inode->i_size == cur_part->sb->dir_entry_size * 2);
}

/*在父目录parent_dir中删除child_dir*/
int32_t dir_remove(struct dir* parent_dir, struct dir* child_dir){
    struct inode* child_dir_inode = child_dir->inode;
    int32_t block_idx = 1;
    while(block_idx < 13){  //除了i_sectors[0]中有扇区, 其他均为空
        ASSERT(child_dir_inode->i_sectors[block_idx]==0);
        block_idx++;
    }
    void* io_buf = sys_malloc(SECTOR_SIZE * 2);
    if(io_buf == NULL){
        printk("dir_remove: sys_malloc for io_buf failed\n");
        return -1;
    }
    /*在父目录中删除对应目录项*/
    delete_dir_entry(cur_part, parent_dir, child_dir_inode->i_no, io_buf);

    /*回收inode中i_sectors中所占用的扇区*/
    inode_release(cur_part, child_dir_inode->i_no);
    sys_free(io_buf);
    return 0;
}