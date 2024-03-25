#include "ioqueue.h"
#include "interrupt.h"
#include "assert.h"

/* 初始化io队列ioq */
void ioqueue_init(struct ioqueue* ioq) {
    lock_init(&ioq->lock);                 // 初始化io队列的锁
    ioq->producer = ioq->consumer = NULL;  // 生产者和消费者置空
    ioq->head = ioq->tail = 0;             // 队列的首尾指针指向缓冲区数组第0个位置
}

/* 返回pos在缓冲区中的下一个位置值 */
static inline int32_t next_pos(int32_t pos) {
    return (pos + 1) % bufsize;
}

/* 判断队列是否已满 */
bool ioq_full(struct ioqueue* ioq) {
    return next_pos(ioq->head) == ioq->tail;
}

/* 判断队列是否已空 */
bool ioq_empty(struct ioqueue* ioq) {
    return ioq->head == ioq->tail;
}

/* 使当前生产者或消费者在此缓冲区上等待 */
static void ioq_wait(struct task_struct** waiter) {
    // 二级指针不为空，指向的pcb指针地址为空
    ASSERT(*waiter == NULL && waiter != NULL);
    *waiter = running_thread();
    thread_block(TASK_BLOCKED);
}

/* 唤醒waiter */
static void wakeup(struct task_struct** waiter) {
    // 二级指针指向不为空
    ASSERT(*waiter != NULL);
    thread_unblock(*waiter);
    *waiter = NULL;
}

/* 消费者从ioq队列中获取一个字符 */
char ioq_getchar(struct ioqueue* ioq) {
    // 若缓冲区(队列)为空,把消费者ioq->consumer记为当前线程自己,等待生产者唤醒
    while (ioq_empty(ioq)) {
        lock_acquire(&ioq->lock);
        ioq_wait(&ioq->consumer);
        lock_release(&ioq->lock);
    }

    char byte = ioq->buf[ioq->tail];	  // 从缓冲区中取出
    ioq->tail = next_pos(ioq->tail);	  // 把读游标移到下一位置

    if (ioq->producer != NULL) {
        wakeup(&ioq->producer);		      // 唤醒生产者
    }

    return byte;
}

/* 生产者往ioq队列中写入一个字符byte */
void ioq_putchar(struct ioqueue* ioq, char byte) {
    // 若缓冲区(队列)已经满了,把生产者ioq->producer记为自己,等待消费者线程唤醒自己
    while (ioq_full(ioq)) {
        lock_acquire(&ioq->lock);
        ioq_wait(&ioq->producer);
        lock_release(&ioq->lock);
    }
    ioq->buf[ioq->head] = byte;      // 把字节放入缓冲区中
    ioq->head = next_pos(ioq->head); // 把写游标移到下一位置

    if (ioq->consumer != NULL) {
        wakeup(&ioq->consumer);      // 唤醒消费者
    }
}

