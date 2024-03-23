#ifndef __THREAD_SYNC_H
#define __THREAD_SYNC_H

#include "list.h"
#include "stdin.h"
#include "thread.h"

/* 信号量结构 */
struct semaphore {
   uint8_t  value;
   struct   list waiters;
};

/* 锁结构 */
struct lock {
   struct   task_struct* holder;	    // 锁的持有者
   struct   semaphore semaphore;	    // 用二元信号量实现锁
   uint32_t holder_repeat_nr;		    // 锁的持有者重复申请锁的次数
};

/* 二元信号量初始化 */
void sema_init(struct semaphore* psema, uint8_t value);
/* 信号down操作 */
void sema_down(struct semaphore* psema);
/* 信号up操作 */
void sema_up(struct semaphore* psema);
/* 锁的初始化 */
void lock_init(struct lock* plock);
/* 获得锁 */
void lock_acquire(struct lock* plock);
/* 释放锁 */
void lock_release(struct lock* plock);
#endif
