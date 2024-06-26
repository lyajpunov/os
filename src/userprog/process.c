#include "process.h"
#include "global.h"
#include "assert.h"
#include "memory.h"
#include "thread.h"    
#include "list.h"    
#include "tss.h"    
#include "interrupt.h"
#include "string.h"
#include "console.h"
#include "mlfq.h"
#include "print.h"

extern void intr_exit(void);

/* 构建用户进程初始上下文信息 */
void start_process(void* filename_) {
    // 后面我们是要从硬盘中读取用户程序的，所以这里是一个filename,不是function
    void* function = filename_;
    struct task_struct* cur = running_thread();
    cur->self_kstack += sizeof(struct thread_stack);  //跨过thread_stack,指向intr_stack
    struct intr_stack* proc_stack = (struct intr_stack*)cur->self_kstack;//可以不用定义成结构体指针	 
    proc_stack->edi = 0;
    proc_stack->esi = 0;
    proc_stack->ebp = 0;
    proc_stack->esp_dummy = 0;
    proc_stack->ebx = 0;
    proc_stack->edx = 0;
    proc_stack->ecx = 0;
    proc_stack->eax = 0;
    proc_stack->gs = 0;		     // 不允许用户态直接访问硬件显存
    proc_stack->ds = SELECTOR_U_DATA;
    proc_stack->es = SELECTOR_U_DATA;
    proc_stack->fs = SELECTOR_U_DATA;
    proc_stack->eip = function;	 // 待执行的用户程序地址
    proc_stack->cs = SELECTOR_U_CODE;
    proc_stack->eflags = (EFLAGS_IOPL_0 | EFLAGS_MBS | EFLAGS_IF_1);
    proc_stack->esp = (void*)((uint32_t)get_a_page(PF_USER, USER_STACK3_VADDR) + PG_SIZE) ;
    proc_stack->ss = SELECTOR_U_DATA; 
    asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (proc_stack) : "memory");
}

/* 击活页表,可以是进程的pcb也可以是内核级线程的pcb，激活页表就是跟新cr3寄存器中的页目录表的物理地址 */
void page_dir_activate(struct task_struct* p_thread) {
    /* 若为内核线程,需要重新填充页表为0x100000 */
    uint32_t pagedir_phy_addr = 0x100000;  // 默认为内核的页目录物理地址,也就是内核线程所用的页目录表
    if (p_thread->pgdir != NULL)	{      // 用户态进程有自己的页目录表
        pagedir_phy_addr = addr_v2p((uint32_t)p_thread->pgdir);
    }
    /* 更新页目录寄存器cr3,使新页表生效 */
    asm volatile ("movl %0, %%cr3" : : "r" (pagedir_phy_addr) : "memory");
}

/* 激活线程或进程的页表,更新tss中的esp0为进程的特权级0的栈 */
void process_activate(struct task_struct* p_thread) {
    ASSERT(p_thread != NULL);
    // 激活该进程或线程的页表
    page_dir_activate(p_thread);
    /* 内核线程特权级本身就是0特权级,处理器进入中断时并不会从tss中获取0特权级栈地址,故不需要更新esp0 */
    if (p_thread->pgdir) {
        /* 更新该进程的esp0,用于此进程被中断时保留上下文 */
        update_tss_esp(p_thread);
    }
}

/* 创建页目录表,将当前页表的表示内核空间的pde复制,成功则返回页目录的虚拟地址,否则返回-1 */
uint32_t* create_page_dir(void) {
    // 用户进程的pcb以及页表都在内核空间，不能由用户访问
    uint32_t* page_dir_vaddr = get_kernel_pages(1);
    if (page_dir_vaddr == NULL) {
        console_put_str("create_page_dir error: get_kernel_page failed!");
        return NULL;
    }
    // 先复制页表,1024正好是后面的1GB
    memcpy((uint32_t*)((uint32_t)page_dir_vaddr + 0x300*4), (uint32_t*)(0xfffff000+0x300*4), 1024);
    // 更新页目录地址
    uint32_t new_page_dir_phy_addr = addr_v2p((uint32_t)page_dir_vaddr);
    /* 页目录地址是存入在页目录的最后一项,更新页目录地址为新页目录的物理地址 */
    page_dir_vaddr[1023] = new_page_dir_phy_addr | PG_US_U | PG_RW_W | PG_P_1;
    return page_dir_vaddr;
}

/* 创建用户进程虚拟地址位图 */
void create_user_vaddr_bitmap(struct task_struct* user_prog) {
    user_prog->userprog_vaddr.vaddr_start = USER_VADDR_START;
    uint32_t bitmap_pg_cnt = DIV_ROUND_UP((0xc0000000 - USER_VADDR_START) / PG_SIZE / 8 , PG_SIZE);
    user_prog->userprog_vaddr.vaddr_bitmap.bits = get_kernel_pages(bitmap_pg_cnt);
    user_prog->userprog_vaddr.vaddr_bitmap.btmp_bytes_len = (0xc0000000 - USER_VADDR_START) / PG_SIZE / 8;
    bitmap_init(&user_prog->userprog_vaddr.vaddr_bitmap);
}

/* 创建用户进程 */
void process_execute(void* filename, char* name) { 
    // pcb是操作系统的数据，由操作系统来维护
    struct task_struct* thread = get_kernel_pages(1);
    // 初始化线程
    init_thread(thread, name);
    // 完善pcb中的用户虚拟地址位图，这也是线程是内核线程还是用户进程的判别
    create_user_vaddr_bitmap(thread);
    // 创建线程
    thread_create(thread, start_process, filename);
    // 创建用户进程的页目录表，用户进程有自己的页目录表，这样就实现了进程的隔离
    thread->pgdir = create_page_dir();
    // 初始化用户进程的内存块描述符
    block_desc_init((struct mem_block_desc*) (&(thread->u_block_desc)));
    // 将当前线程加入多级反馈优先队列
    mlfq_new(thread);
}

