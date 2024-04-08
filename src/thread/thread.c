/*
 * @Author: lyajpunov 1961558693@qq.com
 * @Date: 2024-03-26 07:37:49
 * @LastEditors: lyajpunov 1961558693@qq.com
 * @LastEditTime: 2024-04-08 05:37:32
 * @FilePath: /os/src/thread/thread.c
 * @Description: 线程的主要代码
 *
 * Copyright (c) 2024 by ${git_name_email}, All Rights Reserved.
 */
#include "thread.h"
#include "stdin.h"
#include "string.h"
#include "memory.h"
#include "print.h"
#include "interrupt.h"
#include "mlfq.h"
#include "assert.h"
#include "process.h"
#include "sync.h"
#include "syscall.h"
#include "stdio.h"
#include "shell.h"
#include "file.h"
#include "fs.h"
#include "list.h"

struct task_struct* main_thread;    // 主线程PCB，也就是我们刚进内核的程序，现在运行的程序
struct task_struct* idle_thread;    // idle线程PCB，空闲线程，不空转浪费CPU
struct lock pid_lock;               // 分配pid锁

/* 线程转换从cur到next */
extern void switch_to(struct task_struct* cur, struct task_struct* next);

/* 获取当前线程pcb指针 */
struct task_struct* running_thread() {
    // 原理是拿到栈顶地址
    uint32_t esp;
    asm("mov %%esp, %0" : "=g" (esp));
    return (struct task_struct*)(esp & 0xfffff000);
}


/**
 * @description: 分配pid
 * @return {*} pid值
 */
int32_t pid_allocate(void) {
    // pid变量,这里是静态的，可以一直加上去
    static uint32_t pid_flag = 0;
    lock_acquire(&pid_lock);
    pid_flag++;
    lock_release(&pid_lock);
    return pid_flag;
}

/* 由kernel_thread去执行function(func_arg) */
static void kernel_thread(thread_func* function, void* func_arg) {
    // 要先开中断，否则后面的线程无法调度，这里调度的可能进程也可能是线程，都需要开中断
    intr_enable();
    function(func_arg);
}

/* 初始化线程栈thread_stack,将待执行的函数和参数放到thread_stack中相应的位置 */
void thread_create(struct task_struct* pthread, thread_func function, void* func_arg) {
    /* 先预留中断使用栈的空间*/
    pthread->self_kstack -= sizeof(struct intr_stack);
    /* 再留出线程栈空间 */
    pthread->self_kstack -= sizeof(struct thread_stack);
    /* 拿到线程栈，并初始化 */
    struct thread_stack* kthread_stack = (struct thread_stack*)pthread->self_kstack;
    /* 这些内容是设置线程栈，eip指向要执行的程序*/
    kthread_stack->eip = kernel_thread;
    kthread_stack->function = function;
    kthread_stack->func_arg = func_arg;
    kthread_stack->ebp = 0;
    kthread_stack->ebx = 0;
    kthread_stack->edi = 0;
    kthread_stack->esi = 0;
}

/* 初始化线程基本信息,name:线程名，prio:线程优先级 */
void init_thread(struct task_struct* pthread, char* name) {
    memset(pthread, 0, sizeof(*pthread));
    strcpy(pthread->name, name);

    if (pthread == main_thread) {
        pthread->status = TASK_RUNNING;
    }
    else {
        pthread->status = TASK_READY;
    }
    // 文件描述符数组
    pthread->fd_table[0] = 0;
    pthread->fd_table[1] = 1;
    pthread->fd_table[2] = 2;
    for (int i = 3; i < MAX_FILES_OPEN_PER_PROC; i++) {
        pthread->fd_table[i] = -1;
    }
    // 文件优先级
    pthread->priority = 4;
    // 用户进程在进程初始化时处理，内核线程为NULL
    pthread->pgdir = NULL;
    // 线程pid
    pthread->pid = pid_allocate();
    // 线程栈顶
    pthread->self_kstack = (uint32_t*)((uint32_t)pthread + PG_SIZE);
    // 以根目录作为默认工作路径
    pthread->cwd_inode_nr = 0;
    // 线程魔数
    pthread->stack_magic = PCB_MAGIC;
}

/* 创建一个名为name，优先级为prio，运行函数function，函数参数func_arg的线程 */
struct task_struct* thread_start(char* name, thread_func function, void* func_arg) {
    struct task_struct* thread = get_kernel_pages(1);    // pcb程序控制块均位于内核空间，因为这部分内容是不允许用户知晓的
    init_thread(thread, name);                           // 初始化线程的基本信息
    thread_create(thread, function, func_arg);           // 创建线程
    mlfq_new(thread);                                    // 第一次插入多级反馈队列
    return thread;
}

/* 将kernel中的main函数完善为主线程 */
static void make_main_thread(void) {
    main_thread = running_thread();
    init_thread(main_thread, "main");
    // main 函数是当前线程，当前线程不在就绪队列中，所以只是将其加入所有线程队列
    // 确保这个主线程之前不在所有线程中
    all_push_back(main_thread);
}

/* 切换任务 */
void schedule(void) {
    ASSERT(intr_get_status() == INTR_OFF);

    // 获得当前正在运行的程序的线程pcb
    struct task_struct* cur = running_thread();
    // 若此线程时间片到了，那么将其重新加入到反馈优先队列
    if (cur->status == TASK_RUNNING) {
        // 将此线程加入就绪队列，并修改状态为就绪态
        mlfq_push(cur);
    }
    else {
        // 若此线程需要某事件发生后才能继续上cpu运行,不需要将其加入队列,因为当前线程不在就绪队列中
    }

    // 如果没有可运行的任务,就唤醒idle，
    if (mlfq_is_empty()) {
        thread_unblock(idle_thread);
    }

    // 从就绪队列中弹出一个任务，如果没有可运行的任务的话，现在上CPU的就是idle
    struct task_struct* next = mlfq_pop();
    // 将就绪队列的任务的状态改为运行态
    next->status = TASK_RUNNING;
    // 激活任务页表等
    process_activate(next);
    // 切换两个任务
    switch_to(cur, next);
}

