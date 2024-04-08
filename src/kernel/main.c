/*
 * @Author: lyajpunov 1961558693@qq.com
 * @Date: 2024-03-26 07:37:49
 * @LastEditors: lyajpunov 1961558693@qq.com
 * @LastEditTime: 2024-04-08 00:57:43
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
    while (1);
    return 0;
}


/* init进程 */
void init(void) {
    uint32_t ret_pid = fork();
    if (ret_pid) {
        printf("i am father, my pid is %d, child pid is %d\n", getpid(), ret_pid);
    }
    else {
        printf("i am child, my pid is %d, ret pid is %d\n", getpid(), ret_pid);
    }
    while (1);
}

