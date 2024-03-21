// os/src/device/timer.c
#include "timer.h"
#include "io.h"
#include "print.h"
#include "thread.h"
#include "assert.h"
#include "interrupt.h"

#define IRQ0_FREQUENCY     100               // IR0需要的频率
#define INPUT_FREQUENCY    1193180           // 8253的输入频率
#define COUNTER0_VALUE     INPUT_FREQUENCY / IRQ0_FREQUENCY 
#define CONTRER0_PORT      0x40              // 计数器0的端口
#define CONTRER0_NO        0                 // 选择计数器0
#define CONTRER0_MODE      2                 // 选择工作模式2,比率发生器，即分频
#define CONTRER0_RWL       3                 // 读写方式位先读写低字节再读写高字节
#define COUNTER0_BCD       0                 // 采用二进制方式
#define PIT_CONTROL_PORT   0x43              // 控制端口

/**********************
@author: liyajun
@data: 2024.3.21 20：21
@description: ticks是内核自中断开启以来总共的嘀嗒数
***********************/
uint32_t ticks;

static void frequency_set(uint8_t counter_port, uint8_t counter_no, uint8_t counter_rwl, \
    uint8_t counter_mode, uint8_t counter_bcd, uint16_t counter_value) {
    outb(counter_port, (uint8_t)(counter_no << 6 | counter_rwl << 4 | counter_mode << 1 | counter_bcd));
    outb(counter_port, (uint8_t)counter_value);
    outb(counter_port, (uint8_t)(counter_value >> 8));
}

/**********************
@author: liyajun
@data: 2024.3.21 20：20
@description: 时钟中断处理函数
***********************/
static void intr_timer_handler(void) {
    // 获得当前的线程
    struct task_struct* cur_thread = running_thread();
    // 检查栈是否溢出
    ASSERT(cur_thread->stack_magic == 0x19870916);
    // 记录此线程占用的cpu时间嘀
    cur_thread->elapsed_ticks++;
    // 从内核第一次处理时间中断后开始至今的滴哒数,内核态和用户态总共的嘀哒数
    ticks++;

    // 当前任务的时间片用完就换下
    if (cur_thread->ticks == 0) {
        schedule();
    }
    else {
        cur_thread->ticks--;
    }
}


/**********************
@author: liyajun
@data: 2024.3.21 20：04
@description: 定时器初始化
***********************/
void timer_init() {
    put_str("timer_init start!\n");
    frequency_set(CONTRER0_PORT, CONTRER0_NO, CONTRER0_RWL, CONTRER0_MODE, COUNTER0_BCD, COUNTER0_VALUE);
    register_handler(0x20, intr_timer_handler);
    put_str("timer_init end!\n");
}

