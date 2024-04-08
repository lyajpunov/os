#include "memory.h"
#include "bitmap.h"
#include "stdin.h"
#include "global.h"
#include "assert.h"
#include "print.h"
#include "string.h"
#include "sync.h"
#include "interrupt.h"

/* 内存池结构 */
struct pool {
    struct bitmap pool_bitmap; // 本内存池用到的位图结构,用于管理物理内存
    uint32_t phy_addr_start;   // 本内存池所管理物理内存的起始地址
    uint32_t pool_size;		   // 本内存池字节容量
    struct lock lock;          // 申请内存时互斥
};

// 拿到addr的前10bit，也就是页目录的索引
#define PDE_IDX(addr) ((addr & 0xffc00000) >> 22)
// 拿到addr的中10bit，也就是页表的索引
#define PTE_IDX(addr) ((addr & 0x003ff000) >> 12)


struct mem_block_desc k_block_descs[DESC_CNT];	// 内核内存块描述符数组,其中规格，最小16Byte
struct pool kernel_pool, user_pool;             // 生成内核内存池和用户内存池
struct virtual_addr kernel_vaddr;	            // 此结构是用来给内核分配虚拟地址

/* 在pf表示的虚拟地址池中同时取出pg_cnt个连续的页，成功返回虚拟地址，失败返回NULL */
static void* vaddr_get(enum pool_flags pf, uint32_t pg_cnt) {
    int vaddr_start = 0, bit_idx_start = -1;
    uint32_t cnt = 0;
    if (pf == PF_KERNEL) {
        bit_idx_start = bitmap_scan(&kernel_vaddr.vaddr_bitmap, pg_cnt);
        if (bit_idx_start == -1) {
            return NULL;
        }
        while (cnt < pg_cnt) {
            bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 1);
        }
        vaddr_start = kernel_vaddr.vaddr_start + bit_idx_start * PG_SIZE;
    }
    else {
        struct task_struct* cur = running_thread();
        bit_idx_start = bitmap_scan(&cur->userprog_vaddr.vaddr_bitmap, pg_cnt);
        if (bit_idx_start == -1) {
            return NULL;
        }

        while (cnt < pg_cnt) {
            bitmap_set(&cur->userprog_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 1);
        }
        vaddr_start = cur->userprog_vaddr.vaddr_start + bit_idx_start * PG_SIZE;

        /* (0xc0000000 - PG_SIZE)做为用户3级栈已经在start_process被分配 */
        ASSERT((uint32_t)vaddr_start < (0xc0000000 - PG_SIZE));

    }
    return (void*)vaddr_start;
}

/* 得到虚拟地址vaddr对应的pte指针*/
uint32_t* pte_ptr(uint32_t vaddr) {
    /* 先访问到页表自己，也就是前10bit全是1
     * 再用页目录项pde(页目录内页表的索引)做为pte的索引访问到页表
     * 再用pte的索引做为页内偏移, */
    uint32_t* pte = (uint32_t*)(0xffc00000 + ((vaddr & 0xffc00000) >> 10) + PTE_IDX(vaddr) * 4);
    return pte;
}

/* 得到虚拟地址vaddr对应的pde的指针 */
uint32_t* pde_ptr(uint32_t vaddr) {
    /* 0xfffff是用来访问到页表本身所在的地址 */
    uint32_t* pde = (uint32_t*)((0xfffff000) + PDE_IDX(vaddr) * 4);
    return pde;
}

/**
 * @description: 在m_pool指向的物理内存池中分配1个物理页,成功则返回页框的物理地址,失败则返回NULL
 * @param {pool*} m_pool 内存池（内核内存池，用户内存池）
 * @return {*}
 */
static void* palloc(struct pool* m_pool) {
    int bit_idx = bitmap_scan(&m_pool->pool_bitmap, 1);    // 找一个物理页面
    if (bit_idx == -1) {
        return NULL;
    }
    bitmap_set(&m_pool->pool_bitmap, bit_idx, 1);	      // 将此位bit_idx置1
    uint32_t page_phyaddr = ((bit_idx * PG_SIZE) + m_pool->phy_addr_start);
    return (void*)page_phyaddr;
}

/**
 * @description: 页表中添加虚拟地址_vaddr与物理地址_page_phyaddr的映射
 * @param {void*} _vaddr 虚拟地址
 * @param {void*} _page_phyaddr 物理地址
 * @return {*}
 */
