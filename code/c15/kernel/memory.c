#include "memory.h"
#include "../lib/kernel/stdint.h"
#include "../lib/kernel/print.h"
#include "../lib/kernel/bitmap.h"
#include "debug.h"
#include "../lib/string.h"
#include "../thread/sync.h"
#include "interrupt.h"

#define PG_SIZE 4096

/* 位图地址分配
    0xc009f000  内核主线程栈顶
    0xc009e000  内核主线程pcb
    一个页框的位图可表示128MB内存, 一个页框大小为4KB, 即0x1000
    位图起始地址 0xc009a000
    最多容纳四个页框
*/

/*!!!找到错误了!!*/
#define K_HEAP_START 0xc0100000  //跨越低端1M, 在逻辑上连续

struct pool{    //内存池, 用于管理物理内存
    struct bitmap pool_bitmap;  //物理内存的位图
    uint32_t phy_addr_start;    //物理内存的起始地址
    uint32_t pool_size; //字节容量
    struct lock lock;   //申请内存时需要互斥

};
#define PDE_IDX(addr) ((addr & 0xffc00000) >> 22)
#define PTE_IDX(addr) ((addr & 0x003ff000) >> 12)

struct mem_block_desc k_block_descs[DESC_CNT];  //内核内存块描述符数组
struct pool kernel_pool, user_pool; //物理内存池
struct virtual_addr kernel_vaddr;   //内核虚拟地址池

struct arena{
    struct mem_block_desc* desc;
    uint32_t cnt;   //large为true时, cnt表示页框数, 否则为空闲mem_block数量
    bool large;
};


static void mem_pool_init(uint32_t all_mem){
    put_str("   mem_pool_init start\n");
    lock_init(&kernel_pool.lock);
    lock_init(&user_pool.lock);
    
    //内核占用的空间: 256页
    // 1页目录表 + 1(0/768同一个) + 254(769~1022) = 256
    uint32_t page_table_size = PG_SIZE * 256;
    uint32_t used_mem = page_table_size + 0x100000; //低端1M + 1M后的PDE&PTE
    uint32_t free_mem = all_mem - used_mem; //全局可用空间
    uint16_t all_free_pages = free_mem / PG_SIZE;
    uint16_t kernel_free_pages = all_free_pages / 2;
    uint16_t user_free_pages = all_free_pages - kernel_free_pages;
    //此处不考虑余数
    uint32_t kbm_length = kernel_free_pages / 8;
    uint32_t ubm_length = user_free_pages / 8;
    uint32_t kp_start = used_mem;
    uint32_t up_start = kp_start + kernel_free_pages * PG_SIZE;

    kernel_pool.phy_addr_start = kp_start;
    user_pool.phy_addr_start = up_start;

    kernel_pool.pool_size = kernel_free_pages * PG_SIZE;
    user_pool.pool_size = user_free_pages * PG_SIZE;
    
    kernel_pool.pool_bitmap.btmp_bytes_len = kbm_length;
    user_pool.pool_bitmap.btmp_bytes_len = ubm_length;

    //指定一块内存生成位图
    kernel_pool.pool_bitmap.bits = (void*)MEM_BITMAP_BASE;
    user_pool.pool_bitmap.bits = (void*)(MEM_BITMAP_BASE + kbm_length);

    put_str("   kernel_pool_bitmap_start:");
    put_int((uint32_t)kernel_pool.pool_bitmap.bits);
    put_str(" kernel_pool_phy_addr_start:");
    put_int(kernel_pool.phy_addr_start);
    put_char('\n');
    put_str("   user_pool_bitmap_start:");
    put_int((uint32_t)user_pool.pool_bitmap.bits);
    put_str(" user_pool_phy_addr_start:");
    put_int(user_pool.phy_addr_start);
    put_char('\n');

    /*位图初始化,置为0*/
    bitmap_init(&kernel_pool.pool_bitmap);
    bitmap_init(&user_pool.pool_bitmap);
    kernel_vaddr.vaddr_bitmap.btmp_bytes_len = kbm_length;

    kernel_vaddr.vaddr_bitmap.bits = \
    (void*)(MEM_BITMAP_BASE + kbm_length + ubm_length);

    kernel_vaddr.vaddr_start = K_HEAP_START;
    bitmap_init(&kernel_vaddr.vaddr_bitmap);
    put_str("   mem_pool_init done\n");
}

