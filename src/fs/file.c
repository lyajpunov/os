#include "file.h"
#include "fs.h"
#include "ide.h"
#include "dir.h"
#include "string.h"
#include "memory.h"
#include "stdio.h"
#include "list.h"
#include "inode.h"
#include "interrupt.h"
#include "super_block.h"


// 文件表
struct file file_table[MAX_FILE_OPEN];

/* 从文件表file_table中获取一个空闲位,成功返回下标,失败返回-1 */
int32_t get_free_slot_in_global(void) {
    for (uint32_t fd_idx = 3; fd_idx < MAX_FILE_OPEN; fd_idx++) {
        if (file_table[fd_idx].fd_inode == NULL) return fd_idx;
    }
    printk("get_free_slot_in_global error: exceed max open files\n");
    return -1;
}

/* 将全局描述符下标安装到进程或线程自己的文件描述符数组fd_table中,成功返回下标，失败返回-1 */
int32_t pcb_fd_install(int32_t globa_fd_idx) {
    struct task_struct* cur = running_thread();
    for (uint8_t local_fd_idx = 3; local_fd_idx < MAX_FILES_OPEN_PER_PROC; local_fd_idx++) {
        if (cur->fd_table[local_fd_idx] == -1) {
            cur->fd_table[local_fd_idx] = globa_fd_idx;
            return local_fd_idx;
        }
    }
    printk("pcb_fd_install error: exceed max open files_per_proc\n");
    return -1;
}

/* 分配一个i结点,返回i结点号 */
int32_t inode_bitmap_alloc(struct partition* part) {
    int32_t bit_idx = bitmap_scan(&part->inode_bitmap, 1);
    if (bit_idx == -1) {
        return -1;
    }
    bitmap_set(&part->inode_bitmap, bit_idx, 1);
    return bit_idx;
}

/* 分配1个扇区,返回其扇区地址,这里返回的不是idx，直接就是扇区地址*/
int32_t block_bitmap_alloc(struct partition* part) {
    int32_t bit_idx = bitmap_scan(&part->block_bitmap, 1);
    if (bit_idx == -1) {
        return -1;
    }
    bitmap_set(&part->block_bitmap, bit_idx, 1);
    return (part->sb->data_start_lba + bit_idx);
}

/* 将内存中bitmap第bit_idx位所在的512字节同步到硬盘 */
void bitmap_sync(struct partition* part, uint32_t bit_idx, uint8_t btmp_type) {
    // 本i结点索引相对于位图的扇区偏移量
    uint32_t off_sec = bit_idx / 4096;
    // 本i结点索引相对于位图的字节偏移量
    uint32_t off_size = off_sec * BLOCK_SIZE;
    uint32_t sec_lba;
    uint8_t* bitmap_off;

    // 需要被同步到硬盘的位图只有inode_bitmap和block_bitmap
    switch (btmp_type) {
    case INODE_BITMAP:
        sec_lba = part->sb->inode_bitmap_lba + off_sec;
        bitmap_off = part->inode_bitmap.bits + off_size;
        break;

    case BLOCK_BITMAP:
        sec_lba = part->sb->block_bitmap_lba + off_sec;
        bitmap_off = part->block_bitmap.bits + off_size;
        break;
    }
    ide_write(part->my_disk, sec_lba, bitmap_off, 1);
}



