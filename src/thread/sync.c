#include "sync.h"
#include "list.h"
#include "global.h"
#include "interrupt.h"
#include "assert.h"

/* 初始化信号量 */
void sema_init(struct semaphore* psema, uint8_t value) {
    psema->value = value;         // 为信号量赋初值
    list_init(&psema->waiters);   // 初始化信号量的等待队列
}

/* 信号量down操作 */
void sema_down(struct semaphore* psema) {
    enum intr_status old_status = intr_disable();

    // value == 0 表示被别人持有了这个锁,如果被别人持有，那么阻塞当前线程，将其加入到信号等待队列
    while (psema->value == 0) {
        // 确保当前线程不在等待队列中
        ASSERT(!elem_find(&psema->waiters, &running_thread()->general_tag));
        // 加入等待队列，阻塞自己
        list_append(&psema->waiters, &running_thread()->general_tag);
        thread_block(TASK_BLOCKED);
    }
    // value为1，则表示当前线程获得了锁
    psema->value--;

    intr_set_status(old_status);
}

/* 信号量的up操作 */
void sema_up(struct semaphore* psema) {
    enum intr_status old_status = intr_disable();

    // 执行up操作相当于释放锁，如果释放的过程中发现线程队列里面还有等待的线程，那么直接唤醒他
    if (!list_empty(&psema->waiters)) {
        struct task_struct* thread_blocked = elem2entry(struct task_struct, general_tag, list_pop(&psema->waiters));
        thread_unblock(thread_blocked);
    }
    psema->value++;

    intr_set_status(old_status);
}

/* 初始化锁plock */
void lock_init(struct lock* plock) {
    plock->holder = NULL;
    plock->holder_repeat_nr = 0;
    sema_init(&plock->semaphore, 1);
}

/* 获取锁plock */
void lock_acquire(struct lock* plock) {
    /* 排除曾经自己已经持有锁但还未将其释放的情况。*/
    if (plock->holder != running_thread()) {
        sema_down(&plock->semaphore);    // 对信号量P操作,原子操作
        plock->holder = running_thread();
        plock->holder_repeat_nr = 1;
    }
    else {
        plock->holder_repeat_nr++;
    }
}

/* 释放锁plock */
void lock_release(struct lock* plock) {
    ASSERT(plock->holder == running_thread());
    if (plock->holder_repeat_nr > 1) {
        plock->holder_repeat_nr--;
        return;
    }
    ASSERT(plock->holder_repeat_nr == 1);

    plock->holder = NULL;	       // 把锁的持有者置空放在V操作之前
    plock->holder_repeat_nr = 0;
    sema_up(&plock->semaphore);	   // 信号量的V操作,也是原子操作
}