/*在pf表示的虚拟地址池中申请pg_cnt个虚拟页*/
static void* vaddr_get(enum pool_flags pf,uint32_t pg_cnt){
    int32_t vaddr_start = 0, bit_idx_start = -1;
    uint32_t cnt = 0;
    if(pf==PF_KERNEL){
        bit_idx_start = bitmap_scan(&kernel_pool.pool_bitmap, pg_cnt);
        if(bit_idx_start==-1)
            return NULL;
        while (cnt < pg_cnt)
            bitmap_set(&kernel_pool.pool_bitmap, bit_idx_start + cnt++, 1);
        vaddr_start = kernel_vaddr.vaddr_start + bit_idx_start * PG_SIZE;
    }
    else{
        struct task_struct* cur = running_thread();
        bit_idx_start = bitmap_scan(&cur->userprog_vaddr.vaddr_bitmap,pg_cnt);
        if(bit_idx_start == -1)
            return NULL;
        while(cnt < pg_cnt){
            bitmap_set(&cur->userprog_vaddr.vaddr_bitmap,bit_idx_start + cnt++, 1);
        }
        vaddr_start = cur->userprog_vaddr.vaddr_start + bit_idx_start * PG_SIZE;
        ASSERT((uint32_t)vaddr_start < (0xc0000000 - PG_SIZE));
    }
    return (void*) vaddr_start;
}

/*将虚拟地址vaddr转换为对应的pte指针(指向pte页表)*/
uint32_t* pte_ptr(uint32_t vaddr){
    /*  1.PDE最后一项, 指向PDE自己
        2.PDE+pde索引指向对应的页目录项(页表pte)
        3.pte索引获得对应指针 */
    return (uint32_t*)(0xffc00000 + \
    ((vaddr & 0xffc00000)>>10) +\
    PTE_IDX(vaddr) * 4);
}

uint32_t* pde_ptr(uint32_t vaddr){
    return (uint32_t*)(0xfffff000 + PDE_IDX(vaddr) * 4);
}

/* 在m_pool指向的物理内存池中分配一个物理页
    成功则返回页框的物理地址, 失败返回NULL*/
static void* palloc(struct pool* m_pool){
    int bit_idx = bitmap_scan(&m_pool->pool_bitmap, 1); //找一个物理页
    if(bit_idx == -1) return NULL;
    bitmap_set(&m_pool->pool_bitmap, bit_idx, 1);
    uint32_t page_phyaddr = ((bit_idx * PG_SIZE) + m_pool->phy_addr_start);
    return (void*) page_phyaddr;
}

