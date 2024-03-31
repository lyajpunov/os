// os/src/device/timer.h
#ifndef __DEVICE_TIMER_H
#define __DEVICE_TIMER_H

#include "stdin.h"

// 总滴答数
extern uint64_t ticks;

/* 时钟初始化 */
void timer_init(void);
/* 休眠函数 */
void mtime_sleep(uint32_t m_seconds);

#endif

