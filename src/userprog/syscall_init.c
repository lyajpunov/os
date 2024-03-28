#include "syscall_init.h"
#include "syscall.h"
#include "stdin.h"
#include "print.h"
#include "thread.h"

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
   put_str("syscall_init done!\n");
}