/*在页表中添加虚拟地址和物理地址的映射*/
static void page_table_add(void* _vaddr, void* _page_phyaddr){
    uint32_t vaddr = (uint32_t)_vaddr, page_phyaddr = (uint32_t)_page_phyaddr;
    uint32_t* pde = pde_ptr(vaddr);
    uint32_t* pte = pte_ptr(vaddr);
    // D_put_str("At page_table_add vaddr=");
    // D_put_int(vaddr);
    // D_put_str(" , phyaddr=");
    // D_put_int(page_phyaddr);
    // D_put_char('\n');
    // D_put_str("pde=");
    // D_put_int((uint32_t)pde);
    // D_put_str(" pte=");
    // D_put_int((uint32_t)pte);
    // D_put_char('\n');
    if(*pde & 0x00000001){//页目录项存在
        //新添加的页表项不应该存在
        ASSERT(!(*pte & 0x00000001));
        if(!(*pte & 0x00000001)){
            *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
        } else {
            PANIC("pte repeat");
            *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
        }
    }else{
        //页目录项不存在, 先分配一个页作为页表, 再创建页表项
        uint32_t pde_phyaddr = (uint32_t)palloc(&kernel_pool);
        *pde = (pde_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
        memset((void*)((int)pte & 0xfffff000), 0, PG_SIZE);
        
        ASSERT(!(*pte & 0x00000001));
        *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
    }
}

/*从pf对应的虚拟地址池中分配pg_cnt个页空间, 成功则返回起始虚拟地址, 失败返回NULL*/
void* malloc_page(enum pool_flags pf, uint32_t pg_cnt){
    ASSERT(pg_cnt > 0 && pg_cnt < 3840);
    void* vaddr_start = vaddr_get(pf,pg_cnt);
    if(vaddr_start == NULL) //虚拟地址申请失败
        return NULL;

    uint32_t vaddr = (uint32_t)vaddr_start, cnt = pg_cnt;
    struct pool* mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;
    while(cnt-- > 0){
        void* page_phyaddr = palloc(mem_pool);
        if(page_phyaddr == NULL){
            //申请失败, 要把之前申请的虚拟地址和物理页全部回收
            //暂未实现
            return NULL;
        }
        page_table_add((void*)vaddr, page_phyaddr);
        vaddr += PG_SIZE;
    }
    return vaddr_start;
}

/*从内核物理地址池中申请pg_cnt页内存*/
void* get_kernel_pages(uint32_t pg_cnt){
    void* vaddr = malloc_page(PF_KERNEL, pg_cnt);
    if(vaddr != NULL)   //申请成功, 清空内存旧内容
        memset(vaddr, 0, pg_cnt * PG_SIZE);
    D_put_str("get_kernel_pages from ");
    D_put_str(running_thread()->name);
    D_put_str(", vaddr=");
    D_put_int((uint32_t)vaddr);
    D_put_str(", size=0x");
    D_put_int(pg_cnt * PG_SIZE);
    D_put_char('\n');
    return vaddr;
}
/*从用户物理地址池中申请pg_cnt页内存*/
void* get_user_pages(uint32_t pg_cnt){
    lock_acquire(&user_pool.lock);
    void* vaddr = malloc_page(PF_USER, pg_cnt);
    memset(vaddr, 0, pg_cnt * PG_SIZE);
    lock_release(&user_pool.lock);
    return vaddr;
}

/* 将地址vaddr与pf池中的物理地址关联, 仅支持一页空间分配
    区别是: 可以指定虚拟地址vaddr,并为其分配物理内存*/
void* get_a_page(enum pool_flags pf, uint32_t vaddr){
    struct pool* mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;
    lock_acquire(&mem_pool->lock);

    struct task_struct* cur = running_thread();
    int32_t bit_idx = -1;
    /*如果是用户进程申请用户内存, 就修改用户进程自己的虚拟地址位图*/
    if(cur->pgdir != NULL && pf == PF_USER){
        bit_idx = (vaddr - cur->userprog_vaddr.vaddr_start) / PG_SIZE;
        ASSERT(bit_idx > 0);
        bitmap_set(&cur->userprog_vaddr.vaddr_bitmap, bit_idx, 1);
        /*如果是内核线程申请内核内存, 就修改kernel_vaddr*/
    } else if(cur->pgdir == NULL && pf == PF_KERNEL){
        bit_idx = (vaddr - kernel_vaddr.vaddr_start) / PG_SIZE;
        ASSERT(bit_idx > 0);
        bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx, 1);
    }else{
        PANIC("get_a_page: kernel allocates userspace or user allocates kernel space by get_a_page are not allowed");
    }
    void* page_phyaddr = palloc(mem_pool);
    if(page_phyaddr == NULL)
        return NULL;
    page_table_add((void*)vaddr, page_phyaddr);
    lock_release(&mem_pool->lock);
    return (void*)vaddr;
}

/*得到虚拟地址映射到的物理地址*/
uint32_t addr_v2p(uint32_t vaddr){
    uint32_t* pte = pte_ptr(vaddr);
    return ((*pte & 0xfffff000) + (vaddr & 0x00000fff));
}

/*为malloc做准备*/
void block_desc_init(struct mem_block_desc* desc_array){
    uint16_t desc_idx, block_size=16;
    /*初始化每个mem_block_desc描述符*/
    for(desc_idx = 0; desc_idx < DESC_CNT; desc_idx++){
        desc_array[desc_idx].block_size = block_size;
        desc_array[desc_idx].blocks_per_arena = \
        (PG_SIZE - sizeof(struct arena)) / block_size;
        list_init(&desc_array[desc_idx].free_list);
        block_size *=2;
    }
}

static struct mem_block* arena2block(struct arena* a, uint32_t idx){
    return (struct mem_block*)\
    ((uint32_t)a + sizeof(struct arena) + idx * a->desc->block_size);
}

static struct arena* block2arena(struct mem_block* b){
    return (struct arena*)((uint32_t)b & 0xfffff000);
}

