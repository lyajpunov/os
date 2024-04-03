/*
 * @Author: lyajpunov 1961558693@qq.com
 * @Date: 2024-03-28 00:43:36
 * @LastEditors: lyajpunov 1961558693@qq.com
 * @LastEditTime: 2024-04-02 07:52:45
 * @FilePath: /os/src/lib/user/syscall.h
 * @Description: 
 * 
 * Copyright (c) 2024 by ${git_name_email}, All Rights Reserved. 
 */
#ifndef __LIB_USER_SYSCALL_H
#define __LIB_USER_SYSCALL_H

#include "stdin.h"

enum SYSCALL_NR {
    SYS_GETPID,               // 0号调用：获取当前线程的pid
    SYS_WRITE,                // 1号调用：把buf中count个字符写到文件描述符fd指向的文件中
    SYS_MALLOC,               // 2号调用：申请内存
    SYS_FREE,                 // 3号调用：释放内存
};

/* 获取当前进程的pid */
uint32_t getpid(void);
/* 把buf中count个字符写到文件描述符fd指向的文件中 */
int32_t write(int32_t fd, const void* buf, uint32_t count);
/* 申请堆内存 */
void* malloc(uint32_t size);
/* 释放堆内存 */
void free(void* ptr);
#endif