static void page_table_add(void* _vaddr, void* _page_phyaddr) {
    uint32_t vaddr = (uint32_t)_vaddr;
    uint32_t page_phyaddr = (uint32_t)_page_phyaddr;
    uint32_t* pde = pde_ptr(vaddr);
    uint32_t* pte = pte_ptr(vaddr);

    /* 先在页目录内判断目录项的P位，若为1,则表示该表已存在 */
    if (*pde & 0x00000001) {	      // 页目录项和页表项的第0位为P,此处判断目录项是否存在
        ASSERT(!(*pte & 0x00000001)); // 确保pte的最后一位为0，也就是这一位并未使用
        *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
    }
    else {  // 页目录项不存在,所以要先创建页目录再创建页表项.
        uint32_t pde_phyaddr = (uint32_t)palloc(&kernel_pool); // 先申请一页物理内存当做页表
        *pde = (pde_phyaddr | PG_US_U | PG_RW_W | PG_P_1);     // 将相应的pde绑定到申请的物理内存上
        memset((void*)((int)pte & 0xfffff000), 0, PG_SIZE);    // 将申请到的物理内存全部清0，避免旧数据的影响
        ASSERT(!(*pte & 0x00000001));                          // 确保pte的最后一位为0，也就是这一位并未使用
        *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);    // 写入pte完成绑定
    }
}

/* 分配pg_cnt个页空间,成功则返回起始虚拟地址,失败时返回NULL */
void* malloc_page(enum pool_flags pf, uint32_t pg_cnt) {
    if (pg_cnt <= 0 || pg_cnt > 3840) {
        put_str("malloc_page error: pg_cnt error!\n");
        return NULL;
    }

    void* vaddr_start = vaddr_get(pf, pg_cnt);
    if (vaddr_start == NULL) {
        put_str("malloc_page error: vaddr_get error!\n");
        return NULL;
    }

    uint32_t vaddr = (uint32_t)vaddr_start, cnt = pg_cnt;
    struct pool* mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;

    /* 因为虚拟地址是连续的,但物理地址可以是不连续的,所以逐个做映射*/
    while (cnt-- > 0) {
        void* page_phyaddr = palloc(mem_pool);
        // 失败时要将曾经已申请的虚拟地址和物理页全部回滚 #TODO
        if (page_phyaddr == NULL) {
            put_str("malloc_page error: palloc error!\n");
            return NULL;
        }
        page_table_add((void*)vaddr, page_phyaddr); // 在页表中做映射 
        vaddr += PG_SIZE;		                    // 下一个虚拟页
    }
    return vaddr_start;
}

/* 从内核物理内存池中申请pg_cnt页内存,成功则返回其虚拟地址,失败则返回NULL */
void* get_kernel_pages(uint32_t pg_cnt) {
    lock_acquire(&kernel_pool.lock);
    void* vaddr = malloc_page(PF_KERNEL, pg_cnt);
    if (vaddr != NULL) {
        memset(vaddr, 0, pg_cnt * PG_SIZE);
    }
    else {
        put_str("get_kernel_pages error: vaddr error!\n");
    }
    lock_release(&kernel_pool.lock);
    return vaddr;
}

/* 从内核物理内存池中申请pg_cnt页内存,并返回其虚拟地址 */
void* get_user_pages(uint32_t pg_cnt) {
    lock_acquire(&user_pool.lock);
    void* vaddr = malloc_page(PF_USER, pg_cnt);
    memset(vaddr, 0, pg_cnt * PG_SIZE);
    lock_release(&user_pool.lock);
    return vaddr;
}

/* 将地址vaddr与pf池中的物理地址关联,仅支持一页空间分配 */
void* get_a_page(enum pool_flags pf, uint32_t vaddr) {
    struct pool* mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;
    lock_acquire(&mem_pool->lock);

    /* 先将虚拟地址对应的位图置1 */
    struct task_struct* cur = running_thread();
    int32_t bit_idx = -1;

    /* 若当前是用户进程申请用户内存,就修改用户进程自己的虚拟地址位图 */
    if (cur->pgdir != NULL && pf == PF_USER) {
        bit_idx = (vaddr - cur->userprog_vaddr.vaddr_start) / PG_SIZE;
        ASSERT(bit_idx > 0);
        bitmap_set(&cur->userprog_vaddr.vaddr_bitmap, bit_idx, 1);

    }
    else if (cur->pgdir == NULL && pf == PF_KERNEL) {
        /* 如果是内核线程申请内核内存,就修改kernel_vaddr. */
        bit_idx = (vaddr - kernel_vaddr.vaddr_start) / PG_SIZE;
        ASSERT(bit_idx > 0);
        bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx, 1);
    }
    else {
        PANIC("get_a_page:not allow kernel alloc userspace or user alloc kernelspace by get_a_page");
    }

    void* page_phyaddr = palloc(mem_pool);
    if (page_phyaddr == NULL) {
        return NULL;
    }
    page_table_add((void*)vaddr, page_phyaddr);
    lock_release(&mem_pool->lock);
    return (void*)vaddr;
}

