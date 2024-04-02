// os/src/kernel/main.c
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
#include "super_block.h"

void k_thread_a(void*);
void k_thread_b(void*);
void u_prog_a(void);
void u_prog_b(void);
extern struct tm time;

int main(void) {
    init_all();
    intr_enable();
    process_execute(u_prog_a, "u_prog_a");
    process_execute(u_prog_b, "u_prog_b");
    thread_start("k_thread_a", k_thread_a, "I am thread_a");
    thread_start("k_thread_b", k_thread_b, "I am thread_b");

    // printk("%d \n", cur_part->sb->data_start_lba);
    // sys_open("/file1", O_CREAT);
    uint32_t fd = sys_open("/file1", O_RDONLY);
    printk("fd:%d\n", fd);
    sys_close(fd);
    printk("%d closed now\n", fd);

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
    while (1);
}

/* 测试用户进程 */
void u_prog_b(void) {
    while (1);
}
