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

/* 分配pid */
static uint32_t pid_allocate(void) {
    static uint32_t pid_flag = 0;    // pid变量,这里是静态的，可以一直加上去
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

/* 将线程pthread解除阻塞 */
void thread_unblock(struct task_struct* pthread) {
    if (pthread->status != TASK_READY) {
        pthread->priority = 4;      // 将当前线程的优先级置位4，使其优先得到调度
        pthread->status = TASK_READY;
        mlfq_push_wspt(pthread);
    } 
}

/* 主动放弃CPU的使用 */
void thread_yield(void) {
    enum intr_status old_status = intr_disable();
    struct task_struct* cur = running_thread();
    cur->status = TASK_READY;
    mlfq_push_wspt(cur);           // 不改变其优先级和时间片
    schedule();
    intr_set_status(old_status);
}

/* 系统空闲时运行的线程 */
static void idle(void* arg UNUSED) {
    while(1) {
        thread_block(TASK_BLOCKED); 
        //执行hlt时必须要保证目前处在开中断的情况下
        asm volatile ("sti; hlt" : : : "memory");
    }
}

/* 初始化线程环境 */
void thread_init(void) {
    put_str("thread_init start\n");
    lock_init(&pid_lock);            // pid锁初始化
    mlfq_init();                     // 多级队列初始化
    make_main_thread();              // 创建主线程
    idle_thread = thread_start("idle", idle, NULL); // 创建idle线程
    put_str("thread_init done\n");
}

