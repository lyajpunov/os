// os/src/kernel/init.c
#include "init.h"
#include "interrupt.h"
#include "timer.h"
#include "memory.h"
#include "thread.h"
#include "console.h"
#include "keyboard.h"

void init_all(void) {
    /* 1、初始化中断 */
    idt_init();
    /* 2、初始化时钟*/
    timer_init();
    /* 3、内存初始化 */
    mem_init();
    /* 4、线程初始化*/
    thread_init();
    /* 5、终端初始化 */
    console_init();
    /* 6、键盘初始化 */
    keyboard_init();
}