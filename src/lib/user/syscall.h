#ifndef __LIB_USER_SYSCALL_H
#define __LIB_USER_SYSCALL_H

#include "stdin.h"

enum SYSCALL_NR {
    SYS_GETPID               // 0号调用：获取当前线程的pid
};

/* 获取当前进程的pid */
uint32_t getpid(void);

#endif