/* 创建文件,若成功则返回文件描述符,否则返回-1 */
int32_t file_create(struct dir* parent_dir, char* filename, uint8_t flag) {
    // 后续操作的公共缓冲区
    void* io_buf = sys_malloc(1024);
    // 申请内存失败
    if (io_buf == NULL) {
        printk("in file_creat: sys_malloc for io_buf failed\n");
        return -1;
    }

    // 用于操作失败时回滚的标志位
    uint8_t rollback_step = 0;

    // 为新文件分配inode,第一步从inode位图中找到空闲的位
    int32_t inode_no = inode_bitmap_alloc(cur_part);
    if (inode_no == -1) {
        printk("file_creat: allocate inode failed\n");
        return -1;
    }

    // 生成新的inode节点，在堆中，即使函数结束也不会被释放
    struct inode* new_file_inode = (struct inode*)sys_malloc(sizeof(struct inode));
    // 失败则回滚
    if (new_file_inode == NULL) {
        printk("file_create: sys_malloc for inode failded\n");
        rollback_step = 1;
        goto rollback;
    }

    // 初始化inode结点,这个只是操作已有的内存空间，不大会失败
    inode_init(inode_no, new_file_inode);

    // 返回的是file_table数组的下标
    int fd_idx = get_free_slot_in_global();
    // 失败则回滚
    if (fd_idx == -1) {
        printk("file_create: exceed max open files\n");
        rollback_step = 2;
        goto rollback;
    }

    // 全局文件表中将inode节点指向新生成的inode，剩下的都初始化
    file_table[fd_idx].fd_inode = new_file_inode;
    file_table[fd_idx].fd_pos = 0;
    file_table[fd_idx].fd_flag = flag;
    file_table[fd_idx].fd_inode->write_deny = false;

    // 生成目录项，并初始化
    struct dir_entry new_dir_entry;
    memset(&new_dir_entry, 0, sizeof(struct dir_entry));

    // create_dir_entry只是内存操作不出意外,不会返回失败
    create_dir_entry(filename, inode_no, FT_REGULAR, &new_dir_entry);

    /*******************************************************************************
     * 下面是同步到硬盘的操作
     *******************************************************************************/
     // 1、在目录parent_dir下安装目录项new_dir_entry, 写入硬盘后返回true,否则false
    if (!sync_dir_entry(parent_dir, &new_dir_entry, io_buf)) {
        printk("file_create: sync dir_entry to disk failed\n");
        rollback_step = 3;
        goto rollback;
    }

    // 2、将父目录i结点的内容同步到硬盘
    inode_sync(cur_part, parent_dir->inode);

    // 3、将新创建文件的i结点内容同步到硬盘
    inode_sync(cur_part, new_file_inode);

    // 4、将inode_bitmap位图同步到硬盘
    bitmap_sync(cur_part, inode_no, INODE_BITMAP);

    // 5、将创建的文件i结点添加到open_inodes链表
    list_push(&cur_part->open_inodes, &new_file_inode->inode_tag);

    // 6、将打开inode节点数变为1
    new_file_inode->i_open_cnts = 1;

    // 7、释放缓冲区
    sys_free(io_buf);

    // 8、将描述符安装到当前线程的描述符表中
    return pcb_fd_install(fd_idx);

    // 失败会跳转到这里回滚
rollback:
    switch (rollback_step) {
    case 3:
        // 失败时,将file_table中的相应位清空
        memset(&file_table[fd_idx], 0, sizeof(struct file));
        // fall through
    case 2:
        // 释放生成的inode节点
        sys_free(new_file_inode);
        // fall through
    case 1:
        // 如果新文件的i结点创建失败,之前位图中分配的inode_no也要恢复
        bitmap_set(&cur_part->inode_bitmap, inode_no, 0);
        // fall through
    default:
        break;
    }
    sys_free(io_buf);
    return -1;
}


/* 打开编号为inode_no的inode对应的文件,若成功则返回文件描述符,否则返回-1 */
int32_t file_open(uint32_t inode_no, uint8_t flag) {
    int fd_idx = get_free_slot_in_global();
    if (fd_idx == -1) {
        printk("file_open error: exceed max open files\n");
        return -1;
    }
    // 打开inode节点
    file_table[fd_idx].fd_inode = inode_open(cur_part, inode_no);
    // 每次打开文件,要将fd_pos还原为0,即让文件内的指针指向开头
    file_table[fd_idx].fd_pos = 0;
    // 将文件权限置为flag
    file_table[fd_idx].fd_flag = flag;
    // 获得写文件标志位的指正
    bool* write_deny = &file_table[fd_idx].fd_inode->write_deny;

    // 只要是关于写文件,判断是否有其它进程正写此文件
    if (flag & O_WRONLY || flag & O_RDWR) {
        enum intr_status old_status = intr_disable();
        // 若当前没有其它进程写该文件,将其占用.
        if (!(*write_deny)) {
            // 置为true,避免多个进程同时写此文件
            *write_deny = true;
            // 恢复中断
            intr_set_status(old_status);
        }
        else {
            // 失败先恢复中断，再返回
            intr_set_status(old_status);
            printk("file_open error: file can`t be write now, try again later\n");
            return -1;
        }
    }

    // 若是读文件或创建文件,不用理会write_deny,保持默认
    return pcb_fd_install(fd_idx);
}

/* 关闭文件 */
int32_t file_close(struct file* file) {
    if (file == NULL) return -1;
    // 将写的位置为false
    file->fd_inode->write_deny = false;
    // 关闭inode节点
    inode_close(file->fd_inode);
    // 使文件结构可用
    file->fd_inode = NULL;
    return 0;
}
