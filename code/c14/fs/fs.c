#include "fs.h"
#include "super_block.h"
#include "inode.h"
#include "dir.h"
#include "file.h"
#include "../lib/kernel/bitmap.h"
#include "../device/ide.h"
#include "../kernel/global.h"
#include "../lib/kernel/stdio_kernel.h"
#include "../lib/string.h"
#include "../kernel/debug.h"
#include "../device/console.h"

extern uint8_t channel_cnt;
extern struct ide_channel channels[2];
extern struct list partition_list;
extern struct dir root_dir;
extern struct file file_table[MAX_FILE_OPEN];

struct partition* cur_part;//默认情况下操作的分区

/*在分区列表中找到名为part_name的分区, 并将其指针赋值给cur_part*/
static bool mount_partition(struct list_elem* pelem, int arg){
    char* part_name = (char*)arg;
    struct partition* part = elem2entry(struct partition, part_tag, pelem);
    if(!strcmp(part->name, part_name)){
        cur_part = part;
        struct disk* hd = cur_part->my_disk;
        struct super_block* sb_buf = (struct super_block*)sys_malloc(SECTOR_SIZE);

        /*在内存中创建分区cur_part的超级块*/
        cur_part->sb = (struct super_block*)sys_malloc(sizeof(struct super_block));
        if(cur_part->sb == NULL)
            PANIC("alloc memory failed");
        memset(sb_buf, 0, SECTOR_SIZE);
        ide_read(hd, cur_part->start_lba+1, sb_buf, 1);
        memcpy(cur_part->sb, sb_buf, sizeof(struct super_block));
        /*读入块位图*/
        cur_part->block_bitmap.bits = \
        (uint8_t*)sys_malloc(sb_buf->block_bitmap_sects*SECTOR_SIZE);
        if(cur_part->block_bitmap.bits==NULL)
            PANIC("alloc memory failed");
        cur_part->block_bitmap.btmp_bytes_len = sb_buf->block_bitmap_sects*SECTOR_SIZE;
        ide_read(hd, sb_buf->block_bitmap_lba, cur_part->block_bitmap.bits, sb_buf->block_bitmap_sects);

        /*读入inode位图*/
        cur_part->inode_bitmap.bits =
        (uint8_t*)sys_malloc(sb_buf->inode_bitmap_sects*SECTOR_SIZE);
        if(cur_part->inode_bitmap.bits == NULL)
            PANIC("alloc memory failed");
        cur_part->inode_bitmap.btmp_bytes_len = sb_buf->inode_bitmap_sects * SECTOR_SIZE;
        ide_read(hd, sb_buf->inode_bitmap_lba, cur_part->inode_bitmap.bits, sb_buf->inode_bitmap_sects);

        list_init(&cur_part->open_inodes);
        printk("mount %s done!\n",part->name);
        return true;
        //返回true, list_traversal结束
    }
    return false;
}

