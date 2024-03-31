// os/src/kernel/main.c
#include "print.h"
#include "init.h"
#include "thread.h"
#include "interrupt.h"
#include "console.h"
#include "process.h"
#include "syscall_init.h"
#include "syscall.h"
#include "stdio.h"
#include "memory.h"
#include "ide.h"
#include "cmos.h"
#include "timer.h"
#include "math.h"
#include "assert.h"

void k_thread_a(void*);
void k_thread_b(void*);
void u_prog_a(void);
void u_prog_b(void);
extern struct tm time;



int main(void) {
    put_str("I am kernel\n");
    init_all();
    intr_enable();
    process_execute(u_prog_a, "u_prog_a");
    process_execute(u_prog_b, "u_prog_b");
    thread_start("k_thread_a", k_thread_a, "I am thread_a");
    thread_start("k_thread_b", k_thread_b, "I am thread_b");

    uint64_t a = 0x1111111111111111;

    printk("a = %lu\n", a);

    // 创建时间结构体
    struct tm* t = (struct tm*)sys_malloc(sizeof(struct tm));

    // 开机时间
    t = &time;
    printk("time now is %d-%d-%d,%d:%d:%d\n", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);


    // 获取unix时间戳
    uint64_t now_time = get_time();
    // 将时间戳转换为时间
    timestamp_to_datetime(now_time, t);
    // 打印当前时间戳与时间
    printk("unix is %lu\n", now_time);
    printk("time now is %d-%d-%d,%d:%d:%d\n", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);


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
