// os/src/kernel/main.c

#include "print.h"
#include "init.h"

int main(void) {
    init_all();
    asm volatile("sti");	     // 为演示中断处理,在此临时开中断
    while (1) ;
    return 0;
}