/*格式化分区, 初始化分区的元信息, 创建文件系统*/
static void partition_format(struct partition* part){
    /*1. block_bitmap_init*/
    uint32_t boot_sector_sects = 1;
    uint32_t super_block_sects = 1;
    uint32_t inode_bitmap_sects = DIV_ROUND_UP(MAX_FILES_PER_PART, BITS_PER_SECTOR);
    uint32_t inode_table_sects = DIV_ROUND_UP(((sizeof(struct inode) * MAX_FILES_PER_PART)), SECTOR_SIZE);
    uint32_t used_sects = boot_sector_sects + super_block_sects + inode_bitmap_sects + inode_table_sects;
    uint32_t free_sects = part->sec_cnt - used_sects;

    /*2. 处理块位图占据的扇区数*/
    uint32_t block_bitmap_sects;
    block_bitmap_sects = DIV_ROUND_UP(free_sects, BITS_PER_SECTOR);
    uint32_t block_bitmap_bit_len = free_sects - block_bitmap_sects;
    block_bitmap_sects = DIV_ROUND_UP(block_bitmap_bit_len, BITS_PER_SECTOR);

    /*3. 超级块初始化*/
    struct super_block sb;
    sb.magic = 0x20070329;
    sb.sec_cnt = part->sec_cnt;
    sb.inode_cnt = MAX_FILES_PER_PART;
    sb.part_lba_base = part->start_lba;

    sb.block_bitmap_lba = sb.part_lba_base + 2;
    /*第0块是引导块, 第1块是超级块*/
    sb.block_bitmap_sects = block_bitmap_sects;

    sb.inode_bitmap_lba = sb.block_bitmap_lba + sb.block_bitmap_sects;
    sb.inode_bitmap_sects = inode_bitmap_sects;

    sb.inode_table_lba = sb.inode_bitmap_lba + sb.inode_bitmap_sects;
    sb.inode_table_sects = inode_table_sects;

    sb.data_start_lba = sb.inode_table_lba + sb.inode_table_sects;
    sb.root_inode_no = 0;
    sb.dir_entry_size = sizeof(struct dir_entry);
    printk("%s info:\n",part->name);
    printk("\tmagic: 0x%x\n\tpart_lba_base:0x%x\tall_sectors:0x%x",sb.magic,sb.part_lba_base,sb.sec_cnt);
    printk("\tinode_cnt:0x%x\n\tblock_bitmap_lba:0x%x\tblock_bitmap_sectos:0x%x\n",sb.inode_cnt,sb.inode_bitmap_lba,sb.inode_bitmap_sects);
    printk("\tinode_table_lba:0x%x\tinode_table_sectors:0x%x\n",sb.inode_table_lba,sb.inode_table_sects);
    printk("\tdata_start_lba:0x%x\n",sb.data_start_lba);

    struct disk* hd = part->my_disk;
/*1. 将超级块写入本分区的1扇区*/
    ide_write(hd, part->start_lba+1, &sb, 1);
    printk("\tsuper_block_lba::0x%x\n",part->start_lba+1);

    /*找出数据量最大的元信息, 用其尺寸做缓冲区*/
    uint32_t buf_size = (sb.block_bitmap_sects >= sb.inode_bitmap_sects ? sb.block_bitmap_sects : sb.inode_bitmap_sects);
    buf_size = (buf_size >= sb.inode_table_sects ? buf_size : sb.inode_table_sects) * SECTOR_SIZE;

    //buf为数据的缓冲区
    uint8_t* buf = (uint8_t*)sys_malloc(buf_size);
    
/*2. 将块位图初始化并写入sb.block_bitnap_lba*/
    buf[0] |= 0x01; //第0个块预留给根目录, 位图中先占位
    uint32_t block_bitmap_last_byte = block_bitmap_bit_len / 8;
    uint8_t block_bitmap_last_bit = block_bitmap_bit_len % 8;
    //最后一个扇区中不足一扇区的其余部分
    uint32_t last_size = SECTOR_SIZE - (block_bitmap_last_byte % SECTOR_SIZE);
    
    /*1.现将位图最后一字节到其扇区结束的位置全置为1, 即超出实际块数的部分设置为已占用*/
    memset(&buf[block_bitmap_last_byte], 0xff, last_size);
    uint8_t bit_idx = 0;
    /*2.将上一步中最后一字节中有效部分置为0*/
    while(bit_idx <= block_bitmap_last_bit){
        buf[block_bitmap_last_byte] &= ~(1 << bit_idx++);
    }
    ide_write(hd, sb.block_bitmap_lba, buf, sb.block_bitmap_sects);
/*3. 将inode位图初始化并写入sb.inode_bitmap_lba*/
    /*先清空缓冲区*/
    memset(buf, 0, buf_size);
    buf[0] |= 0x1;
    /*inode_table恰好有4096个元素, 故inode_bitmap恰好占用一个扇区, 所以最后一个扇区中没有多余部分*/
    ide_write(hd, sb.inode_bitmap_lba, buf, sb.inode_bitmap_sects);

/*4. 将inode数组初始化并写入sb.inode_table_lba*/
    memset(buf, 0, buf_size);
    struct inode* i = (struct inode*)buf;
    i->i_size = sb.dir_entry_size*2;    //.和..目录
    i->i_no = 0;    //根目录
    i->i_sectors[0] = sb.data_start_lba;
    ide_write(hd, sb.inode_table_lba, buf, sb.inode_table_sects);

/*5. 将根目录写入sb.data_start_lba*/
    memset(buf, 0, buf_size);
    struct dir_entry* p_de = (struct dir_entry*)buf;

    /*初始化.目录*/
    memcpy(p_de->filename,".", 1);
    p_de->f_type = FT_DIRECTORY;
    p_de->i_no = 0;
    p_de++;
    /*初始化..目录*/
    memcpy(p_de->filename,"..", 2);
    p_de->i_no = 0; //根目录的父目录还是自己
    p_de->f_type = FT_DIRECTORY;

    ide_write(hd, sb.data_start_lba, buf, 1);
    printk("\troot_dir_lba:0x%x\t",sb.data_start_lba);
    printk("%s format done\n", part->name);
    sys_free(buf);
}

