#ifndef __KERNEL_MEMORY_H
#define __KERNEL_MEMORY_H
#include "../lib/kernel/stdint.h"
#include "../lib/kernel/bitmap.h"
#include "../lib/kernel/list.h"

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
#define DESC_CNT 7  //目前只有其中内存块描述符种类  16~1024



struct virtual_addr{
    struct bitmap vaddr_bitmap; //每个bit代表4K的一页
    uint32_t vaddr_start;
};


/*内存块*/
struct mem_block{
    struct list_elem free_elem;
};

/*内存块描述符*/
struct mem_block_desc{
    uint32_t block_size;    
    uint32_t blocks_per_arena;
    struct list free_list;  //可用的mem_block链表
};

void mem_init(void);
void* get_kernel_pages(uint32_t pg_cnt);
void* get_a_page(enum pool_flags pf, uint32_t vaddr);
uint32_t addr_v2p(uint32_t vaddr);
void block_desc_init(struct mem_block_desc* desc_array);
void* sys_malloc(uint32_t size);
void sys_free(void* ptr);
void* get_a_page_without_opvaddrbitmap(enum pool_flags pf, uint32_t vaddr);
void mfree_page(enum pool_flags pf, void* _vaddr, uint32_t pg_cnt);
uint32_t* pte_ptr(uint32_t vaddr);
uint32_t* pde_ptr(uint32_t vaddr);
void free_a_phy_page(uint32_t pg_phy_addr);

#endif