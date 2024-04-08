#include "fork.h"
#include "process.h"
#include "memory.h"
#include "interrupt.h"
#include "assert.h"
#include "thread.h"    
#include "string.h"
#include "file.h"
#include "mlfq.h"

extern void intr_exit(void);

/**
 * @description: 将父进程的pcb、虚拟地址位图拷贝给子进程
 * @param {task_struct*} child_thread 子进程
 * @param {task_struct*} parent_thread 父进程
 * @return {*}
 */
static int32_t copy_pcb_vaddrbitmap_stack0(struct task_struct* child_thread, struct task_struct* parent_thread) {
    // a 复制pcb所在的整个页,里面包含进程pcb信息及特级0极的栈,里面包含了返回地址, 然后再单独修改个别部分
    memcpy(child_thread, parent_thread, PG_SIZE);
    child_thread->pid = pid_allocate();
    child_thread->elapsed_ticks = 0;
    child_thread->status = TASK_READY;
    child_thread->parent_pid = parent_thread->pid;
    child_thread->general_tag.prev = child_thread->general_tag.next = NULL;
    child_thread->all_tag.prev = child_thread->all_tag.next = NULL;
    block_desc_init(child_thread->u_block_desc);
    // b 复制父进程的虚拟地址池的位图
    uint32_t bitmap_pg_cnt = DIV_ROUND_UP((0xc0000000 - USER_VADDR_START) / PG_SIZE / 8, PG_SIZE);
    // c 获得一个内核地址页面
    void* vaddr_btmp = get_kernel_pages(bitmap_pg_cnt);
    if (vaddr_btmp == NULL) return -1;
    /* 此时child_thread->userprog_vaddr.vaddr_bitmap.bits还是指向父进程虚拟地址的位图地址
     * 下面将child_thread->userprog_vaddr.vaddr_bitmap.bits指向自己的位图vaddr_btmp */
    memcpy(vaddr_btmp, child_thread->userprog_vaddr.vaddr_bitmap.bits, bitmap_pg_cnt * PG_SIZE);
    child_thread->userprog_vaddr.vaddr_bitmap.bits = vaddr_btmp;
    // pcb.name的长度是16,为避免下面strcat越界
    ASSERT(strlen(child_thread->name) < 11);
    strcat(child_thread->name, "_fork");
    return 0;
}

/**
 * @description: 复制子进程的进程体(代码和数据)及用户栈
 * @param {task_struct*} child_thread 子进程
 * @param {task_struct*} parent_thread 父进程
 * @param {void*} buf_page 
 * @return {*}
 */
static void copy_body_stack3(struct task_struct* child_thread, struct task_struct* parent_thread, void* buf_page) {
    uint8_t* vaddr_btmp = parent_thread->userprog_vaddr.vaddr_bitmap.bits;
    uint32_t btmp_bytes_len = parent_thread->userprog_vaddr.vaddr_bitmap.btmp_bytes_len;
    uint32_t vaddr_start = parent_thread->userprog_vaddr.vaddr_start;
    
    /* 在父进程的用户空间中查找已有数据的页 */
    for (uint32_t idx_byte = 0; idx_byte < btmp_bytes_len; idx_byte++) {
        if (vaddr_btmp[idx_byte]) {
            for (uint32_t idx_bit = 0; idx_bit < 8; idx_bit++) {
                if ((BITMAP_MASK << idx_bit) & vaddr_btmp[idx_byte]) {
                    uint32_t prog_vaddr = (idx_byte * 8 + idx_bit) * PG_SIZE + vaddr_start;
                    // 下面的操作是将父进程用户空间中的数据通过内核空间做中转,最终复制到子进程的用户空间
                    // a 将父进程在用户空间中的数据复制到内核缓冲区buf_page,目的是下面切换到子进程的页表后,还能访问到父进程的数据
                    memcpy(buf_page, (void*)prog_vaddr, PG_SIZE);
                    // b 将页表切换到子进程,目的是避免下面申请内存的函数将pte及pde安装在父进程的页表中
                    page_dir_activate(child_thread);
                    // c 申请虚拟地址prog_vaddr
                    get_a_page_without_opvaddrbitmap(PF_USER, prog_vaddr);
                    // d 从内核缓冲区中将父进程数据复制到子进程的用户空间
                    memcpy((void*)prog_vaddr, buf_page, PG_SIZE);
                    // e 恢复父进程页表
                    page_dir_activate(parent_thread);
                }
            }
        }
    }
}

