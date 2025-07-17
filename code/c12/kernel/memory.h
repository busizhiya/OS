#ifndef __KERNEL_MEMORY_H
#define __KERNEL_MEMORY_H
#include "../lib/kernel/stdint.h"
#include "../lib/kernel/bitmap.h"

#define MEM_BITMAP_BASE 0xc009a000
enum pool_flags{
    PF_KERNEL = 1,
    PF_USER = 2
};
#define PG_P_1  1
#define PG_P_0  0
#define PG_RW_R 0
#define PG_RW_W 2
#define PG_US_S 0
#define PG_US_U 4



struct virtual_addr{
    struct bitmap vaddr_bitmap; //每个bit代表4K的一页
    uint32_t vaddr_start;
};
void mem_init(void);
void* get_kernel_pages(uint32_t pg_cnt);
void* get_a_page(enum pool_flags pf, uint32_t vaddr);
uint32_t addr_v2p(uint32_t vaddr);

#endif