/**
 * @description: 安装1页大小的vaddr,专门针对fork时虚拟地址位图无须操作的情况
 * @param {enum pool_flags} pf 内存池标志
 * @param {uint32_t} vaddr 虚拟地址
 * @return {*} 虚拟地址
 */
void* get_a_page_without_opvaddrbitmap(enum pool_flags pf, uint32_t vaddr) {
    struct pool* mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;
    lock_acquire(&mem_pool->lock);
    void* page_phyaddr = palloc(mem_pool);
    if (page_phyaddr == NULL) {
        lock_release(&mem_pool->lock);
        return NULL;
    }
    page_table_add((void*)vaddr, page_phyaddr);
    lock_release(&mem_pool->lock);
    return (void*)vaddr;
}

/**
 * @description: 得到虚拟地址映射到的物理地址
 * @param {uint32_t} vaddr 虚拟地址
 * @return {*} 物理地址
 */
uint32_t addr_v2p(uint32_t vaddr) {
    uint32_t* pte = pte_ptr(vaddr);
    /* (*pte)的值是页表所在的物理页框地址,去掉其低12位的页表项属性+虚拟地址vaddr的低12位 */
    return ((*pte & 0xfffff000) + (vaddr & 0x00000fff));
}


/* 初始化内存池 */
static void mem_pool_init(uint32_t all_mem) {
    put_str("----mem_pool_init start\n");

    uint32_t page_table_size = PG_SIZE * 256;	  // 1页的页目录表+第0和第768个页目录项指向同一个页表
    // 第769~1022个页目录项共指向254个页表,共256个页框
    uint32_t used_mem = page_table_size + 0x100000;	  // 使用的内存，也就是低端1MB+128KB
    uint32_t free_mem = all_mem - used_mem;           // 空闲的内存
    uint16_t all_free_pages = free_mem / PG_SIZE;     // 空闲了多少页 
    uint16_t kernel_free_pages = all_free_pages / 2;                  // 内核空间拿一半空闲的页
    uint16_t user_free_pages = all_free_pages - kernel_free_pages;    // 用户空间拿一半空闲的页
    uint32_t kbm_length = kernel_free_pages / 8;		  // 内核空间bitmap的长度
    uint32_t ubm_length = user_free_pages / 8;			  // 用户空间bitmap的长度
    uint32_t kp_start = used_mem;			        	  // 内核空间的起始地址
    uint32_t up_start = kp_start + kernel_free_pages * PG_SIZE;	  // 用户空间的起始地址

    kernel_pool.phy_addr_start = kp_start;     // 内核内存池物理地址的起始
    user_pool.phy_addr_start = up_start;     // 用户内存池物理地址的起始

    kernel_pool.pool_size = kernel_free_pages * PG_SIZE;  // 内核内存池的大小
    user_pool.pool_size = user_free_pages * PG_SIZE;     // 用户内存池的大小

    kernel_pool.pool_bitmap.btmp_bytes_len = kbm_length;  // 内核内存池位图的长度
    user_pool.pool_bitmap.btmp_bytes_len = ubm_length;    // 用户内存池位图的长度

    kernel_pool.pool_bitmap.bits = (void*)MEM_BITMAP_BASE;               // 内核内存池位图的起始地址
    user_pool.pool_bitmap.bits = (void*)(MEM_BITMAP_BASE + kbm_length);  // 用户内存池位图的起始地址

    bitmap_init(&kernel_pool.pool_bitmap);  // 内核内存池位图初始化
    bitmap_init(&user_pool.pool_bitmap);    // 用户内存池位图初始化

    /******************** 输出内核内存池和用户内存池信息(物理地址) **********************/
    put_str("--------kernel_pool_bitmap_start:");put_hex((int)kernel_pool.pool_bitmap.bits);put_str("\n");
    put_str("--------kernel_pool_phy_addr_start:");put_hex(kernel_pool.phy_addr_start);put_str("\n");
    put_str("--------user_pool_bitmap_start:");put_hex((int)user_pool.pool_bitmap.bits);put_str("\n");
    put_str("--------user_pool_phy_addr_start:");put_hex(user_pool.phy_addr_start);put_str("\n");

    /* 下面初始化内核虚拟地址的位图,按实际物理内存大小生成数组。*/
    kernel_vaddr.vaddr_bitmap.btmp_bytes_len = kbm_length;
    /* 位图的数组指向一块未使用的内存,目前定位在内核内存池和用户内存池之外*/
    kernel_vaddr.vaddr_bitmap.bits = (void*)(MEM_BITMAP_BASE + kbm_length + ubm_length);
    /* 虚拟地址跨过低端的1MB以及页表的128KB */
    kernel_vaddr.vaddr_start = K_HEAP_START;
    bitmap_init(&kernel_vaddr.vaddr_bitmap);
    bitmap_init(&user_pool.pool_bitmap);

    lock_init(&kernel_pool.lock);
    lock_init(&user_pool.lock);

    put_str("----mem_pool_init done\n");
}

