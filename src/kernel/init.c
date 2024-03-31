// os/src/kernel/init.c
#include "init.h"
#include "interrupt.h"
#include "timer.h"
#include "memory.h"
#include "thread.h"
#include "console.h"
#include "keyboard.h"
#include "tss.h"
#include "syscall_init.h"
#include "ide.h"
#include "cmos.h"

void init_all(void) {
    /* 1、初始化中断 */
    idt_init();
    /* 2、时间初始化 */
    time_init();
    /* 3、初始化时钟*/
    timer_init();
    /* 4、内存初始化 */
    mem_init();
    /* 5、线程初始化*/
    thread_init();
    /* 6、终端初始化 */
    console_init();
    /* 7、键盘初始化 */
    keyboard_init();
    /* 8、TSS状态段初始化 */
    tss_init();
    /* 9、系统调用初始化 */
    syscall_init();
    /* 10、硬盘驱动初始化 */
    ide_init();
}