/*将最上层路径名称解析出来,返回剩下的路径*/
static char* path_parse(char* pathname, char* name_store){
    if(pathname[0] == '/'){//跳过根目录
        while(*(++pathname)=='/');
    }

    while(*pathname != '/' && *pathname != 0){
        *name_store++ = *pathname++;
    }

    if(pathname[0]==0)  //剩下的路径字符串为空
        return NULL;
    
    return pathname;
}

/*返回路径深度*/
int32_t path_depth_cnt(char* pathname){
    ASSERT(pathname!=NULL);
    char* p = pathname;
    char name[MAX_FILE_NAME_LEN];
    uint32_t depth = 0;
    p = path_parse(p, name);
    while(name[0]){
        depth++;
        memset(name,0, MAX_FILE_NAME_LEN);
        if(p)
            p=path_parse(p, name);
    }
    return depth;
}

/* 搜索文件pathname(全路径),若找到返回其inode号, 否则返回-1*/
static int search_file(const char* pathname, struct path_search_record* searched_record){
    //根目录
    if(!strcmp(pathname, "/") || !strcmp(pathname, "/.") || !strcmp(pathname, "/..")){
        searched_record->parent_dir = &root_dir;
        searched_record->file_type = FT_DIRECTORY;
        searched_record->searched_path[0] = 0;  //搜索路径置空
        return 0;
    }
    uint32_t path_len = strlen(pathname);
    ASSERT(pathname[0] == '/' && path_len > 1 && path_len < MAX_PATH_LEN);
    char* sub_path = (char*)pathname;
    struct dir* parent_dir = &root_dir;
    struct dir_entry dir_e;

    /*揭露解析出来的各级名称*/
    //出现问题!!
    //char name[MAX_FILE_NAME_LEN] = {0};
    char name[MAX_FILE_NAME_LEN];
    for(int i = 0;i < MAX_FILE_NAME_LEN; i++)
        name[i] = 0;

    searched_record->parent_dir = parent_dir;
    searched_record->file_type = FT_UNKNOWN;
    uint32_t parent_inode_no = 0;   //父目录的inode号
    
    sub_path = path_parse(sub_path, name);
    while(name[0]){
        ASSERT(strlen(searched_record->searched_path)<512);
        //记录已经存在的父目录
        strcat(searched_record->searched_path, "/");
        strcat(searched_record->searched_path, name);
        //在所给的目录中查找文件
        if(search_dir_entry(cur_part, parent_dir, name, &dir_e)){
            memset(name, 0, MAX_FILE_NAME_LEN);
            if(sub_path)    //非空, 继续解析
                sub_path = path_parse(sub_path, name);
            if(dir_e.f_type == FT_DIRECTORY){   //被打开的是目录
                parent_inode_no = parent_dir->inode->i_no;
                dir_close(parent_dir);
                parent_dir = dir_open(cur_part, dir_e.i_no);
                searched_record->parent_dir = parent_dir;
                continue;
            } else if(dir_e.f_type == FT_REGULAR){
                searched_record->file_type = FT_REGULAR;
                return dir_e.i_no;
            }
        }else{
            return -1;
        }
    }
    //执行到此,说明要查找的不是文件,而是目录
    //此时searched_record->parent_dir保存的是最后一个目录自己, 而不是他的父目录
    dir_close(searched_record->parent_dir);

    /*保存被查找目录的直接父目录*/
    searched_record->parent_dir = dir_open(cur_part, parent_inode_no);
    searched_record->file_type = FT_DIRECTORY;
    return dir_e.i_no;
}

