#include "memory.h"
#include "bitmap.h"
#include "stdin.h"
#include "global.h"
#include "assert.h"
#include "print.h"
#include "string.h"

// 拿到addr的前10bit，也就是页目录的索引
#define PDE_IDX(addr) ((addr & 0xffc00000) >> 22)
// 拿到addr的中10bit，也就是页表的索引
#define PTE_IDX(addr) ((addr & 0x003ff000) >> 12)

struct pool kernel_pool, user_pool;      // 生成内核内存池和用户内存池
struct virtual_addr kernel_vaddr;	     // 此结构是用来给内核分配虚拟地址

/* 在pf表示的虚拟地址池中同时取出pg_cnt个连续的页，成功返回虚拟地址，失败返回NULL */
static void* vaddr_get(enum pool_flags pf, uint32_t pg_cnt) {
    int vaddr_start = 0, bit_idx_start = -1;
    uint32_t cnt = 0;
    if (pf == PF_KERNEL) {
        bit_idx_start  = bitmap_scan(&kernel_vaddr.vaddr_bitmap, pg_cnt);
        if (bit_idx_start == -1) {
            return NULL;
        }
        while(cnt < pg_cnt) {
            bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 1);
        }
        vaddr_start = kernel_vaddr.vaddr_start + bit_idx_start * PG_SIZE;
    } else {	
    // 用户内存池,将来实现用户进程再补充
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

/* 在m_pool指向的物理内存池中分配1个物理页,成功则返回页框的物理地址,失败则返回NULL */
static void* palloc(struct pool* m_pool) {
    int bit_idx = bitmap_scan(&m_pool->pool_bitmap, 1);    // 找一个物理页面
    if (bit_idx == -1 ) {
        return NULL;
    }
    bitmap_set(&m_pool->pool_bitmap, bit_idx, 1);	      // 将此位bit_idx置1
    uint32_t page_phyaddr = ((bit_idx * PG_SIZE) + m_pool->phy_addr_start);
    return (void*)page_phyaddr;
}

/* 页表中添加虚拟地址_vaddr与物理地址_page_phyaddr的映射 */
static void page_table_add(void* _vaddr, void* _page_phyaddr) {
    uint32_t vaddr = (uint32_t)_vaddr;
    uint32_t page_phyaddr = (uint32_t)_page_phyaddr;
    uint32_t* pde = pde_ptr(vaddr);
    uint32_t* pte = pte_ptr(vaddr);

    /* 先在页目录内判断目录项的P位，若为1,则表示该表已存在 */
    if (*pde & 0x00000001) {	      // 页目录项和页表项的第0位为P,此处判断目录项是否存在
        ASSERT(!(*pte & 0x00000001)); // 确保pte的最后一位为0，也就是这一位并未使用
        *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
    } else {  // 页目录项不存在,所以要先创建页目录再创建页表项.
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
    void* vaddr =  malloc_page(PF_KERNEL, pg_cnt);
    if (vaddr != NULL) {
        memset(vaddr, 0, pg_cnt * PG_SIZE);
    } else {
        put_str("get_kernel_pages error: vaddr error!\n");
    }
    return vaddr;
}

/* 初始化内存池 */
static void mem_pool_init(uint32_t all_mem) {
    put_str("----mem_pool_init start\n");

    uint32_t page_table_size = PG_SIZE * 256;	  // 1页的页目录表+第0和第768个页目录项指向同一个页表
                                                  // 第769~1022个页目录项共指向254个页表,共256个页框
    uint32_t used_mem = page_table_size + 0x100000;	  // 使用的内存，也就是低端1MB+128KB
    uint32_t free_mem = all_mem - used_mem;           // 空闲的内存
    uint16_t all_free_pages = free_mem / PG_SIZE;		  // 空闲了多少页 
    uint16_t kernel_free_pages = all_free_pages / 2;                  // 内核空间拿一半空闲的页
    uint16_t user_free_pages = all_free_pages - kernel_free_pages;    // 用户空间拿一半空闲的页
    uint32_t kbm_length = kernel_free_pages / 8;		  // 内核空间bitmap的长度
    uint32_t ubm_length = user_free_pages / 8;			  // 用户空间bitmap的长度
    uint32_t kp_start = used_mem;			        	  // 内核空间的起始地址
    uint32_t up_start = kp_start + kernel_free_pages * PG_SIZE;	  // 用户空间的起始地址

    kernel_pool.phy_addr_start = kp_start;     // 内核内存池物理地址的起始
    user_pool.phy_addr_start   = up_start;     // 用户内存池物理地址的起始

    kernel_pool.pool_size = kernel_free_pages * PG_SIZE;  // 内核内存池的大小
    user_pool.pool_size	 = user_free_pages * PG_SIZE;     // 用户内存池的大小

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

    put_str("----mem_pool_init done\n");
}


void mem_init() {
    put_str("mem_init start\n");
    uint32_t mem_bytes_total = (*(uint32_t*)(0xc0000809));
    put_str("mem_bytes_total:"); put_int(mem_bytes_total); put_str("Byte = "); put_int(mem_bytes_total/1024/1024);  put_str("MB\n");
    mem_pool_init(mem_bytes_total);	  // 初始化内存池
    put_str("mem_init done\n");
}
