/*
 * @Author: lyajpunov 1961558693@qq.com
 * @Date: 2024-04-01 00:22:32
 * @LastEditors: lyajpunov 1961558693@qq.com
 * @LastEditTime: 2024-04-02 07:33:31
 * @FilePath: /os/src/fs/inode.c
 * @Description: 
 * 
 * Copyright (c) 2024 by ${lyajpunov 1961558693@qq.com}, All Rights Reserved. 
 */

#include "inode.h"
#include "stdin.h"
#include "ide.h"
#include "string.h"
#include "list.h"
#include "assert.h"
#include "super_block.h"
#include "thread.h"
#include "interrupt.h"

/* 用来存储inode位置 */
struct inode_position {
    bool	 two_sec;	    // inode是否跨扇区
    uint32_t sec_lba;	    // inode所在的扇区号
    uint32_t off_size;	    // inode在扇区内的字节偏移量
};

/**
 * @description: 获取inode所在扇区和扇区内的偏移量，方便后续的写入工作，返回值存入inode_pos
 * @param {partition*} part 分区
 * @param {uint32_t} inode_no inode号
 * @param {inode_position*} inode_pos 记录inode位置的结构体
 * @return {*}
 */
static void inode_locate(struct partition* part, uint32_t inode_no, struct inode_position* inode_pos) {
    // 最多4096个文件
    if (inode_no > 4096) return;
    // inode所在硬盘的lba
    uint32_t inode_table_lba = part->sb->inode_table_lba;
    // 一个inode的大小
    uint32_t inode_size = sizeof(struct inode);
    // 第inode_no号I结点相对于inode_table_lba的字节偏移量
    uint32_t off_size = inode_no * inode_size;
    // 第inode_no号I结点相对于inode_table_lba的扇区偏移量
    uint32_t off_sec = off_size / 512;
    // 待查找的inode所在扇区中的起始地址
    uint32_t off_size_in_sec = off_size % 512;

    // 判断此i结点是否跨越2个扇区
    uint32_t left_in_sec = 512 - off_size_in_sec;
    if (left_in_sec < inode_size) {
        // 若扇区内剩下的空间不足以容纳一个inode,必然是I结点跨越了2个扇区
        inode_pos->two_sec = true;
    }
    else {
        // 否则,所查找的inode未跨扇区
        inode_pos->two_sec = false;
    }
    inode_pos->sec_lba = inode_table_lba + off_sec;
    inode_pos->off_size = off_size_in_sec;
}


/**
 * @description: 将inode信息更新到硬盘
 * @param {partition*} part 要写入的分区
 * @param {inode*} inode 内存地址
 * @return {*}
 */
void inode_sync(struct partition* part, struct inode* inode) {
    // inode 编号
    uint8_t inode_no = inode->i_no;
    // inode 位置
    struct inode_position inode_pos;
    // inode 位置信息会存入 inode_pos
    inode_locate(part, inode_no, &inode_pos);
    // 保证 inode 所在扇区是有效的
    ASSERT(inode_pos.sec_lba <= (part->start_lba + part->sec_cnt));
    // 创建一个 inode
    struct inode pure_inode;
    // 给这个 inode 初始化
    memcpy(&pure_inode, inode, sizeof(struct inode));

    // 以下inode的三个成员只存在于内存中,现在将inode同步到硬盘,清掉这三项即可
    pure_inode.i_open_cnts = 0;
    // 置为false,以保证在硬盘中读出时为可写
    pure_inode.write_deny = false;
    // 链表清空
    pure_inode.inode_tag.prev = NULL;
    pure_inode.inode_tag.next = NULL;

    // 申请两个扇区的空间
    char* inode_buf = (char*)sys_malloc(1024);
    // 若是跨了两个扇区,就要读出两个扇区再写入两个扇区
    if (inode_pos.two_sec) {
        // 先读出两个扇区的内容
        ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 2);
        // 开始将待写入的inode拼入到这2个扇区中的相应位置 
        memcpy((inode_buf + inode_pos.off_size), &pure_inode, sizeof(struct inode));
        // 将拼接好的数据再写入磁盘
        ide_write(part->my_disk, inode_pos.sec_lba, inode_buf, 2);
    }
    // 若只是一个扇区
    else {
        // 先读出一个扇区的内容
        ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 1);
        // 开始将待写入的inode拼入到这个扇区中的相应位置 
        memcpy((inode_buf + inode_pos.off_size), &pure_inode, sizeof(struct inode));
        // 将拼接好的数据再写入磁盘
        ide_write(part->my_disk, inode_pos.sec_lba, inode_buf, 1);
    }
    sys_free(inode_buf);
}