/*打开/创建文件成功后, 返回文件描述符, 否则返回-1*/
int32_t sys_open(const char* pathname, uint8_t flag){
    if(pathname[strlen(pathname)-1]=='/'){
        printk("Can't open a firectory %s\n",pathname);
        return -1;
    }
    
    ASSERT(flag <= 7);
    int32_t fd = -1;

    struct path_search_record searched_record;
    memset(&searched_record,0, sizeof(struct path_search_record));
    
    uint32_t pathname_depth = path_depth_cnt((char*)pathname);
    
    /*先检查文件是否存在*/
    int inode_no = search_file(pathname, &searched_record);
    bool found = inode_no != -1 ? true : false;
    if(searched_record.file_type == FT_DIRECTORY){
        printk("Can't open a directory with open(), use opendir() instead\n");
        dir_close(searched_record.parent_dir);
        return -1;
    }
    
    
    uint32_t path_searched_depth = path_depth_cnt(searched_record.searched_path);
    if(pathname_depth != path_searched_depth){
        //某个中间目录不存在
        printk("cannot access %s: Not a directory, subpath %s isn't exist\n", pathname, searched_record.searched_path);
        dir_close(searched_record.parent_dir);
        return -1;
    }

    //如果是在最后一个路径上没找到, 且并不是创建文件, 返回-1
    if(!found && !(flag & O_CREAT)){
        printk("in path %s, file %s isn't exist\n", searched_record.searched_path, (strrchr(searched_record.searched_path,'/') + 1));
        return -1;
    }else if(found && (flag & O_CREAT)){//要创建的文件已存在
        printk("%s has already exist\n", pathname);
        dir_close(searched_record.parent_dir);
        return -1;
    }
    switch(flag & O_CREAT){
        case O_CREAT:
            printk("creating file\n");
            fd = file_create(searched_record.parent_dir,(strrchr(pathname, '/')+1),flag);
            dir_close(searched_record.parent_dir);
            break;
        //其余为打开文件
        default:
            fd = file_open(inode_no, flag);
    }
    return fd;
}

/*将文件描述符转化为全局文件表的下标*/
static uint32_t fd_local2global(uint32_t local_fd){
    struct task_struct* cur = running_thread();
    int32_t global_fd = cur->fd_table[local_fd];
    ASSERT(global_fd >= 0 && global_fd < MAX_FILE_OPEN);
    return (uint32_t)global_fd;
}

/*关闭文件描述符fd所指向的文件. 成功返回0, 失败返回-1*/
int32_t sys_close(int32_t fd){
    int32_t ret = -1;
    if(fd > 2){
        uint32_t _fd = fd_local2global(fd);
        ret = file_close(&file_table[_fd]);
        running_thread()->fd_table[fd] = -1;
    }
    return ret;
}

/*将buf中连续count个字节写入文件描述符fd, 成功则返回写入的字节数,失败返回-1*/
int32_t sys_write(int32_t fd,const void* buf, uint32_t count){
    if(fd < 0){
        printk("sys_write: fd error");
        return -1;
    }
    if(fd == stdout_no){
        char tmp_buf[1024] = {0};
        memcpy(tmp_buf, buf, count);
        console_put_str(tmp_buf);
        return count;
    }
    uint32_t _fd = fd_local2global(fd);
    struct file* wr_file = &file_table[_fd];
    if(wr_file->fd_flag & O_WRONLY || wr_file->fd_flag & O_RDWR){
        uint32_t bytes_written = file_write(wr_file, buf, count);
        return bytes_written;
    }else{
        console_put_str("sys_write: not allowd to write file without O_RDWR or O_WRONLY\n");
        return -1;
    }
}

