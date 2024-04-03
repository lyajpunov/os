/*
 * @Author: lyajpunov 1961558693@qq.com
 * @Date: 2024-03-28 01:03:39
 * @LastEditors: lyajpunov 1961558693@qq.com
 * @LastEditTime: 2024-04-02 07:48:35
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
    put_str("syscall_init done!\n");
}
