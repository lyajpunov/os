// os/src/kernel/init.c
#include "init.h"
#include "interrupt.h"
#include "timer.h"

void init_all(void) {
    /* 1、初始化中断 */
    idt_init();
    /* 2、初始化时钟*/
    timer_init();
}