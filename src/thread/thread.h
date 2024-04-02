#ifndef __THREAD_THREAD_H
#define __THREAD_THREAD_H

#include "stdin.h"
#include "list.h"
#include "memory.h"

/* pcb栈顶的魔数 */
#define PCB_MAGIC 0x19870916 
/* 每个线程最多打开的文件数 */
#define MAX_FILES_OPEN_PER_PROC 8 

/* 自定义通用函数类型,它将在很多线程函数中做为形参类型 */
typedef void thread_func(void*);

/* 进程或线程的状态 */
enum task_status {
    TASK_RUNNING,
    TASK_READY,
    TASK_BLOCKED,
    TASK_WAITING,
    TASK_HANGING,
    TASK_DIED
};

/***********   中断栈intr_stack   ***********
* 中断栈用于中断发生时保护程序(线程或进程)的上下文环境:
* 进程或线程被中断打断时，会按照此结构压入上下文寄存器,
* intr_exit中的出栈操作是此结构的逆操作
* 此栈在线程自己的内核栈中位置固定,所在页的最顶端
********************************************/
struct intr_stack {
    uint32_t vec_no;	  // kernel.S 宏VECTOR中push %1压入的中断号
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp_dummy;	  // 虽然pushad把esp也压入,但esp是不断变化的,所以会被popad忽略
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t gs;
    uint32_t fs;
    uint32_t es;
    uint32_t ds;

    /* 以下由cpu从低特权级进入高特权级时压入,
     * 低特权级进入高特权级需要使用中断，所以这里有中断栈 */
    uint32_t err_code;
    void (*eip) (void);   // 无参数无返回值的函数指针
    uint32_t cs;
    uint32_t eflags;
    void* esp;
    uint32_t ss;
};

/***********  线程栈thread_stack  ***********
* 线程自己的栈,用于存储线程中待执行的函数
* 此结构在线程自己的内核栈中位置不固定,
* 用在switch_to时保存线程环境。
* 实际位置取决于实际运行情况。
******************************************/
struct thread_stack {
    uint32_t ebp;
    uint32_t ebx;
    uint32_t edi;
    uint32_t esi;

    /* 线程第一次执行时,eip指向待调用的函数kernel_thread
    其它时候,eip是指向switch_to的返回地址*/
    void (*eip) (thread_func* func, void* func_arg);

    /*****   以下仅供第一次被调度上cpu时使用   ****/
    void(*unused_retaddr);   // 为占位置充数为返回地址
    thread_func* function;   // 由Kernel_thread所调用的函数名
    void* func_arg;          // 由Kernel_thread所调用的函数所需的参数
};

/* 进程或线程的pcb,程序控制块 */
struct task_struct {
    uint32_t* self_kstack;	 // 线程或者进程内核栈的栈顶，就是pcb的高位
    enum task_status status; // 线程的运行状态
    char name[16];           // 线程名，最多16个字母
    uint32_t pid;            // 线程pid，也就是线程的标识号
    uint8_t priority;		 // 线程优先级
    uint8_t ticks;	         // 每次在处理器上的执行时间的滴答数
    uint32_t elapsed_ticks;  // 这个任务总的滴答数
    int32_t fd_table[MAX_FILES_OPEN_PER_PROC];    // 文件描述符数组,里面存放的是文件打开的描述符
    struct list_elem general_tag; // 线程在一段队列中的节点
    struct list_elem all_tag;// 线程在所有任务队列中的节点
    uint32_t* pgdir;         // 进程自己页表的虚拟地址
    struct virtual_addr userprog_vaddr;           // 用户进程虚拟地址池
    struct mem_block_desc u_block_desc[DESC_CNT]; // 用户进程内存块描述符
    uint32_t stack_magic;	 // 用这串数字做栈的边界标记,用于检测栈的溢出
};

/* 获取当前线程的pcb */
struct task_struct* running_thread(void);
/* 初始化线程栈thread_stack,将待执行的函数和参数放到thread_stack中相应的位置 */
void thread_create(struct task_struct* pthread, thread_func function, void* func_arg);
/* 初始化线程基本信息,name:线程名，prio:线程优先级 */
void init_thread(struct task_struct* pthread, char* name);
/* 创建一个名为name，优先级为prio，运行函数function，函数参数func_arg的线程 */
struct task_struct* thread_start(char* name, thread_func function, void* func_arg);
/* 切换任务 */
void schedule(void);
/* 当前线程将自己阻塞,标志其状态为stat. */
void thread_block(enum task_status stat);
/* 将线程pthread解除阻塞 */
void thread_unblock(struct task_struct* pthread);
/* 主动放弃CPU的使用 */
void thread_yield(void);
/* 初始化线程环境 */
void thread_init(void);
#endif
