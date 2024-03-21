#include "thread.h"
#include "stdin.h"
#include "string.h"
#include "global.h"
#include "memory.h"
#include "print.h"
#include "interrupt.h"

struct task_struct* main_thread;    // 主线程PCB，也就是我们刚进内核的程序，现在运行的程序
struct list thread_ready_list;	    // 就绪队列
struct list thread_all_list;	    // 所有任务队列
static struct list_elem* thread_tag;// 用于保存队列中的线程结点

/* 线程转换从cur到next */
extern void switch_to(struct task_struct* cur, struct task_struct* next);

/* 获取当前线程pcb指针 */
struct task_struct* running_thread() {
    // 原理是拿到栈顶地址
    uint32_t esp;
    asm("mov %%esp, %0" : "=g" (esp));
    return (struct task_struct*)(esp & 0xfffff000);
}

/* 由kernel_thread去执行function(func_arg) */
static void kernel_thread(thread_func* function, void* func_arg) {
    // 要先开中断，否则后面的线程无法调度
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
void init_thread(struct task_struct* pthread, char* name, int prio) {
    memset(pthread, 0, sizeof(*pthread));
    strcpy(pthread->name, name);

    if (pthread == main_thread) {
        pthread->status = TASK_RUNNING;
    }
    else {
        pthread->status = TASK_READY;
    }

    pthread->priority = prio;
    pthread->self_kstack = (uint32_t*)((uint32_t)pthread + PG_SIZE);
    pthread->stack_magic = PCB_MAGIC;
}

/* 创建一个名为name，优先级为prio，运行函数function，函数参数func_arg的线程 */
struct task_struct* thread_start(char* name, int prio, thread_func function, void* func_arg) {
    struct task_struct* thread = get_kernel_pages(1);          // pcb程序控制块均位于内核空间，因为这部分内容是不允许用户知晓的
    init_thread(thread, name, prio);                           // 初始化线程的基本信息
    thread_create(thread, function, func_arg);                 // 创建线程

    // 确保这个线程之前不在就绪队列中
    if (elem_find(&thread_ready_list, &thread->general_tag)) {
        put_str("thread_start error: thread ");put_str(name);put_str("is in thread_ready_list\n");
        return NULL;
    }
    else {
        list_push(&thread_ready_list, &thread->general_tag);
    }

    // 确保这个线程之前不在所有线程中 
    if (elem_find(&thread_all_list, &thread->all_tag)) {
        put_str("thread_start error: thread ");put_str(name);put_str("is in thread_all_list\n");
        return NULL;
    }
    else {
        list_push(&thread_all_list, &thread->all_tag);
    }

    return thread;
}

/* 将kernel中的main函数完善为主线程 */
static void make_main_thread(void) {
    main_thread = running_thread();
    init_thread(main_thread, "main", 31);

    // main 函数是当前线程，当前线程不在就绪队列中，所以只是将其加入所有线程队列
    // 确保这个主线程之前不在所有线程中
    if (elem_find(&thread_all_list, &main_thread->all_tag)) {
        put_str("make_main_thread error: main_thread is in thread_all_list\n");
    }
    else {
        list_push(&thread_all_list, &main_thread->all_tag);
    }
}

/* 切换任务 */
void schedule(void) {
    // 关中断,确保下面的切换过程无人打扰
    enum intr_status schedule_intr = intr_disable();

    // 获得当前正在运行的程序的线程pcb
    struct task_struct* cur = running_thread();
    // 若此线程只是cpu时间片到了,将其加入到就绪队列尾
    if (cur->status == TASK_RUNNING) {
        // 将此线程加入就绪队列
        if (elem_find(&thread_ready_list, &cur->general_tag)) {
            put_str("schedule error: running_thread is in thread_ready_list\n");
            goto done;
        }
        else {
            list_append(&thread_ready_list, &cur->general_tag);
        }
        // 重置当前线程的ticks为priority
        cur->ticks = cur->priority;
        // 将当前线程标记为就绪态
        cur->status = TASK_READY;
    }
    else {
        /* 若此线程需要某事件发生后才能继续上cpu运行,
        不需要将其加入队列,因为当前线程不在就绪队列中。*/
    }

    // 就绪队列不能为空
    if (list_empty(&thread_ready_list)) {
        put_str("schedule error: thread_ready_list is empty\n");
        goto done;
    }
    // 从就绪队列中弹出一个任务
    thread_tag = list_pop(&thread_ready_list);
    // 获得弹出的这个任务的pcb
    struct task_struct* next = elem2entry(struct task_struct, general_tag, thread_tag);
    // 将就绪队列的任务的状态改为运行态
    next->status = TASK_RUNNING;
    // 切换两个任务
    switch_to(cur, next);

done:
    // 恢复到中断前的状态
    intr_set_status(schedule_intr);

}

/* 初始化线程环境 */
void thread_init(void) {
    put_str("thread_init start\n");
    list_init(&thread_ready_list);
    list_init(&thread_all_list);
    make_main_thread();              // 创建主线程
    put_str("thread_init done\n");
}





