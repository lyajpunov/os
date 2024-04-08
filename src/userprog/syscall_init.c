/*
 * @Author: lyajpunov 1961558693@qq.com
 * @Date: 2024-03-28 01:03:39
 * @LastEditors: lyajpunov 1961558693@qq.com
 * @LastEditTime: 2024-04-08 02:13:28
 * @FilePath: /os/src/userprog/syscall_init.c
 * @Description: 系统调用初始化
 *
 * Copyright (c) 2024 by ${git_name_email}, All Rights Reserved.
 */
#include "syscall_init.h"
#include "syscall.h"
#include "stdin.h"
#include "thread.h"
#include "string.h"
#include "console.h"
#include "print.h"
#include "interrupt.h"
#include "memory.h"
#include "fs.h"
#include "fork.h"

 // 系统调用总数 
#define syscall_nr 64
// 系统调用数组
typedef void* syscall;

syscall syscall_table[syscall_nr];

/* 0号调用：返回当前任务的pid */
uint32_t sys_getpid(void) {
    return running_thread()->pid;
}

/* 初始化系统调用，也就是将syscall_table数组中绑定好确定的函数 */
void syscall_init(void) {
    put_str("syscall_init begin!\n");
    syscall_table[SYS_GETPID] = sys_getpid;
    syscall_table[SYS_WRITE] = sys_write;
    syscall_table[SYS_MALLOC] = sys_malloc;
    syscall_table[SYS_FREE] = sys_free;
    syscall_table[SYS_FORK] = sys_fork;
    syscall_table[SYS_READ] = sys_read;
    syscall_table[SYS_PUTCHAR] = sys_putchar;
    syscall_table[SYS_CLEAR] = cls_screen;
    syscall_table[SYS_GETCWD] = sys_getcwd;
    syscall_table[SYS_OPEN] = sys_open;
    syscall_table[SYS_CLOSE] = sys_close;
    syscall_table[SYS_LSEEK] = sys_lseek;
    syscall_table[SYS_UNLINK] = sys_unlink;
    syscall_table[SYS_MKDIR] = sys_mkdir;
    syscall_table[SYS_OPENDIR] = sys_opendir;
    syscall_table[SYS_CLOSEDIR] = sys_closedir;
    syscall_table[SYS_CHDIR] = sys_chdir;
    syscall_table[SYS_RMDIR] = sys_rmdir;
    syscall_table[SYS_READDIR] = sys_readdir;
    syscall_table[SYS_REWINDDIR] = sys_rewinddir;
    syscall_table[SYS_STAT] = sys_stat;
    syscall_table[SYS_PS] = sys_ps;
    put_str("syscall_init done!\n");
}
