// os/src/kernel/main.c
#include "print.h"
#include "init.h"
#include "thread.h"
#include "interrupt.h"
#include "console.h"
#include "process.h"
#include "syscall.h"
#include "syscall_init.h"

void k_thread_a(void*);
void k_thread_b(void*);
void u_prog_a(void);
void u_prog_b(void);
int test_var_a = 0, test_var_b = 0;

int main(void) {
    init_all();
    console_put_str("I am main thread, my pid is");
    console_put_hex(sys_getpid());
    console_put_str("\n");

    process_execute(u_prog_a, "user_prog_a");
    process_execute(u_prog_b, "user_prog_b");

    thread_start("k_thread_a", k_thread_a, NULL);
    thread_start("k_thread_b", k_thread_b, NULL);

    intr_enable();
    while(1);
    return 0;
}

/* 在线程中运行的函数 */
void k_thread_a(void* arg) {
    int i = 10000000;
    while (i--);
    console_put_str("k_thread_a's pid is");
    console_put_hex(sys_getpid());
    console_put_str("\n");
    console_put_str("u_prog_a's pid is");
    console_put_hex(test_var_a);
    console_put_str("\n");
    while (1);
}

/* 在线程中运行的函数 */
void k_thread_b(void* arg) {
    int i = 10000000;
    while (i--);
    console_put_str("k_thread_b's pid is");
    console_put_hex(sys_getpid());
    console_put_str("\n");
    console_put_str("u_prog_b's pid is");
    console_put_hex(test_var_b);
    console_put_str("\n");
    while (1);
}

/* 测试用户进程 */
void u_prog_a(void) {
    test_var_a = getpid();
    while (1);
}

/* 测试用户进程 */
void u_prog_b(void) {
    test_var_b = getpid();
    while (1);
}
