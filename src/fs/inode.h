/*
 * @Author: lyajpunov 1961558693@qq.com
 * @Date: 2024-04-01 00:22:27
 * @LastEditors: lyajpunov 1961558693@qq.com
 * @LastEditTime: 2024-04-08 05:42:29
 * @FilePath: /os/src/fs/inode.h
 * @Description: 
 * 
 * Copyright (c) 2024 by ${git_name_email}, All Rights Reserved. 
 */

#ifndef __FS_INODE_H
#define __FS_INODE_H
#include "stdin.h"
#include "list.h"
#include "ide.h"

/* inode结构 */
struct inode {
    uint32_t i_no;           // inode编号
    uint32_t i_size;         // 当此inode是文件时,i_size是指文件大小,若此inode是目录,i_size是指该目录下所有目录项大小之和
    uint32_t user_id;        // 文件所属用户id
    uint64_t ctime;          // inode上一次变动时间
    uint64_t mtime;          // 文件内容上一次变动时间
    uint64_t atime;          // 文件上一次打开时间
    uint32_t privilege;      // 权限，可读可写可执行 1-7
    uint32_t i_open_cnts;    // 记录此文件被打开的次数
    bool write_deny;	     // 写文件不能并行,进程写文件前检查此标识
    uint32_t i_sectors[13];  // i_sectors[0-11]是直接块, i_sectors[12]用来存储一级间接块指针
    struct list_elem inode_tag;
};

struct inode* inode_open(struct partition* part, uint32_t inode_no);
void inode_close(struct inode* inode);
void inode_sync(struct partition* part, struct inode* inode);
void inode_init(uint32_t inode_no, struct inode* new_inode);
void inode_delete(struct partition* part, uint32_t inode_no, void* io_buf);
void inode_release(struct partition* part, uint32_t inode_no);

#endif