/**
 * @description: 内存块描述符数组初始化
 * @param {mem_block_desc*} desc_array 内存块描述符数组
 * @return {*}
 */
void block_desc_init(struct mem_block_desc* desc_array) {
    uint32_t block_size = 16;
    // 初始化每个mem_block_desc描述符
    for (uint32_t desc_idx = 0; desc_idx < DESC_CNT; desc_idx++) {
        desc_array[desc_idx].block_size = block_size;
        // 初始化arena中的内存块数量
        desc_array[desc_idx].blocks_per_arena = (PG_SIZE - sizeof(struct arena)) / block_size;
        // 初始化每个描述符的空闲块链表
        list_init(&(desc_array[desc_idx].free_list));
        // 更新为下一个规格内存块
        block_size *= 2;
    }
}

/* 返回arena中第idx个内存块的地址 */
static struct mem_block* arena2block(struct arena* a, uint32_t idx) {
    return (struct mem_block*)((uint32_t)a + sizeof(struct arena) + idx * a->desc->block_size);
}

/* 返回内存块b所在的arena地址 */
static struct arena* block2arena(struct mem_block* b) {
    return (struct arena*)((uint32_t)b & 0xfffff000);
}

/* 在堆中申请size字节内存 */
void* sys_malloc(uint32_t size) {
    enum pool_flags PF;        // 线程标识
    struct pool* mem_pool;     // 内核内存池或者用户内存池
    uint32_t pool_size;        // 内存池大小
    struct mem_block_desc* descs; // 内存块描述符
    struct task_struct* cur_thread = running_thread();

    if (cur_thread->pgdir == NULL) {     // 若为内核线程
        PF = PF_KERNEL;
        pool_size = kernel_pool.pool_size;
        mem_pool = &kernel_pool;
        descs = k_block_descs;
    }
    else {				                 // 若为用户线程
        PF = PF_USER;
        pool_size = user_pool.pool_size;
        mem_pool = &user_pool;
        descs = cur_thread->u_block_desc;
    }

    if (!(size > 0 && size < pool_size)) { // 若申请的内存不在内存池容量范围内则直接返回NULL
        return NULL;
    }

    struct arena* a;         // 内存仓库元信息
    struct mem_block* b;     // 内存块
    lock_acquire(&mem_pool->lock);

    if (size > 1024) {
        // 超过最大内存块1024, 就分配页框，需要的页框数为申请内存大小+内存块元信息
        uint32_t page_cnt = DIV_ROUND_UP(size + sizeof(struct arena), PG_SIZE);

        a = malloc_page(PF, page_cnt);

        if (a != NULL) {
            memset(a, 0, page_cnt * PG_SIZE);	 // 将分配的内存清0  

            /* 对于分配的大块页框,将desc置为NULL, cnt置为页框数,large置为true */
            a->desc = NULL;
            a->cnt = page_cnt;
            a->large = true;
            lock_release(&mem_pool->lock);
            return (void*)(a + 1);		 // 跨过arena大小，把剩下的内存返回
        }
        else {
            lock_release(&mem_pool->lock);
            return NULL;
        }
    }
    else {
        // 若申请的内存小于等于1024,可在各种规格的mem_block_desc中去适配
        uint8_t desc_idx;

        // 从内存块描述符中匹配合适的内存块规格
        for (desc_idx = 0; desc_idx < DESC_CNT; desc_idx++) {
            if (size <= descs[desc_idx].block_size) {  // 从小往大后,找到后退出
                break;
            }
        }

        // 若mem_block_desc的free_list中已经没有可用的mem_block, 就创建新的arena提供mem_block
        if (list_empty(&descs[desc_idx].free_list)) {
            // 分配1页框做为arena
            a = malloc_page(PF, 1);
            if (a == NULL) {
                lock_release(&mem_pool->lock);
                return NULL;
            }
            memset(a, 0, PG_SIZE);

            // 对于分配的小块内存,将desc置为相应内存块描述符,cnt置为此arena可用的内存块数,large置为false
            a->desc = &descs[desc_idx];
            a->large = false;
            a->cnt = descs[desc_idx].blocks_per_arena;
            uint32_t block_idx;

            enum intr_status old_status = intr_disable();

            // 开始将arena拆分成内存块,并添加到内存块描述符的free_list中
            for (block_idx = 0; block_idx < descs[desc_idx].blocks_per_arena; block_idx++) {
                b = arena2block(a, block_idx);
                ASSERT(!elem_find(&a->desc->free_list, &b->free_elem));
                list_append(&a->desc->free_list, &b->free_elem);
            }
            intr_set_status(old_status);
        }

        /* 开始分配内存块 */
        b = elem2entry(struct mem_block, free_elem, list_pop(&(descs[desc_idx].free_list)));
        memset(b, 0, descs[desc_idx].block_size);

        a = block2arena(b);  // 获取内存块b所在的arena
        a->cnt--;		     // 将此arena中的空闲内存块数减1
        lock_release(&mem_pool->lock);
        return (void*)b;
    }
}

