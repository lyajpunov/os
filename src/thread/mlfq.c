/* 一个多级反馈优先队列
 * 1、线程的初始优先级最高，时间片最少
 * 2、经过一次调度则其优先级降低，时间片增多（判断其为计算型任务，而非交互性）
 * 3、如果主动放弃CPU，则优先级不变，时间片不变
 * 4、经过一段时间，主动刷新，将所有任务都置于最高优先级
 */
#include "mlfq.h"
#include "thread.h"
#include "print.h"
#include "interrupt.h"

struct list thread_ready_list4;	    // 就绪队列4
struct list thread_ready_list8;	    // 就绪队列8
struct list thread_ready_list16;	// 就绪队列16
struct list thread_ready_list32;	// 就绪队列32
struct list thread_all_list;	    // 所有任务队列

/* 多级反馈优先队列新插入一个线程 */
void mlfq_new(struct task_struct* pthread) {
    // 关闭中断
    enum intr_status pop = intr_disable();
    // 修改线程可用时间片
    pthread->ticks = 4;
    // 修改线程优先级
    pthread->priority = 4;
    // 最低层队列插入
    list_append(&thread_ready_list4, &pthread->general_tag);
    // 所有任务队列插入
    list_append(&thread_all_list, &pthread->all_tag);
    // 开启中断
    intr_set_status(pop);
}

/* 所有线程队列插入一个线程 */
void all_push_back(struct task_struct* pthread) {
    // 关闭中断
    enum intr_status pop = intr_disable();
    // 所有线程队列中没有新插入的线程
    if (!elem_find(&thread_all_list, &pthread->all_tag)) {
        // 所有任务队列插入
        list_append(&thread_all_list, &pthread->all_tag);
    }
    // 开启中断
    intr_set_status(pop);
}
 
/* 多级反馈优先队列插入一个线程, 优先级降低，时间片变多*/
void mlfq_push(struct task_struct* pthread) {
    if (pthread == NULL) return;
    if (mlfq_find(pthread)) return;
    // 关闭中断
    enum intr_status mlqf = intr_disable();
    // 线程优先级,高优先级的先降级，但是时间片变多
    // 不知道什么优先级的，就先按照4来。
    if (pthread->priority == 4) {
        pthread->priority = 8;
        pthread->ticks = 8;
        list_append(&thread_ready_list8, &pthread->general_tag);
    }
    else if (pthread->priority == 8) {
        pthread->priority = 16;
        pthread->ticks = 16;
        list_append(&thread_ready_list16, &pthread->general_tag);
    }
    else if (pthread->priority == 16) {
        pthread->priority = 32;
        pthread->ticks = 32;
        list_append(&thread_ready_list32, &pthread->general_tag);
    }
    else if (pthread->priority == 32) {
        pthread->priority = 32;
        pthread->ticks = 32;
        list_append(&thread_ready_list32, &pthread->general_tag);
    }
    else {
        pthread->priority = 4;
        pthread->ticks = 4;
        list_append(&thread_ready_list4, &pthread->general_tag);
    }
    // 开启中断
    intr_set_status(mlqf);
}

/* 多级反馈优先队列插入一个线程, 优先级不变，时间片不变,with same priority and timeslice*/
void mlfq_push_wspt(struct task_struct* pthread) {
    if (pthread == NULL) return;
    if (mlfq_find(pthread)) return;
    // 关闭中断
    enum intr_status mlqf = intr_disable();
    // 线程优先级,高优先级的先降级，但是时间片变多
    // 不知道什么优先级的，就先按照4来。
    if (pthread->priority == 4) {
        list_append(&thread_ready_list8, &pthread->general_tag);
    }
    else if (pthread->priority == 8) {
        list_append(&thread_ready_list16, &pthread->general_tag);
    }
    else if (pthread->priority == 16) {
        list_append(&thread_ready_list32, &pthread->general_tag);
    }
    else if (pthread->priority == 32) {
        list_append(&thread_ready_list32, &pthread->general_tag);
    }
    else {
        pthread->priority = 4;
        pthread->ticks = 4;
        list_append(&thread_ready_list4, &pthread->general_tag);
    }
    // 开启中断
    intr_set_status(mlqf);
}

/* 多级反馈优先队列弹出一个线程,队列全为空则返回NULL */
struct task_struct* mlfq_pop(void) {
    if (mlfq_is_empty()) return NULL;
    // 关闭中断
    enum intr_status mlfq = intr_disable();
    struct task_struct* pthread = NULL;
    struct list_elem* pelem = NULL;
    if (!list_empty(&thread_ready_list4)) {
        pelem = list_pop(&thread_ready_list4);

    }
    else if (!list_empty(&thread_ready_list8)) {
        pelem = list_pop(&thread_ready_list8);
    }
    else if (!list_empty(&thread_ready_list16)) {
        pelem = list_pop(&thread_ready_list16);
    }
    else if (!list_empty(&thread_ready_list32)) {
        pelem = list_pop(&thread_ready_list32);
    }
    pthread = elem2entry(struct task_struct, general_tag, pelem);
    // 开启中断
    intr_set_status(mlfq);
    return pthread;
}

/* 多级反馈优先队列判断是否为空，是返回true */
bool mlfq_is_empty(void) {
    return (list_empty(&thread_ready_list4) && list_empty(&thread_ready_list8) \
        && list_empty(&thread_ready_list16) && list_empty(&thread_ready_list32));
}

/* 多级返回优先队列查找，找到返回true */
bool mlfq_find(struct task_struct* pthread) {
    if (elem_find(&thread_ready_list4, &pthread->general_tag)) return true;
    else if (elem_find(&thread_ready_list4, &pthread->general_tag)) return true;
    else if (elem_find(&thread_ready_list4, &pthread->general_tag)) return true;
    else if (elem_find(&thread_ready_list4, &pthread->general_tag)) return true;
    return false;
}

/* 多级返回优先队列长度 */
uint32_t mlfq_len(void) {
    return list_len(&thread_ready_list4) + list_len(&thread_ready_list8) \
        + list_len(&thread_ready_list16) + list_len(&thread_ready_list32);
}

/* 多级反馈优先队列刷新,将低优先级线程往上提 */
void mlfq_flash(void) {
    // 关闭中断
    enum intr_status mlfq = intr_disable();

    struct list_elem* temp = NULL;
    while (!list_empty(&thread_ready_list8)) {
        temp = list_pop(&thread_ready_list8);
        list_append(&thread_ready_list8, temp);
    }
    while (!list_empty(&thread_ready_list16)) {
        temp = list_pop(&thread_ready_list16);
        list_append(&thread_ready_list16, temp);
    }
    while (!list_empty(&thread_ready_list32)) {
        temp = list_pop(&thread_ready_list32);
        list_append(&thread_ready_list32, temp);
    }
    // 开启中断
    intr_set_status(mlfq);
}

/* 多级反馈优先队列初始化 */
void mlfq_init(void) {
    put_str("mlfq_init start!\n");
    list_init(&thread_ready_list4);
    list_init(&thread_ready_list8);
    list_init(&thread_ready_list16);
    list_init(&thread_ready_list32);
    list_init(&thread_all_list);
    put_str("mlfq_init done!\n");
}