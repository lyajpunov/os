/*
 * @Author: lyajpunov 1961558693@qq.com
 * @Date: 2024-03-26 07:37:49
 * @LastEditors: lyajpunov 1961558693@qq.com
 * @LastEditTime: 2024-04-08 00:52:43
 * @FilePath: /os/src/kernel/memory.h
 * @Description: 内存管理
 * 
 * Copyright (c) 2024 by ${git_name_email}, All Rights Reserved. 
 */
#ifndef __KERNEL_MEMORY_H
#define __KERNEL_MEMORY_H

#include "stdin.h"
#include "bitmap.h"
#include "list.h"

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

#define  DESC_CNT 7	// 内存块描述符个数

enum pool_flags {
    PF_KERNEL = 1,  // 内核内存池
    PF_USER = 2	    // 用户内存池
};

/* 用于虚拟地址管理，虚拟地址池 */
struct virtual_addr {
    /* 虚拟地址用到的位图结构，用于记录哪些虚拟地址被占用了。以页为单位。*/
    struct bitmap vaddr_bitmap;
    /* 管理的虚拟地址的起始 */
    uint32_t vaddr_start;
};

/* 内存块 */
struct mem_block {
    struct list_elem free_elem;
};

/* 内存块描述符 */
struct mem_block_desc {
    uint32_t block_size;		 // 内存块大小
    uint32_t blocks_per_arena;	 // 本arena中可容纳此mem_block的数量.
    struct list free_list;    	 // 目前可用的mem_block链表
};

/* 内存仓库arena元信息 */
struct arena {
    struct mem_block_desc* desc; // 此arena关联的mem_block_desc
    uint32_t cnt;                // large为ture时,cnt表示的是页框数。否则cnt表示空闲mem_block数量
    bool large;
};


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
/* 在堆中申请内存 */
void* sys_malloc(uint32_t size);
/* 回收堆内存 */
void sys_free(void* ptr);
/* 初始化块描述符 */
void block_desc_init(struct mem_block_desc* desc_array);
/* 将物理地址pg_phy_addr回收到物理内存池,这里的回收以页为单位 */
void pfree(uint32_t pg_phy_addr);
/* 释放以虚拟地址vaddr为起始的cnt个页框，vaddr必须是页框起始地址 */
void mfree_page(enum pool_flags pf, void* _vaddr, uint32_t pg_cnt);

void* get_a_page_without_opvaddrbitmap(enum pool_flags pf, uint32_t vaddr);

#endif