/* 将物理地址pg_phy_addr回收到物理内存池,这里的回收以页为单位 */
void pfree(uint32_t pg_phy_addr) {
    struct pool* mem_pool;
    uint32_t bit_idx = 0;
    if (pg_phy_addr >= user_pool.phy_addr_start) {         // 用户物理内存池
        mem_pool = &user_pool;
        bit_idx = (pg_phy_addr - user_pool.phy_addr_start) / PG_SIZE;
    }
    else {	                                               // 内核物理内存池
        mem_pool = &kernel_pool;
        bit_idx = (pg_phy_addr - kernel_pool.phy_addr_start) / PG_SIZE;
    }
    bitmap_set(&mem_pool->pool_bitmap, bit_idx, 0);	 // 将位图中该位清0
}

/* 去掉页表中虚拟地址vaddr的映射,只去掉vaddr对应的pte */
static void page_table_pte_remove(uint32_t vaddr) {
    uint32_t* pte = pte_ptr(vaddr);
    *pte &= ~PG_P_1;	                                   // 将页表项pte的P位置0，不需要删除pde
    asm volatile ("invlpg %0"::"m" (vaddr) : "memory");    // 更新tlb
}

/* 在虚拟地址池中释放以_vaddr起始的连续pg_cnt个虚拟页地址 */
static void vaddr_remove(enum pool_flags pf, void* _vaddr, uint32_t pg_cnt) {
    uint32_t bit_idx_start = 0;
    uint32_t vaddr = (uint32_t)_vaddr;
    uint32_t cnt = 0;

    if (pf == PF_KERNEL) {
        // 内核虚拟内存池
        bit_idx_start = (vaddr - kernel_vaddr.vaddr_start) / PG_SIZE;
        while (cnt < pg_cnt) {
            bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 0);
        }
    }
    else if (pf == PF_USER) {
        // 用户虚拟内存池
        struct task_struct* cur_thread = running_thread();
        bit_idx_start = (vaddr - cur_thread->userprog_vaddr.vaddr_start) / PG_SIZE;
        while (cnt < pg_cnt) {
            bitmap_set(&cur_thread->userprog_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 0);
        }
    }
    else {
        PANIC("vaddr_remove error!\n");
    }
}