/*从文件描述符指向的文件读取count个字节到buf, 若成功则返回读出的字节数, 到文件尾则返回-1*/
int32_t sys_read(int32_t fd, void* buf, uint32_t count){
    
    if(fd < 0){
        printk("sys_read: fd_error\n");
        return -1;
    }
    ASSERT(buf != NULL);
    uint32_t _fd = fd_local2global(fd);
    return file_read(&file_table[_fd], buf, count);
}

/*重置用于文件读写操作的偏移指针, 成功时返回新的偏移量, 出错返回-1*/
int32_t sys_lseek(int32_t fd, int32_t offset, uint8_t whence){
    if(fd < 0){
        printk("sys_lseek: fd error\n");
        return -1;
    }
    ASSERT(whence > 0 && whence < 4);
    uint32_t _fd = fd_local2global(fd);
    struct file* pf = &file_table[_fd];
    int32_t new_pos = 0;
    int32_t file_size = (int32_t)pf->fd_inode->i_size;
    switch(whence){
        case SEEK_SET:
            new_pos = offset;
            break;
        case SEEK_CUR:
            new_pos = (int32_t)pf->fd_pos + offset;
            break;
        case SEEK_END:
            new_pos = file_size + offset;   //此情况下, offset应该为负值
            break;
    }
    if(new_pos < 0 || new_pos >= file_size)
        return -1;
    pf->fd_pos = new_pos;
    return pf->fd_pos;
}

/*删除文件(非目录), 成功返回0, 失败返回-1*/
int32_t sys_unlink(const char* pathname){
    ASSERT(strlen(pathname) < MAX_PATH_LEN);
    struct path_search_record searched_record;
    memset(&searched_record, 0, sizeof(struct path_search_record));
    int inode_no = search_file(pathname, &searched_record);
    ASSERT(inode_no != 0);
    if(inode_no == -1){
        printk("file %s not found\n", pathname);
        dir_close(searched_record.parent_dir);
        return -1;
    }
    if(searched_record.file_type == FT_DIRECTORY){
        printk("can't delete a directory with unlink(), use rmdir() instead\n");
        dir_close(searched_record.parent_dir);
        return -1;
    }
    /*检查是否在已打开文件列表中*/
    uint32_t file_idx = 0;
    while(file_idx < MAX_FILE_OPEN){
        if(file_table[file_idx].fd_inode != NULL && (uint32_t)inode_no == file_table[file_idx].fd_inode->i_no){
            break;   
        }
        file_idx++;
    }
    if(file_idx < MAX_FILE_OPEN){
        dir_close(searched_record.parent_dir);
        printk("file %s is in use, not allowed to delete!\n", pathname);
        return -1;
    }
    ASSERT(file_idx == MAX_FILE_OPEN);
    /*为delete_dir_entry申请缓冲区*/
    void* io_buf = sys_malloc(SECTOR_SIZE * 2);
    if(io_buf == NULL){
        dir_close(searched_record.parent_dir);
        printk("sys_unlink: malloc for io_buf failed\n");
        return -1;
    }

    struct dir* parent_dir = searched_record.parent_dir;
    delete_dir_entry(cur_part, parent_dir, inode_no, io_buf);
    inode_release(cur_part, inode_no);
    sys_free(io_buf);
    dir_close(searched_record.parent_dir);
    return 0;
}

