#ifndef __FS_FS_H
#define __FS_FS_H
#include "../lib/kernel/stdint.h"
#include "../lib/kernel/list.h"

#define MAX_FILES_PER_PART  4096
//每个分区最大文件数

#define BITS_PER_SECTOR 4096
#define SECTOR_SIZE 512
#define BLOCK_SIZE SECTOR_SIZE

enum file_types{
    FT_UNKNOWN,
    FT_REGULAR,
    FT_DIRECTORY
};
void filesys_init();
static bool mount_partition(struct list_elem* pelem, int arg);



#endif