/* 释放以虚拟地址vaddr为起始的cnt个页框，vaddr必须是页框起始地址 */
void mfree_page(enum pool_flags pf, void* _vaddr, uint32_t pg_cnt) {
    uint32_t vaddr = (int32_t)_vaddr;
    uint32_t page_cnt = 0;
    // 确保虚拟地址是页框的起始
    ASSERT(pg_cnt >= 1 && vaddr % PG_SIZE == 0);
    // 获取虚拟地址vaddr对应的物理地址
    uint32_t pg_phy_addr = addr_v2p(vaddr);
    // 确保物理地址也是页框的起始
    ASSERT((pg_phy_addr % PG_SIZE) == 0);
    // 确保待释放的物理内存在低端1M+1k大小的页目录+1k大小的页表地址范围外
    ASSERT(pg_phy_addr >= 0x102000);

    if (pg_phy_addr >= user_pool.phy_addr_start) {
        // 位于user_pool内存池,要释放的是用户内存
        for (page_cnt = 0; page_cnt < pg_cnt; page_cnt++) {
            vaddr = (int)_vaddr + PG_SIZE * page_cnt;
            pg_phy_addr = addr_v2p(vaddr);
            // 确保物理地址属于用户物理内存池 
            ASSERT((pg_phy_addr % PG_SIZE) == 0);
            ASSERT(pg_phy_addr >= user_pool.phy_addr_start);
            // 先将对应的物理页框归还到内存池
            pfree(pg_phy_addr);
            // 再从页表中清除此虚拟地址所在的页表项pte
            page_table_pte_remove(vaddr);
        }
    }
    else {
        // 位于kernel_pool内存池,要释放的是内核内存
        for (page_cnt = 0; page_cnt < pg_cnt; page_cnt++) {
            vaddr = (int)_vaddr + PG_SIZE * page_cnt;
            // 获得物理地址
            pg_phy_addr = addr_v2p(vaddr);
            // 确保待释放的物理内存只属于内核物理内存池
            ASSERT((pg_phy_addr % PG_SIZE) == 0);
            ASSERT(pg_phy_addr >= kernel_pool.phy_addr_start);
            ASSERT(pg_phy_addr < user_pool.phy_addr_start);
            // 先将对应的物理页框归还到内存池
            pfree(pg_phy_addr);
            // 再从页表中清除此虚拟地址所在的页表项pte
            page_table_pte_remove(vaddr);
        }
    }
    // 清空虚拟地址的位图中的相应位
    vaddr_remove(pf, _vaddr, pg_cnt);
}

/* 回收堆内存 */
void sys_free(void* ptr) {
    ASSERT(ptr != NULL);
    if (ptr == NULL) return;

    enum pool_flags PF;        // 回收的是内核还是用户的内存
    struct pool* mem_pool;     // 内核用户池或者用户内存池

    /* 判断是线程还是进程 */
    if (running_thread()->pgdir == NULL) {
        ASSERT((uint32_t)ptr >= K_HEAP_START);
        PF = PF_KERNEL;
        mem_pool = &kernel_pool;
    }
    else {
        PF = PF_USER;
        mem_pool = &user_pool;
    }

    lock_acquire(&mem_pool->lock);
    struct mem_block* b = ptr;
    struct arena* a = block2arena(b);	       // 把mem_block转换成arena,获取元信息，元信息在每个块的头部
    if (a->desc == NULL && a->large == true) { // 大于1024的内存
        mfree_page(PF, a, a->cnt);
    }
    else {
        // 小于等于1024的内存块，先将内存块回收到描述符的空闲列表
        list_append(&a->desc->free_list, &b->free_elem);
        // 将内存块元信息中的块数量加1
        a->cnt++;
        // 再判断此arena中的内存块是否都是空闲,如果是就释放这个arena块
        if (a->cnt == a->desc->blocks_per_arena) {
            // 先从空闲列表中逐个删除块
            for (uint32_t block_idx = 0; block_idx < a->desc->blocks_per_arena; block_idx++) {
                struct mem_block* b = arena2block(a, block_idx);
                ASSERT(elem_find(&a->desc->free_list, &b->free_elem));
                list_remove(&b->free_elem);
            }
            // 在删除整个页
            mfree_page(PF, a, 1);
        }
    }
    lock_release(&mem_pool->lock);
}


/* 初始化内核的堆arean内存 */
static void arena_init(void) {
    put_str("----arena_init begin!\n");
    block_desc_init(k_block_descs);
    put_str("----arena_init end!\n");
}

/* 内存资源初始化 */
void mem_init() {
    put_str("mem_init begin!\n");
    uint32_t mem_bytes_total = (*(uint32_t*)(0xc0000809));  // 这里真实物理地址是0x809,这个是在loader中存储的
    put_str("mem_bytes_total:"); put_int(mem_bytes_total); put_str("Byte = "); put_int(mem_bytes_total / 1024 / 1024);  put_str("MB\n");
    mem_pool_init(mem_bytes_total);	  // 初始化内存池
    arena_init();                     // 初始化arena
    put_str("mem_init done!\n");
}
