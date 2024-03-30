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

void k_thread_a(void*);
void k_thread_b(void*);
void u_prog_a(void);
void u_prog_b(void);

int main(void) {
    put_str("I am kernel\n");
    init_all();
    intr_enable();
    process_execute(u_prog_a, "u_prog_a");
    process_execute(u_prog_b, "u_prog_b");
    thread_start("k_thread_a", k_thread_a, "I am thread_a");
    thread_start("k_thread_b", k_thread_b, "I am thread_b");
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
