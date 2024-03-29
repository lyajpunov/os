#ifndef __LIB_USER_SYSCALL_H
#define __LIB_USER_SYSCALL_H

#include "stdin.h"

enum SYSCALL_NR {
    SYS_GETPID,               // 0号调用：获取当前线程的pid
    SYS_WRITE,                // 1号调用：把buf中count个字符写到文件描述符fd指向的文件中
    SYS_MALLOC,               // 2号调用：申请内存
    SYS_FREE,                 // 3号调用：释放内存
};

/* 获取当前进程的pid */
uint32_t getpid(void);
/* 把buf中count个字符写到文件描述符fd指向的文件中 */
uint32_t write(char* str); 
/* 申请堆内存 */
void* malloc(uint32_t size);
/* 释放堆内存 */
void free(void* ptr);
#endif

