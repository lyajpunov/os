#ifndef __KERNEL_MEMORY_H
#define __KERNEL_MEMORY_H

#include "stdin.h"
#include "bitmap.h"

// 一个页框的大小，4KB
#define PG_SIZE 4096
// 位图的地址，这里涉及到一点后续的内存规划 
// 0xc009a000~0xc009dfff 为物理位图
// 0xc009e000~0xc009efff 为main线程的pcb
// 0xc009f000 为main线程的栈顶
#define MEM_BITMAP_BASE 0xc009a000

// 0x100000意指跨过低端1M内存,也就是低端的1MB随我们折腾了
#define K_HEAP_START 0xc0100000


#define	 PG_P_1	  1	// 页表项或页目录项存在属性位
#define	 PG_P_0	  0	// 页表项或页目录项存在属性位
#define	 PG_RW_R  0	// R/W 属性位值, 读/执行
#define	 PG_RW_W  2	// R/W 属性位值, 读/写/执行
#define	 PG_US_S  0	// U/S 属性位值, 系统级
#define	 PG_US_U  4	// U/S 属性位值, 用户级


/* 内存池标记,用于判断用哪个内存池 */
void malloc_init(void);
enum pool_flags {
    PF_KERNEL = 1,    // 内核内存池
    PF_USER = 2	     // 用户内存池
};

/* 用于虚拟地址管理，虚拟地址池 */
struct virtual_addr {
    /* 虚拟地址用到的位图结构，用于记录哪些虚拟地址被占用了。以页为单位。*/
    struct bitmap vaddr_bitmap;
    /* 管理的虚拟地址 */
    uint32_t vaddr_start;
};

extern struct pool kernel_pool, user_pool;

/* 得到虚拟地址vaddr对应的pte指针*/
uint32_t* pte_ptr(uint32_t vaddr);
/* 得到虚拟地址vaddr对应的pde指针*/
uint32_t* pde_ptr(uint32_t vaddr);
/* 内存管理部分初始化 */
void mem_init(void);
/* 从内核物理内存池中申请pg_cnt页内存,成功则返回其虚拟地址,失败则返回NULL */
void* get_kernel_pages(uint32_t pg_cnt);
/* 从内核用户内存池中申请pg_cnt页内存,成功则返回其虚拟地址,失败则返回NULL */
void* get_user_pages(uint32_t pg_cnt);
/* 将地址vaddr与pf池中的物理地址关联,仅支持一页空间分配 */
void* get_a_page(enum pool_flags pf, uint32_t vaddr);
/* 分配pg_cnt个页空间,成功则返回起始虚拟地址,失败时返回NULL */
void* malloc_page(enum pool_flags pf, uint32_t pg_cnt);
/* 得到虚拟地址映射到的物理地址 */
uint32_t addr_v2p(uint32_t vaddr);

#endif
