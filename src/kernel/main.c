/*
 * @Author: lyajpunov 1961558693@qq.com
 * @Date: 2024-03-26 07:37:49
 * @LastEditors: lyajpunov 1961558693@qq.com
 * @LastEditTime: 2024-04-03 01:44:54
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

    // char buf[512] = { 0 };
    // ide_write(cur_part->my_disk, 0xB68, buf, 1);

    // // 目录项指正指向buf
    // struct dir_entry* p_de = (struct dir_entry*)buf;
    // // 初始化当前目录
    // memcpy(p_de->filename, ".", 1);
    // p_de->i_no = 0;
    // p_de->f_type = FT_DIRECTORY;
    // p_de++;
    // // 初始化当前目录的父目录
    // memcpy(p_de->filename, "..", 2);
    // p_de->i_no = 0;   // 根目录的父目录依然是根目录自己
    // p_de->f_type = FT_DIRECTORY;
    // // 将目录项写入硬盘
    // ide_write(cur_part->my_disk, cur_part->sb->data_start_lba, buf, 1);



    // struct path_search_record  temp;
    // memset(&temp, 0, sizeof(struct path_search_record));
    // uint32_t fd_ino = search_file("/file1", &temp);
    // printk("%d\n", fd_ino);

    // int fd;
    // fd = sys_open("/file1", O_CREAT);
    // fd = sys_open("/file2", O_CREAT);
    // fd = sys_open("/file3", O_CREAT);
    // fd = sys_open("/file4", O_CREAT);

    // int fd = sys_open("/file1", O_RDWR);
    // printk("open /file1, fd:%d\n", fd);
    // char buf[64] = "lyjlyj123123lyjlyj123123 hahhaha";
    // sys_write(fd, buf, strlen(buf));
    // sys_close(fd);


    // fd = sys_open("/file1", O_RDWR);
    // memset(buf, 0, 64);
    // int read_bytes = sys_read(fd, buf, 18);
    // printk("1_ read %d bytes:%s\n", read_bytes, buf);

    // memset(buf, 0, 64);
    // read_bytes = sys_read(fd, buf, 6);
    // printk("2_ read %d bytes:%s\n", read_bytes, buf);

    // memset(buf, 0, 64);
    // read_bytes = sys_read(fd, buf, 6);
    // printk("3_ read %d bytes:%s\n", read_bytes, buf);
    // sys_close(fd);


    char buf[64] = { 0 };
    int fd = sys_open("/file1", O_RDWR);
    int read_bytes = sys_read(fd, buf, 6);
    printk("1_ read %d bytes:\n%s", read_bytes, buf);

    read_bytes = sys_read(fd, buf, 6);
    printk("1_ read %d bytes:\n%s", read_bytes, buf);

    read_bytes = sys_read(fd, buf, 18);
    printk("1_ read %d bytes:\n%s", read_bytes, buf);

    sys_close(fd);

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
