/*
 * @Author: lyajpunov 1961558693@qq.com
 * @Date: 2024-04-01 04:52:35
 * @LastEditors: lyajpunov 1961558693@qq.com
 * @LastEditTime: 2024-04-03 00:13:24
 * @FilePath: /os/src/fs/file.h
 * @Description: 
 * 
 * Copyright (c) 2024 by ${git_name_email}, All Rights Reserved. 
 */

#ifndef __FS_FILE_H
#define __FS_FILE_H

#include "stdin.h"
#include "ide.h"
#include "dir.h"

// 系统可打开的最大文件数
#define MAX_FILE_OPEN 32    

/* 文件结构 */
struct file {
    uint32_t fd_pos;          // 记录当前文件操作的偏移地址,以0为起始,最大为文件大小-1
    uint32_t fd_flag;         // 权限      
    struct inode* fd_inode;   // 当前文件对应的inode节点指针
};

/* 标准输入输出描述符 */
enum std_fd {
    stdin_no,   // 0 标准输入
    stdout_no,  // 1 标准输出
    stderr_no   // 2 标准错误
};

/* 位图类型 */
enum bitmap_type {
    INODE_BITMAP,     // inode位图
    BLOCK_BITMAP	  // 空闲块位图
};

/* 文件表 */
extern struct file file_table[MAX_FILE_OPEN];

int32_t inode_bitmap_alloc(struct partition* part);
int32_t block_bitmap_alloc(struct partition* part);
int32_t file_create(struct dir* parent_dir, char* filename, uint8_t flag);
void bitmap_sync(struct partition* part, uint32_t bit_idx, uint8_t btmp);
int32_t get_free_slot_in_global(void);
int32_t pcb_fd_install(int32_t globa_fd_idx);
int32_t file_open(uint32_t inode_no, uint8_t flag);
int32_t file_close(struct file* file);
int32_t file_write(struct file* file, const void* buf, uint32_t count);
int32_t file_read(struct file* file, void* buf, uint32_t count);

#endif //__FS_FILE_H