/*创建目录pathname, 成功返回0, 失败返回-1*/
int32_t sys_mkdir(const char* pathname){
    uint8_t rollback_step = 0;
    void* io_buf = sys_malloc(SECTOR_SIZE * 2);
    if(io_buf == NULL){
        printk("sys_mkdir: sys_malloc for io_buf failed\n");
        return -1;
    }

    struct path_search_record searched_record;
    memset(&searched_record, 0, sizeof(struct path_search_record));
    int inode_no = -1;
    inode_no = search_file(pathname, &searched_record);
    if(inode_no != -1){ //找到了
        printk("sys_mkdir: file or directory %s exist\n",pathname);
        rollback_step = 1;
        goto rollback;
    }else{  //没找到, 但判断路径中是否有不存在的中间目录
        uint32_t pathname_depth = path_depth_cnt((char*)pathname);
        uint32_t path_searched_depth = path_depth_cnt(searched_record.searched_path);
        if(pathname_depth != path_searched_depth){
            printk("sys_mkdir: cannot access %s: Not a directory, subpath %s isn't exist\n",pathname, searched_record.searched_path);
            rollback_step = 1;
            goto rollback;
        }
    }
    struct dir* parent_dir = searched_record.parent_dir;
    char* dirname = strrchr(searched_record.searched_path, '/') + 1;
    inode_no = inode_bitmap_alloc(cur_part);
    //后面会同步inode_bitmap

    //为新目录(新文件)创建inode
    if(inode_no == -1){
        printk("sys_mkdir: allocate inode failed\n");
        rollback_step = 1;
        goto rollback;
    }
    //局部变量,但本函数内会将数据写入硬盘,无需担心
    struct inode new_dir_inode;
    inode_init(inode_no, &new_dir_inode);
    
    uint32_t block_bitmap_idx = 0;
    int32_t block_lba = -1;
    //为新目录分配一个块, 用于写入目录项.和..
    block_lba = block_bitmap_alloc(cur_part);
    if(block_lba == -1){
        printk("sys_mkdir block_bitmap_alloc for new directory failed\n");
        rollback_step = 2;
        goto rollback;
    }
    new_dir_inode.i_sectors[0] = block_lba;
    block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
    ASSERT(block_bitmap_idx != 0);
    bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
    
    /*将当前目录的目录项.和..写入目录*/
    memset(io_buf, 0, SECTOR_SIZE*2);
    struct dir_entry* p_de = (struct dir_entry*)io_buf;
    
    memcpy(p_de->filename, ".",1);
    p_de->i_no = inode_no;
    p_de->f_type = FT_DIRECTORY;
    p_de++;

    memcpy(p_de->filename, "..",2);
    p_de->i_no = parent_dir->inode->i_no;
    p_de->f_type = FT_DIRECTORY;

    ide_write(cur_part->my_disk, new_dir_inode.i_sectors[0],io_buf, 1);

    new_dir_inode.i_size = 2 * cur_part->sb->dir_entry_size;
    /*在父目录中添加新目录的目录项*/
    struct dir_entry new_dir_entry;
    memset(&new_dir_entry, 0, sizeof(struct dir_entry));
    create_dir_entry(dirname, inode_no, FT_DIRECTORY, &new_dir_entry);
    memset(io_buf, 0, SECTOR_SIZE*2);
    //先将父目录的信息同步到磁盘
    if(!sync_dir_entry(parent_dir, &new_dir_entry, io_buf)){
        printk("sys_mkdir: sync_dir_entry to disk failed\n");
        rollback_step = 2;
        goto rollback;
    }
    memset(io_buf, 0, SECTOR_SIZE*2);
    inode_sync(cur_part, parent_dir->inode, io_buf);

    /*将新创建目录的inode同步到硬盘*/
    memset(io_buf, 0, SECTOR_SIZE*2);
    inode_sync(cur_part, &new_dir_inode, io_buf);

    bitmap_sync(cur_part, inode_no, INODE_BITMAP);
    sys_free(io_buf);
    dir_close(searched_record.parent_dir);
    return 0;

rollback:
    switch(rollback_step){
        case 2:
            bitmap_set(&cur_part->inode_bitmap, inode_no, 0);
        case 1:
            dir_close(searched_record.parent_dir);
            break;
    }
    sys_free(io_buf);
    return -1;
}

/*打开目录, 成功返回目录指针, 失败返回NULL*/
struct dir* sys_opendir(const char* name){
    ASSERT(strlen(name) < MAX_FILE_NAME_LEN);
    if(name[0] == '/' && (name[1]==0 || name[0]=='.')){
        return &root_dir;
    }

