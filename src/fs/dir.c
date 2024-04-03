/*
 * @Author: lyajpunov 1961558693@qq.com
 * @Date: 2024-04-01 00:22:21
 * @LastEditors: lyajpunov 1961558693@qq.com
 * @LastEditTime: 2024-04-02 06:19:43
 * @FilePath: /os/src/fs/dir.c
 * @Description: 目录的基本操作
 * 
 * Copyright (c) 2024 by ${git_name_email}, All Rights Reserved. 
 */
#include "dir.h"
#include "ide.h"
#include "file.h"
#include "string.h"
#include "stdio.h"
#include "assert.h"
#include "super_block.h"

// 根目录
struct dir root_dir;

/**
 * @description: 打开根目录
 * @param {partition*} part 打开根目录的分区
 * @return {*}
 */
void open_root_dir(struct partition* part) {
    root_dir.inode = inode_open(part, part->sb->root_inode_no);
    root_dir.dir_pos = 0;
}

/**
 * @description: 在分区part上打开i结点为inode_no的目录并返回目录指针
 * @param {partition*} part 分区
 * @param {uint32_t} inode_no 节点
 * @return {*}
 */
struct dir* dir_open(struct partition* part, uint32_t inode_no) {
    struct dir* pdir = (struct dir*)sys_malloc(sizeof(struct dir));
    pdir->inode = inode_open(part, inode_no);
    pdir->dir_pos = 0;
    return pdir;
}

/**
 * @description: 在part分区内的pdir目录内寻找名为name的文件或目录,找到后返回true并将其目录项存入dir_e,否则返回false
 * @param {partition*} part 分区
 * @param {dir*} pdir 从此目录查找
 * @param {char*} name 文件或者目录
 * @param {dir_entry*} dir_e 找到目录项存入dir_e
 * @return {*}
 */
bool search_dir_entry(struct partition* part, struct dir* pdir, const char* name, struct dir_entry* dir_e) {
    // 12个直接块大小+128个间接块,共560字节
    uint32_t* all_blocks = (uint32_t*)sys_malloc(560);
    // 申请内存
    if (all_blocks == NULL) {
        printk("search_dir_entry: sys_malloc for all_blocks failed");
        return false;
    }

    // 填充12个一级指针
    for (uint32_t block_idx = 0; block_idx < 12; block_idx++) {
        all_blocks[block_idx] = pdir->inode->i_sectors[block_idx];
    }
    // 再处理二级页表
    if (pdir->inode->i_sectors[12] != 0) {
        ide_read(part->my_disk, pdir->inode->i_sectors[12], all_blocks + 12, 1);
    }

    // 写目录项的时候保证在一个扇区，读的时候就可以从一个扇区读，这样浪费了一点空间，但是更方便了
    uint8_t* buf = (uint8_t*)sys_malloc(SECTOR_SIZE);
    // 指向目录项的指针,值为buf起始地址
    struct dir_entry* p_de = (struct dir_entry*)buf;
    // 目录项的大小
    uint32_t dir_entry_size = part->sb->dir_entry_size;
    // 1扇区内可容纳的目录项个数
    uint32_t dir_entry_cnt = SECTOR_SIZE / dir_entry_size;

    // 开始在所有块中查找目录项
    for (uint32_t block_idx = 0; block_idx < 140; block_idx++) {
        // 块地址为0时表示该块中无数据,继续在其它块中找
        if (all_blocks[block_idx] == 0) {
            block_idx++;
            continue;
        }
        // 读取有目录的块
        ide_read(part->my_disk, all_blocks[block_idx], buf, 1);
        // 遍历扇区中所有目录项
        for (uint32_t dir_entry_idx = 0; dir_entry_idx < dir_entry_cnt; dir_entry_idx++) {
            // 若找到了,就直接复制整个目录项
            if (!strcmp(p_de->filename, name)) {
                memcpy(dir_e, p_de, dir_entry_size);
                sys_free(buf);
                sys_free(all_blocks);
                return true;
            }
            p_de++;
        }
        // 此时p_de已经指向扇区内最后一个完整目录项了,需要恢复p_de指向为buf
        p_de = (struct dir_entry*)buf;
        // 将buf清0,下次再用
        memset(buf, 0, SECTOR_SIZE);
    }
    sys_free(buf);
    sys_free(all_blocks);
    return false;
}

/**
 * @description: 关闭目录
 * @param {dir*} dir 要关闭的目录
 * @return {*}
 */
void dir_close(struct dir* dir) {
    // 如果是根目录是不能关闭的，根目录打开的空间直接在内核。不在堆中，不能free
    if (dir == &root_dir) return;
    inode_close(dir->inode);
    sys_free(dir);
}

/**
 * @description: 在内存中初始化目录项p_de
 * @param {char*} filename 文件名
 * @param {uint32_t} inode_no inode编号
 * @param {uint8_t} file_type 文件类型
 * @param {dir_entry*} p_de 目录项
 * @return {*}
 */
void create_dir_entry(char* filename, uint32_t inode_no, uint8_t file_type, struct dir_entry* p_de) {
    memcpy(p_de->filename, filename, strlen(filename));
    p_de->i_no = inode_no;
    p_de->f_type = file_type;
}