/*申请内存*/
void* sys_malloc(uint32_t size){
    enum pool_flags PF;
    struct pool* mem_pool;
    uint32_t pool_size;
    struct mem_block_desc* descs;
    struct task_struct* cur_thread = running_thread();
    if(cur_thread->pgdir == NULL){
        //内核线程
        PF = PF_KERNEL;
        pool_size = kernel_pool.pool_size;
        mem_pool = &kernel_pool;
        descs = k_block_descs;
    }else{
        PF = PF_USER;
        pool_size = user_pool.pool_size;
        mem_pool = &user_pool;
        descs = cur_thread->u_block_descs;
    }
    if(!(size>0 && size<pool_size))
        return NULL;

    struct arena* a;
    struct mem_block* b;
    lock_acquire(&mem_pool->lock);
    if(size > 1024){    //分配页框
        uint32_t page_cnt = \
        DIV_ROUND_UP(size + sizeof(struct arena),PG_SIZE);
        a = malloc_page(PF, page_cnt);
        if(a!= NULL){
            memset(a, 0, page_cnt*PG_SIZE);
            a->desc = NULL; //大块页框, desc为NULL
            a->cnt = page_cnt;
            a->large = true;
            lock_release(&mem_pool->lock);
            return (void*)(a+1);//跳过arena元信息字段
        }else{
            lock_release(&mem_pool->lock);
            return NULL;
        }
    }else{  //size≤1024
        uint8_t desc_idx;
        //从小到大,找到第一个能容纳size的arena类型
        for(desc_idx = 0; desc_idx < DESC_CNT; desc_idx++){
            if(size<= descs[desc_idx].block_size)
                break;
        }
        //free_list已经没有可用的mem_block, 创建新的arena
        if(list_empty(&descs[desc_idx].free_list)){
            a = malloc_page(PF, 1);
            if(a == NULL){
                lock_release(&mem_pool->lock);
                return NULL;
            }
            memset(a, 0, PG_SIZE);
            a->desc = &descs[desc_idx];
            a->large = false;
            a->cnt = descs[desc_idx].blocks_per_arena;
            uint32_t block_idx;

            enum intr_status old_status = intr_disable();
            for(block_idx = 0;\
            block_idx < descs[desc_idx].blocks_per_arena; block_idx++){
                b = arena2block(a, block_idx);
                ASSERT(!elem_find(&a->desc->free_list,&b->free_elem));
                list_append(&a->desc->free_list,&b->free_elem);
            }
            intr_set_status(old_status);
        }
        /*开始分配内存块*/
        b = elem2entry(struct mem_block, free_elem, list_pop(&(descs[desc_idx].free_list)));
        a = block2arena(b);
        a->cnt--;
        lock_release(&mem_pool->lock);
        return (void*)b;

    }

}

/*将物理地址pg_pht_addr回收,物理页对应bitmap置0*/
void pfree(uint32_t pg_phy_addr){
    struct pool* mem_pool;
    uint32_t bit_idx = 0;
    if(pg_phy_addr >= user_pool.phy_addr_start){
        //用户物理内存池
        mem_pool = &user_pool;
        bit_idx = (pg_phy_addr - user_pool.phy_addr_start) / PG_SIZE;
    }else{
        //内核物理内存池
        mem_pool = &kernel_pool;
        bit_idx = (pg_phy_addr - kernel_pool.phy_addr_start) / PG_SIZE;
    }
    bitmap_set(&mem_pool->pool_bitmap, bit_idx, 0);
}

/*去掉页表中虚拟地址vaddr的映射, 只去掉vaddr对应的pte, P位置0*/
static void page_table_pte_remove(uint32_t vaddr){
    uint32_t* pte = pte_ptr(vaddr);
    *pte &= ~PG_P_1;
    asm volatile ("invlpg %0"::"m"(vaddr):"memory");//更新tlb
}


/*在虚拟地址池中释放以_vaddr起始的连续的pg_cnt个虚拟页地址*/
static void vaddr_remove(enum pool_flags pf, void* _vaddr, uint32_t pg_cnt){
    uint32_t bit_idx_start= 0, vaddr = (uint32_t)_vaddr, cnt = 0;

    if(pf == PF_KERNEL){
        bit_idx_start = (vaddr - kernel_vaddr.vaddr_start) / PG_SIZE;
        while(cnt < pg_cnt){
            bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 0);
        }
    }else{
        struct task_struct* cur_thread = running_thread();
        bit_idx_start = (vaddr - cur_thread->userprog_vaddr.vaddr_start) / PG_SIZE;
        while(cnt < pg_cnt){
            bitmap_set(&cur_thread->userprog_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 0);
        }
    }
}