    /*先检查目录是否存在*/
    struct path_search_record searched_record;
    memset(&searched_record, 0, sizeof(struct path_search_record));
    int inode_no = search_file(name, &searched_record);
    struct dir* ret = NULL;
    if(inode_no == -1){
        printk("In %s, sub path %s not exist\n", name, searched_record.searched_path);
    }else{
        if(searched_record.file_type == FT_REGULAR){
            printk("%s is a regular file!\n",name);
        }else if(searched_record.file_type == FT_DIRECTORY){
            ret = dir_open(cur_part, inode_no);
        }
    }
    dir_close(searched_record.parent_dir);
    return ret;
}

/*关闭目录dir, 成功返回0, 失败返回-1*/
int32_t sys_closedir(struct dir* dir){
    int32_t ret = -1;
    if(dir != NULL){    //有啥意义? dir不是NULL但不存在怎么办?
        dir_close(dir);
        ret = 0;
    }
    return ret;
}

/*读目录dir的一个目录项, 成功后返回其目录项地址, 到尾/出错返回NULL*/
struct dir_entry* sys_readdir(struct dir* dir){
    ASSERT(dir != NULL);
    return dir_read(dir);
}

void sys_rewinddir(struct dir* dir){
    dir->dir_pos = 0;
}

/*删除空目录, 成功返回0, 失败返回-1*/
int32_t sys_rmdir(const char* pathname){
    //先检查要删除的目录是否存在
    struct path_search_record searched_record;
    memset(&searched_record, 0, sizeof(struct path_search_record));
    int inode_no = search_file(pathname, &searched_record);
    ASSERT(inode_no != 0);
    int retval = -1;
    if(inode_no == -1){
        printk("sys_rmdir:In %s, sub path %s not exist\n", pathname, searched_record.searched_path);
    }else{
        if(searched_record.file_type == FT_REGULAR){
            printk("sys_rmdir: %s is a regular file\n, use sys_unlink() instead", pathname);
        }else{
            struct dir* dir = dir_open(cur_part, inode_no);
            if(!dir_is_empty(dir)){
                printk("sys_rmdir: dir %s is not empty, not allowed to delete a nonempty directory\n",pathname);
            } else {
                if(dir_remove(searched_record.parent_dir, dir) == 0){
                    retval = 0;
                }
            }
            dir_close(dir);
        }
    }
    dir_close(searched_record.parent_dir);
    return retval;
}

/*在磁盘上搜索文件系统, 若没有则格式化分区创建文件系统*/
void filesys_init(){
    uint8_t channel_no = 0, dev_no, part_idx = 0;
    /*sb_buf用来存储从硬盘上读入的超级块*/
    struct super_block* sb_buf = (struct super_block*)sys_malloc(SECTOR_SIZE);

    if(sb_buf == NULL)
        PANIC("alloc memory failed\n");
    printk("searching filesystem...\n");
    //通道->硬盘->分区
    while (channel_no < channel_cnt){
        while(dev_no < 2){
            if(dev_no == 0){    //跳过hd60M.img
                dev_no++;
                continue;
            }
            struct disk* hd = &channels[channel_no].devices[dev_no];
            struct partition* part = hd->prim_parts;
            while(part_idx < 12){   //4个主分区+8个逻辑分区
                if(part_idx==4){ //开始处理逻辑分区
                    part = hd->logic_parts;
                }
                if(part->sec_cnt != 0){ //如果分区存在
                    memset(sb_buf, 0, SECTOR_SIZE);
                    //读入超级块
                    ide_read(hd, part->start_lba+1, sb_buf, 1);
                    if(sb_buf->magic == 0x20070329){
                        printk("%s has filesystem\n", part->name);
                    }else{
                        printk("formatting %s's partition %s...\n", hd->name, part->name);
                        partition_format(part);
                    }
                }
                part_idx++;
                part++;
            }
            dev_no++;
        }
        channel_no++;
    }
    sys_free(sb_buf);
    char default_part[8] = "sdb1";
    //挂载分区
    list_traversal(&partition_list, mount_partition, (int)default_part);
    /*将当前分区根目录打开*/
    open_root_dir(cur_part);
    /*初始化文件表*/
    uint32_t fd_idx = 0;
    while(fd_idx < MAX_FILE_OPEN)
        file_table[fd_idx++].fd_inode = NULL;
}