/**
 * @description: 将目录项p_de写入父目录parent_dir中,io_buf由主调函数提供
 * @param {dir*} parent_dir 父目录
 * @param {dir_entry*} p_de 要写入的目录项
 * @param {void*} io_buf    缓存，由主调函数提供
 * @return {*}
 */
bool sync_dir_entry(struct dir* parent_dir, struct dir_entry* p_de, void* io_buf) {
    // 根目录的inode
    struct inode* dir_inode = parent_dir->inode;
    // 根目录的文件大小
    uint32_t dir_size = dir_inode->i_size;
    // 目录项的大小，现在是
    uint32_t dir_entry_size = cur_part->sb->dir_entry_size;
    // 保证根目录的文件大小是目录项的整数倍
    ASSERT(dir_size % dir_entry_size == 0);
    // 每个扇区最大的目录项树木
    uint32_t dir_entrys_per_sec = (512 / dir_entry_size);
    int32_t block_lba = -1;

    // 将该目录的所有扇区地址存入all_blocks
    uint32_t all_blocks[140];
    for (uint8_t block_idx = 0; block_idx < 12; block_idx++) {
        all_blocks[block_idx] = dir_inode->i_sectors[block_idx];
    }

    // 创建指向iobuf的指针用来遍历
    struct dir_entry* dir_e = (struct dir_entry*)io_buf;
    int32_t block_bitmap_idx = -1;

    // 开始遍历所有块以寻找目录项空位,若已有扇区中没有空闲位,在不超过文件大小的情况下申请新扇区来存储新目录项
    for (uint8_t block_idx = 0; block_idx < 140; block_idx++) {
        block_bitmap_idx = -1;
        if (all_blocks[block_idx] == 0) {
            // 找到了一个没有地址的指针
            block_lba = block_bitmap_alloc(cur_part);
            if (block_lba == -1) {
                printk("sync_dir_entry error: alloc block bitmap failed\n");
                return false;
            }

            // 每分配一个块就同步一次block_bitmap
            block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
            bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);


            block_bitmap_idx = -1;
            // 若是直接块，那么将这个地址赋值给直接块指正
            if (block_idx < 12) {
                dir_inode->i_sectors[block_idx] = all_blocks[block_idx] = block_lba;
            }
            // 若是尚未分配一级间接块表(block_idx等于12表示第0个间接块地址为0)
            else if (block_idx == 12) {
                // 将上面分配的块做为一级间接块表地址
                dir_inode->i_sectors[12] = block_lba;
                block_lba = -1;
                // 再分配一个块做为第0个间接块，也就是第13个块
                block_lba = block_bitmap_alloc(cur_part);
                // 如果分配失败
                if (block_lba == -1) {
                    // 找到第一个分配的块
                    block_bitmap_idx = dir_inode->i_sectors[12] - cur_part->sb->data_start_lba;
                    // 将其恢复
                    bitmap_set(&cur_part->block_bitmap, block_bitmap_idx, 0);
                    // 将一节间接表指正恢复
                    dir_inode->i_sectors[12] = 0;
                    // 打印报错信息
                    printk("sync_dir_entry error: alloc block bitmap failed\n");
                    return false;
                }

                // 每分配一个块就同步一次block_bitmap
                block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
                bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
                // 将指针指向新分配的块
                all_blocks[12] = block_lba;
                // 把新分配的第0个间接块地址写入一级间接块表
                ide_write(cur_part->my_disk, dir_inode->i_sectors[12], all_blocks + 12, 1);
            }
            // 若是间接块未分配
            else {
                all_blocks[block_idx] = block_lba;
                // 把新分配的第(block_idx-12)个间接块地址写入一级间接块表
                ide_write(cur_part->my_disk, dir_inode->i_sectors[12], all_blocks + 12, 1);
            }

            // 再将新目录项p_de写入新分配的间接块
            memset(io_buf, 0, 512);
            memcpy(io_buf, p_de, dir_entry_size);
            ide_write(cur_part->my_disk, all_blocks[block_idx], io_buf, 1);
            dir_inode->i_size += dir_entry_size;
            return true;
        }

        // 若第block_idx块已存在,将其读进内存,然后在该块中查找空目录项
        ide_read(cur_part->my_disk, all_blocks[block_idx], io_buf, 1);
        // 在扇区内查找空目录项 
        for (uint8_t dir_entry_idx = 0; dir_entry_idx < dir_entrys_per_sec; dir_entry_idx++) {
            // FT_UNKNOWN为0,无论是初始化或是删除文件后,都会将f_type置为FT_UNKNOWN.
            if ((dir_e + dir_entry_idx)->f_type == FT_UNKNOWN) {
                memcpy(dir_e + dir_entry_idx, p_de, dir_entry_size);
                ide_write(cur_part->my_disk, all_blocks[block_idx], io_buf, 1);
                dir_inode->i_size += dir_entry_size;
                return true;
            }
        }
    }
    printk("directory is full!\n");
    return false;
}