/**
 * @description: 打开一个inode节点
 * @param {struct partition*} part 选择分区
 * @param {uint32_t} inode_no 选择inode号
 * @return {struct inode*} 返回inode结构体的指针
 */
struct inode* inode_open(struct partition* part, uint32_t inode_no) {
    // 先在已打开inode链表中找inode,此链表是为提速创建的缓冲区
    struct list_elem* elem = part->open_inodes.head.next;
    struct inode* inode_found;
    while (elem != &part->open_inodes.tail) {
        inode_found = elem2entry(struct inode, inode_tag, elem);
        if (inode_found->i_no == inode_no) {
            inode_found->i_open_cnts++;
            // 在已打开的inode中找到了该节点，就直接返回
            return inode_found;
        }
        elem = elem->next;
    }

    // 在已打开节点中没找到，就从硬盘中找
    struct inode_position inode_pos;

    // inode位置信息会存入inode_pos, 包括inode所在扇区地址和扇区内的字节偏移量
    inode_locate(part, inode_no, &inode_pos);

    // 想要让这个inode节点被所有线程共享，只能在高1GB的内核空间，所以先降页表置为空
    struct task_struct* cur = running_thread();

    // 获取页表的旧地址 #TODO
    uint32_t* cur_pagedir_bak = cur->pgdir;
    cur->pgdir = NULL;
    // 此时申请的内存在内核地址池
    inode_found = (struct inode*)sys_malloc(sizeof(struct inode));
    // 页表的旧地址赋值
    cur->pgdir = cur_pagedir_bak;

    char* inode_buf;
    // inode处于两个扇区
    if (inode_pos.two_sec) {
        inode_buf = (char*)sys_malloc(1024);
        ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 2);
    }
    // inode处于一个扇区
    else {
        inode_buf = (char*)sys_malloc(512);
        ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 1);
    }
    memcpy(inode_found, inode_buf + inode_pos.off_size, sizeof(struct inode));
    // 因为一会很可能要用到此inode,故将其插入到队首便于提前检索到
    list_push(&part->open_inodes, &(inode_found->inode_tag));
    // 第一次被打开，将cnt置为1
    inode_found->i_open_cnts = 1;
    // 释放缓存
    sys_free(inode_buf);
    // 返回inode节点
    return inode_found;
}


/**
 * @description: 关闭一个inode节点，释放inode节点占用的内存，如果有多个线程都打开了这个inode，那么计数器减1
 * @param {inode*} inode 要关闭的inode节点
 * @return {*}
 */
void inode_close(struct inode* inode) {
    // 关中断
    enum intr_status old_status = intr_disable();
    // 如果当前线程关闭这个inode，且没有线程再占用这个inode
    if (--inode->i_open_cnts == 0) {
        list_remove(&inode->inode_tag);
        // 释放掉inode节点占用的堆内存,也是需要将页表值置空再恢复
        struct task_struct* cur = running_thread();
        uint32_t* cur_pagedir_bak = cur->pgdir;
        cur->pgdir = NULL;
        sys_free(inode);
        cur->pgdir = cur_pagedir_bak;
    }
    // 恢复中断
    intr_set_status(old_status);
}

/**
 * @description: 初始化一个inode
 * @param {uint32_t} inode编号
 * @param {inode*} 新的inode节点在内存中的地址
 * @return {*}
 */
void inode_init(uint32_t inode_no, struct inode* new_inode) {
    new_inode->i_no = inode_no;
    new_inode->i_size = 0;
    new_inode->i_open_cnts = 0;
    new_inode->write_deny = false;

    /* 初始化块索引数组i_sector */
    for (uint8_t sec_idx = 0; sec_idx < 13; sec_idx++) {
        new_inode->i_sectors[sec_idx] = 0;
    }
}