/*释放以虚拟地址vaddr为起始的cnt个物理页框*/
void mfree_page(enum pool_flags pf, void* _vaddr, uint32_t pg_cnt){
    uint32_t pg_phy_addr;
    uint32_t vaddr = (uint32_t)_vaddr, page_cnt = 0;
    ASSERT(pg_cnt >= 1 && vaddr % PG_SIZE == 0);
    pg_phy_addr = addr_v2p(vaddr);
    /*低端1M+1KB页目录表+1KB页表*/
    ASSERT((pg_phy_addr % PG_SIZE == 0) && pg_phy_addr >= 0x102000);
    
    //用户内存也可以申请内核空间吗?不然为什么不用PF直接判断?
    if(pg_phy_addr >= user_pool.phy_addr_start){
        vaddr -= PG_SIZE;
        while(page_cnt < pg_cnt){
            vaddr += PG_SIZE;
            pg_phy_addr = addr_v2p(vaddr);
            //确保物理地址属于用户物理内存池
            ASSERT((pg_phy_addr % PG_SIZE) == 0 && pg_phy_addr >= user_pool.phy_addr_start);
            pfree(pg_phy_addr);
            page_table_pte_remove(vaddr);
            page_cnt++;
        }
        vaddr_remove(pf, _vaddr, pg_cnt);
    }else{
        vaddr -= PG_SIZE;
        while (page_cnt<pg_cnt){
            vaddr += PG_SIZE;
            pg_phy_addr = addr_v2p(vaddr);
            ASSERT((pg_phy_addr % PG_SIZE) == 0 && \
                pg_phy_addr >= kernel_pool.phy_addr_start &&\
                pg_phy_addr < user_pool.phy_addr_start);
            pfree(pg_phy_addr);
            page_table_pte_remove(vaddr);
            page_cnt++;
        }
        vaddr_remove(pf, _vaddr, pg_cnt);
    }
}

void sys_free(void* ptr){
    ASSERT(ptr != NULL);
    if(ptr != NULL){
        enum pool_flags PF;
        struct pool* mem_pool;
        if(running_thread()->pgdir == NULL){
            //内核线程
            ASSERT((uint32_t)ptr >= K_HEAP_START);
            PF = PF_KERNEL;
            mem_pool = &kernel_pool;
        }else{
            PF = PF_USER;
            mem_pool = &user_pool;
        }

        lock_acquire(&mem_pool->lock);
        struct mem_block* b = ptr;
        struct arena* a = block2arena(b);
        if(a->desc == NULL && a->large == true){
            //大于1024的内存, 直接释放页框
            mfree_page(PF, a, a->cnt);
        }else{
            //回收到可用内存free_list
            list_append(&a->desc->free_list, &b->free_elem);
        
            //整个arena的内存块都空闲, 释放arena
            if(++a->cnt == a->desc->blocks_per_arena){
                uint32_t block_idx;
                for(block_idx = 0; block_idx < a->desc->blocks_per_arena; block_idx++){
                    struct mem_block* b = arena2block(a, block_idx);
                    ASSERT(elem_find(&a->desc->free_list, &b->free_elem));
                    list_remove(&b->free_elem);
                }
            }
        }
        lock_release(&mem_pool->lock);

    }
}

/*安装1页大小的vaddr, 专门针对fork时虚拟地址位图无需操作的情况*/
void* get_a_page_without_opvaddrbitmap(enum pool_flags pf, uint32_t vaddr){
    struct pool* mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;
    lock_acquire(&mem_pool->lock);
    void* page_phyaddr = palloc(mem_pool);
    if(page_phyaddr == NULL){
        lock_release(&mem_pool->lock);
        return NULL;
    }
    page_table_add((void*)vaddr, page_phyaddr);
    lock_release(&mem_pool->lock);
    return (void*)vaddr;
    
}


void mem_init(){
    put_str("mem_init start\n");
    uint32_t mem_bytes_total =*((uint32_t*)(0xb00));
    mem_pool_init(mem_bytes_total);
    block_desc_init(k_block_descs);
    put_str("mem_init done\n");
}