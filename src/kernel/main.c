/*
 * @Author: lyajpunov 1961558693@qq.com
 * @Date: 2024-03-26 07:37:49
 * @LastEditors: lyajpunov 1961558693@qq.com
 * @LastEditTime: 2024-04-07 07:27:43
 * @FilePath: /os/src/kernel/main.c
 * @Description:
 *
 * Copyright (c) 2024 by ${git_name_email}, All Rights Reserved.
 */
 /*
  * @Author: lyajpunov 1961558693@qq.com
  * @Date: 2024-03-26 07:37:49
  * @LastEditors: lyajpunov 1961558693@qq.com
  * @LastEditTime: 2024-04-02 06:51:44
  * @FilePath: /os/src/kernel/main.c
  * @Description: 主函数入口
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

void k_thread_a(void*);
void k_thread_b(void*);
void u_prog_a(void);
void u_prog_b(void);


int main(void) {
    init_all();
    intr_enable();
    process_execute(u_prog_a, "u_prog_a");
    process_execute(u_prog_b, "u_prog_b");
    thread_start("k_thread_a", k_thread_a, "I am thread_a");
    thread_start("k_thread_b", k_thread_b, "I am thread_b");

    sys_open("/file1",O_CREAT);
    sys_open("/file2",O_CREAT);
    sys_open("/file3",O_CREAT);
    sys_mkdir("/dir1");
    sys_mkdir("/dir1/subdir1");
    
    struct stat obj_stat;
    sys_stat("/", &obj_stat);
    printf("/`s info\n   i_no:%d\n   size:%d\n   filetype:%s\n", \
        obj_stat.st_ino, obj_stat.st_size, \
        obj_stat.st_filetype == 2 ? "directory" : "regular");
    sys_stat("/dir1", &obj_stat);
    printf("/dir1`s info\n   i_no:%d\n   size:%d\n   filetype:%s\n", \
        obj_stat.st_ino, obj_stat.st_size, \
        obj_stat.st_filetype == 2 ? "directory" : "regular");
    sys_stat("/file1", &obj_stat);
    printf("/file1`s info\n   i_no:%d\n   size:%d\n   filetype:%s\n", \
        obj_stat.st_ino, obj_stat.st_size, \
        obj_stat.st_filetype == 2 ? "directory" : "regular");

    while (1);
    return 0;
}


/* 在线程中运行的函数 */
void k_thread_a(void* arg UNUSED) {
    while (1);
}

/* 在线程中运行的函数 */
void k_thread_b(void* arg UNUSED) {
    while (1);
}

/* 测试用户进程 */
void u_prog_a(void) {
    printf("I am u_prog_a\n");
    while (1);
}

/* 测试用户进程 */
void u_prog_b(void) {
    while (1);
}