/**
 * @description: 为子进程构建thread_stack和修改返回值
 * @param {task_struct*} child_thread
 * @return {*}
 */
static int32_t build_child_stack(struct task_struct* child_thread) {
    // a 使子进程pid返回值为0
    struct intr_stack* intr_0_stack = (struct intr_stack*)((uint32_t)child_thread + PG_SIZE - sizeof(struct intr_stack));
    intr_0_stack->eax = 0;

    // b 为switch_to 构建 struct thread_stack,将其构建在紧临intr_stack之下的空间
    uint32_t* ret_addr_in_thread_stack = (uint32_t*)intr_0_stack - 1;
    uint32_t* esi_ptr_in_thread_stack = (uint32_t*)intr_0_stack - 2;
    uint32_t* edi_ptr_in_thread_stack = (uint32_t*)intr_0_stack - 3;
    uint32_t* ebx_ptr_in_thread_stack = (uint32_t*)intr_0_stack - 4;
    uint32_t* ebp_ptr_in_thread_stack = (uint32_t*)intr_0_stack - 5;

    // c switch_to的返回地址更新为intr_exit,直接从中断返回
    *ret_addr_in_thread_stack = (uint32_t)intr_exit;

    // d 这几个值会在出栈时被覆盖
    *ebp_ptr_in_thread_stack = 0;
    *ebx_ptr_in_thread_stack = 0;
    *edi_ptr_in_thread_stack = 0;
    *esi_ptr_in_thread_stack = 0;

    // e 把构建的thread_stack的栈顶做为switch_to恢复数据时的栈顶
    child_thread->self_kstack = ebp_ptr_in_thread_stack;

    return 0;
}

/**
 * @description: 更新inode打开数
 * @param {task_struct*} thread
 * @return {*}
 */
static void update_inode_open_cnts(struct task_struct* thread) {
    int32_t local_fd = 3, global_fd = 0;
    while (local_fd < MAX_FILES_OPEN_PER_PROC) {
        global_fd = thread->fd_table[local_fd];
        ASSERT(global_fd < MAX_FILE_OPEN);
        if (global_fd != -1) {
            file_table[global_fd].fd_inode->i_open_cnts++;
        }
        local_fd++;
    }
}

/**
 * @description: 拷贝父进程本身所占资源给子进程
 * @param {task_struct*} child_thread
 * @param {task_struct*} parent_thread
 * @return {*}
 */
static int32_t copy_process(struct task_struct* child_thread, struct task_struct* parent_thread) {
    // 内核缓冲区,作为父进程用户空间的数据复制到子进程用户空间的中转
    void* buf_page = get_kernel_pages(1);
    if (buf_page == NULL) return -1;

    // a 复制父进程的pcb、虚拟地址位图、内核栈到子进程
    if (copy_pcb_vaddrbitmap_stack0(child_thread, parent_thread) == -1) return -1;

    // b 为子进程创建页表,此页表仅包括内核空间
    child_thread->pgdir = create_page_dir();
    if (child_thread->pgdir == NULL) return -1;

    // c 复制父进程进程体及用户栈给子进程
    copy_body_stack3(child_thread, parent_thread, buf_page);

    // d 构建子进程thread_stack和修改返回值pid
    build_child_stack(child_thread);

    // e 更新文件inode的打开数
    update_inode_open_cnts(child_thread);

    // 释放内核缓冲区
    mfree_page(PF_KERNEL, buf_page, 1);
    return 0;
}

/**
 * @description: fork子进程,内核线程不可直接调用
 * @return {*}
 */
uint32_t sys_fork(void) {
    // 获取父进程pcb
    struct task_struct* parent_thread = running_thread();
    // 为子进程创建pcb(task_struct结构)
    struct task_struct* child_thread = get_kernel_pages(1);
    // 确保关中断以及当前调用的线程不是内核线程
    ASSERT(INTR_OFF == intr_get_status() && parent_thread->pgdir != NULL);
    // 拷贝父进程本身所占资源给子进程
    if (copy_process(child_thread, parent_thread) == -1) return -1;
    // 添加到就绪线程队列和所有线程队列,子进程由调试器安排运行
    mlfq_push_wspt(child_thread);
    // 父进程返回子进程的pid
    return child_thread->pid;    
}

