/*
 * @Author: lyajpunov 1961558693@qq.com
 * @Date: 2024-03-26 07:37:49
 * @LastEditors: lyajpunov 1961558693@qq.com
 * @LastEditTime: 2024-04-08 05:36:01
 * @FilePath: /os/src/kernel/main.c
 * @Description:
 *
 * Copyright (c) 2024 by ${git_name_email}, All Rights Reserved.
 */

#include "init.h"
#include "thread.h"
#include "interrupt.h"
#include "process.h"
#include "syscall_init.h"
#include "syscall.h"
#include "stdio.h"
#include "file.h"
#include "fs.h"
#include "ide.h"
#include "string.h"
#include "super_block.h"
#include "ide.h"
#include "timer.h"

void k_thread_a(void*);
void k_thread_b(void*);
void u_prog_a(void);
void u_prog_b(void);


int main(void) {
    init_all();
    intr_enable();

    // void* d1 = sys_malloc(2 << 2);
    // void* d2 = sys_malloc(2 << 3);
    // void* d3 = sys_malloc(2 << 4);
    // void* d4 = sys_malloc(2 << 5);
    // void* d5 = sys_malloc(2 << 6);
    // void* d6 = sys_malloc(2 << 7);
    // void* d7 = sys_malloc(2 << 8);

    // printf("%x %x %x %x %x %x %x\n", d1, d2, d3, d4, d5, d6, d7);

    // sys_free(d1);
    // sys_free(d2);
    // sys_free(d3);
    // sys_free(d4);
    // sys_free(d5);
    // sys_free(d6);
    // sys_free(d7);


    // sys_unlink("/file1");
    // sys_unlink("/file2");
    // sys_unlink("/file3");
    // sys_unlink("/file4");
    // sys_open("/file1", O_CREAT);
    // sys_open("/file2", O_CREAT);
    // sys_open("/file3", O_CREAT);
    // sys_open("/file4", O_CREAT);

    while (1);
    return 0;
}



