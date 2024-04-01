#ifndef __FS_INODE_H
#define __FS_INODE_H
#include "stdin.h"
#include "list.h"

/* inode结构 */
struct inode {
    uint32_t i_no;           // inode编号
    uint32_t i_size;         // 当此inode是文件时,i_size是指文件大小,若此inode是目录,i_size是指该目录下所有目录项大小之和
    uint32_t i_open_cnts;    // 记录此文件被打开的次数
    uint32_t user_id;        // 文件所属用户id
    uint64_t ctime;          // inode上一次变动时间
    uint64_t mtime;          // 文件内容上一次变动时间
    uint64_t atime;          // 文件上一次打开时间
    uint8_t privilege;       // 权限，可读可写可执行 1-7
    uint8_t write_deny;	     // 写文件不能并行,进程写文件前检查此标识
    uint32_t i_sectors[13];  // i_sectors[0-11]是直接块, i_sectors[12]用来存储一级间接块指针
    struct list_elem inode_tag;
} __attribute__((packed));

#endif