/* 当前线程将自己阻塞,标志其状态为stat. */
void thread_block(enum task_status stat) {
    enum intr_status old_status = intr_disable();
    struct task_struct* cur_thread = running_thread();
    cur_thread->status = stat; // 置其状态为stat 
    schedule();		           // 将当前线程换下处理器
    intr_set_status(old_status);
}

/**
 * @description: 将线程pthread解除阻塞
 * @param {task_struct*} pthread
 * @return {*}
 */
void thread_unblock(struct task_struct* pthread) {
    if (pthread->status != TASK_READY) {
        pthread->priority = 4;      // 将当前线程的优先级置位4，使其优先得到调度
        pthread->status = TASK_READY;
        mlfq_push_wspt(pthread);
    }
}

/**
 * @description: 主动放弃CPU的使用
 * @return {*}
 */
void thread_yield(void) {
    enum intr_status old_status = intr_disable();
    struct task_struct* cur = running_thread();
    cur->status = TASK_READY;
    // 不改变其优先级和时间片
    mlfq_push_wspt(cur);
    schedule();
    intr_set_status(old_status);
}

/**
 * @description: 以填充空格的方式输出buf
 * @param {char*} buf 
 * @param {int32_t} buf_len
 * @param {void*} ptr
 * @param {char} format
 * @return {*}
 */
static void pad_print(char* buf, int32_t buf_len, void* ptr, char format) {
    memset(buf, 0, buf_len);
    uint8_t out_pad_0idx = 0;
    switch (format) {
    case 's':
        out_pad_0idx = sprintf(buf, "%s", ptr);
        break;
    case 'd':
        out_pad_0idx = sprintf(buf, "%d", *((int32_t*)ptr));
        break;
    case 'x':
        out_pad_0idx = sprintf(buf, "%x", *((uint32_t*)ptr));
        break;
    }
    while (out_pad_0idx < buf_len) {
        // 以空格填充
        buf[out_pad_0idx] = ' ';
        out_pad_0idx++;
    }
    sys_write(stdout_no, buf, buf_len - 1);
}


/**
 * @description: 用于在list_traversal函数中的回调函数,用于针对线程队列的处理
 * @param {list_elem*} pelem
 * @param {int arg} UNUSED
 * @return {*}
 */
static bool elem2thread_info(struct list_elem* pelem, int arg UNUSED) {
    struct task_struct* pthread = elem2entry(struct task_struct, all_tag, pelem);
    char out_pad[16] = { 0 };

    pad_print(out_pad, 16, &pthread->pid, 'd');

    if (pthread->parent_pid == -1) {
        pad_print(out_pad, 16, "NULL", 's');
    }
    else {
        pad_print(out_pad, 16, &pthread->parent_pid, 'd');
    }

    switch (pthread->status) {
    case 0:
        pad_print(out_pad, 16, "RUNNING", 's');
        break;
    case 1:
        pad_print(out_pad, 16, "READY", 's');
        break;
    case 2:
        pad_print(out_pad, 16, "BLOCKED", 's');
        break;
    case 3:
        pad_print(out_pad, 16, "WAITING", 's');
        break;
    case 4:
        pad_print(out_pad, 16, "HANGING", 's');
        break;
    case 5:
        pad_print(out_pad, 16, "DIED", 's');
    }
    pad_print(out_pad, 16, &pthread->elapsed_ticks, 'x');

    memset(out_pad, 0, 16);
    memcpy(out_pad, pthread->name, strlen(pthread->name));
    strcat(out_pad, "\n");
    sys_write(stdout_no, out_pad, strlen(out_pad));
    // 此处返回false是为了迎合主调函数list_traversal,只有回调函数返回false时才会继续调用此函数
    return false;
}

/**
 * @description: 打印任务列表
 * @return {*}
 */
void sys_ps(void) {
    char* ps_title = "PID            PPID           STAT           TICKS          COMMAND\n";
    sys_write(stdout_no, ps_title, strlen(ps_title));
    list_traversal(&thread_all_list, elem2thread_info, 0);
}

/**
 * @description: idle进程
 * @param {void* arg} UNUSED
 * @return {*}
 */
static void idle(void* arg UNUSED) {
    while (1) {
        thread_block(TASK_BLOCKED);
        //执行hlt时必须要保证目前处在开中断的情况下
        asm volatile ("sti; hlt" : : : "memory");
    }
}

/**
 * @description: init进程
 * @return {*}
 */
static void init_th(void) {
    uint32_t ret_pid = fork();
    if (ret_pid) {
        // init父线程
    }
    else {
        // 子线程
        my_shell();
    }
    while (1);
}

/**
 * @description: 初始化线程环境
 * @return {*}
 */
void thread_init(void) {
    put_str("thread_init start\n");

    // pid锁初始化
    lock_init(&pid_lock);
    // 多级队列初始化
    mlfq_init();
    // 创建主线程
    make_main_thread();
    // 创建idle线程
    idle_thread = thread_start("idle", idle, NULL);
    // 创建第一个用户进程init
    process_execute(init_th, "init");

    put_str("thread_init done\n");
}

