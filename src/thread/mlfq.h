/*
 * @Author: lyajpunov 1961558693@qq.com
 * @Date: 2024-03-26 07:37:49
 * @LastEditors: lyajpunov 1961558693@qq.com
 * @LastEditTime: 2024-04-08 02:17:59
 * @FilePath: /os/src/thread/mlfq.h
 * @Description: 多级就绪队列
 * 
 * Copyright (c) 2024 by ${git_name_email}, All Rights Reserved. 
 */
#ifndef __THREAD_MLFQ_H
#define __THREAD_MLFQ_H

#include "thread.h"

extern struct list thread_all_list;	    // 所有任务队列

/* 多级反馈优先队列新插入一个线程 */
void mlfq_new(struct task_struct* pthread);
/* 多级反馈优先队列插入一个线程, 优先级降低，时间片变多*/
void mlfq_push(struct task_struct* pthread);
/* 多级反馈优先队列插入一个线程, 优先级不变，时间片不变*/
void mlfq_push_wspt(struct task_struct* pthread);
/* 所有线程队列插入一个线程 */
void all_push_back(struct task_struct* pthread);
/* 多级反馈优先队列弹出一个线程 */
struct task_struct* mlfq_pop(void);
/* 多级反馈优先队列判断是否为空，是返回true */
bool mlfq_is_empty(void);
/* 多级返回优先队列查找，找到返回true */
bool mlfq_find(struct task_struct* pthread);
/* 多级返回优先队列长度 */
uint32_t mlfq_len(void);
/* 多级反馈优先队列刷新,将低优先级线程往上提 */
void mlfq_flash(void);
/* 多级反馈优先队列初始化 */
void mlfq_init(void);


#endif