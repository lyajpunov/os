/*
 * @Author: lyajpunov 1961558693@qq.com
 * @Date: 2024-04-01 04:52:39
 * @LastEditors: lyajpunov 1961558693@qq.com
 * @LastEditTime: 2024-04-02 07:51:09
 * @FilePath: /os/src/fs/file.c
 * @Description: 文件的相关操作
 *
 * Copyright (c) 2024 by ${git_name_email}, All Rights Reserved.
 */

#include "file.h"
#include "fs.h"
#include "ide.h"
#include "dir.h"
#include "string.h"
#include "memory.h"
#include "stdio.h"
#include "list.h"
#include "assert.h"
#include "inode.h"
#include "interrupt.h"
#include "super_block.h"

 // 文件表
struct file file_table[MAX_FILE_OPEN];

/**
 * @description: 从文件表file_table中获取一个空闲位,成功返回下标,失败返回-1
 * @return {*}
 */
int32_t get_free_slot_in_global(void) {
    for (uint32_t fd_idx = 3; fd_idx < MAX_FILE_OPEN; fd_idx++) {
        if (file_table[fd_idx].fd_inode == NULL) return fd_idx;
    }
    printk("get_free_slot_in_global error: exceed max open files\n");
    return -1;
}

/**
 * @description: 将全局描述符下标安装到进程或线程自己的文件描述符数组fd_table中,成功返回下标，失败返回-1
 * @param {int32_t} globa_fd_idx 全局描述符下标
 * @return {*}
 */
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

/**
 * @description: 分配一个i结点,返回i结点号
 * @param {partition*} part 硬盘分区
 * @return {*}
 */
int32_t inode_bitmap_alloc(struct partition* part) {
    int32_t bit_idx = bitmap_scan(&part->inode_bitmap, 1);
    if (bit_idx == -1) {
        return -1;
    }
    bitmap_set(&part->inode_bitmap, bit_idx, 1);
    return bit_idx;
}

/**
 * @description: 分配1个扇区,返回其扇区地址
 * @param {partition*} part 分区
 * @return {*} 扇区地址
 */
int32_t block_bitmap_alloc(struct partition* part) {
    int32_t bit_idx = bitmap_scan(&part->block_bitmap, 1);
    if (bit_idx == -1) {
        return -1;
    }
    bitmap_set(&part->block_bitmap, bit_idx, 1);
    return (part->sb->data_start_lba + bit_idx);
}

/**
 * @description: 将内存中bitmap第bit_idx位所在的512字节同步到硬盘
 * @param {partition*} part 分区
 * @param {uint32_t} bit_idx 位图中低bit_idx位
 * @param {uint8_t} btmp_type 同步inode位图或者块位图
 * @return {*}
 */
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

/**
 * @description: 创建文件,若成功则返回文件描述符,否则返回-1
 * @param {dir*} parent_dir 这个文件的父目录
 * @param {char*} filename 文件名
 * @param {uint8_t} flag 创建文件时赋予文件的权限
 * @return {*}
 */
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

/**
 * @description: 打开编号为inode_no的inode对应的文件,若成功则返回文件描述符,否则返回-1
 * @param {uint32_t} inode_no inode号
 * @param {uint8_t} flag 打开
 * @return {*}
 */
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

/**
 * @description: 关闭文件，释放inode节点
 * @param {file*} file 文件
 * @return {*}
 */
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


/**
 * @description: 把buf中的count个字节写入file,成功则返回写入的字节数,失败则返回-1 
 * @param {file*} file 文件
 * @param {void*} buf  缓存
 * @param {uint32_t} count  写入的字节数
 * @return {*} 写入的字节数，失败返回-1
 */
int32_t file_write(struct file* file, const void* buf, uint32_t count) {
    // 文件目前最大只支持512*140=71680字节
    if ((file->fd_inode->i_size + count) > (BLOCK_SIZE * 140)) {
        printk("file_write: exceed max file_size 71680 bytes\n");
        return -1;
    }
    // 一个扇区的缓存
    uint8_t* io_buf = sys_malloc(BLOCK_SIZE);
    if (io_buf == NULL) {
        printk("file_write: sys_malloc for io_buf failed\n");
        return -1;
    }
    // 记录所有的块地址的缓存
    uint32_t* all_blocks = (uint32_t*)sys_malloc(BLOCK_SIZE + 48);
    if (all_blocks == NULL) {
        printk("file_write: sys_malloc for all_blocks failed\n");
        return -1;
    }

    const uint8_t* src = buf;	    // 用src指向buf中待写入的数据 
    uint32_t bytes_written = 0;	    // 用来记录已写入数据大小
    uint32_t size_left = count;	    // 用来记录未写入数据大小
    int32_t block_lba = -1;	        // 块地址
    uint32_t block_bitmap_idx = 0;  // 用来记录block对应于block_bitmap中的索引,做为参数传给bitmap_sync
    uint32_t sec_idx;	            // 用来索引扇区
    uint32_t sec_lba;	            // 扇区地址
    uint32_t sec_off_bytes;         // 扇区内字节偏移量
    uint32_t sec_left_bytes;        // 扇区内剩余字节量
    uint32_t chunk_size;	        // 每次写入硬盘的数据块大小
    int32_t indirect_block_table;   // 用来获取一级间接表地址
    uint32_t block_idx;		        // 块索引

    // 判断文件是否是第一次写,如果是,先为其分配一个块
    if (file->fd_inode->i_sectors[0] == 0) {
        block_lba = block_bitmap_alloc(cur_part);
        if (block_lba == -1) {
            printk("file_write: block_bitmap_alloc failed\n");
            return -1;
        }
        file->fd_inode->i_sectors[0] = block_lba;

        // 每分配一个块就将位图同步到硬盘
        block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
        bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
    }

    // 写入count个字节前,该文件已经占用的块数 
    uint32_t file_has_used_blocks = file->fd_inode->i_size / BLOCK_SIZE + 1;
    // 存储count字节后该文件将占用的块数
    uint32_t file_will_use_blocks = (file->fd_inode->i_size + count) / BLOCK_SIZE + 1;
    if (file_will_use_blocks > 140) {
        printk("file_write: buf is more than 140 blocks\n");
        return -1;
    }

    // 通过此增量判断是否需要分配扇区,如增量为0,表示原扇区够用
    uint32_t add_blocks = file_will_use_blocks - file_has_used_blocks;

    // 若不需要增加扇区
    if (add_blocks == 0) {
        // 文件数据量将在12块之内
        if (file_has_used_blocks <= 12) {
            block_idx = file_has_used_blocks - 1;
            all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
        }
        // 文件数据量将在12块之外
        else {
            indirect_block_table = file->fd_inode->i_sectors[12];
            ide_read(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);
        }
    }
    // 若需要增加扇区则分为三种情况
    else {
        // 12个直接块够用
        if (file_will_use_blocks <= 12) {
            // 先将有剩余空间的可继续用的扇区地址写入all_blocks
            block_idx = file_has_used_blocks - 1;
            all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];

            // 再将未来要用的扇区分配好后写入all_blocks
            block_idx = file_has_used_blocks;
            while (block_idx < file_will_use_blocks) {
                block_lba = block_bitmap_alloc(cur_part);
                if (block_lba == -1) {
                    printk("file_write: block_bitmap_alloc for situation 1 failed\n");
                    return -1;
                }

                // 写文件时,不应该存在块未使用但已经分配扇区的情况,当文件删除时,就会把块地址清0
                ASSERT(file->fd_inode->i_sectors[block_idx] == 0); 
                file->fd_inode->i_sectors[block_idx] = all_blocks[block_idx] = block_lba;

                // 每分配一个块就将位图同步到硬盘
                block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
                bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
                // 下一个分配的新扇区
                block_idx++;   
            }
        }
        // 第二种情况: 旧数据在12个直接块内,新数据将使用间接块
        else if (file_has_used_blocks <= 12 && file_will_use_blocks > 12) {
            // 先将有剩余空间的可继续用的扇区地址收集到all_blocks 
            block_idx = file_has_used_blocks - 1;
            all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];

            // 创建一级间接块表
            block_lba = block_bitmap_alloc(cur_part);
            if (block_lba == -1) {
                printk("file_write: block_bitmap_alloc for situation 2 failed\n");
                return -1;
            }
            // 确保一级间接块表未分配
            ASSERT(file->fd_inode->i_sectors[12] == 0);  
            // 分配一级间接块索引表
            indirect_block_table = file->fd_inode->i_sectors[12] = block_lba;
            // 第一个未使用的块,即本文件最后一个已经使用的直接块的下一块
            block_idx = file_has_used_blocks;	
            while (block_idx < file_will_use_blocks) {
                block_lba = block_bitmap_alloc(cur_part);
                if (block_lba == -1) {
                    printk("file_write: block_bitmap_alloc for situation 2 failed\n");
                    return -1;
                }
                // 新创建的0~11块直接存入all_blocks数组
                if (block_idx < 12) {
                    // 确保尚未分配扇区地址
                    ASSERT(file->fd_inode->i_sectors[block_idx] == 0);
                    file->fd_inode->i_sectors[block_idx] = all_blocks[block_idx] = block_lba;
                }
                // 间接块只写入到all_block数组中,待全部分配完成后一次性同步到硬盘
                else {
                    all_blocks[block_idx] = block_lba;
                }

                // 每分配一个块就将位图同步到硬盘
                block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
                bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
                // 下一个新扇区
                block_idx++;   
            }
            // 同步一级间接块表到硬盘
            ide_write(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);
        }
        // 第三种情况:新数据占据间接块
        else if (file_has_used_blocks > 12) {
            // 已经具备了一级间接块表
            ASSERT(file->fd_inode->i_sectors[12] != 0);
            // 获取一级间接表地址
            indirect_block_table = file->fd_inode->i_sectors[12];

            // 已使用的间接块也将被读入all_blocks,无须单独收录
            ide_read(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);
            // 第一个未使用的间接块,即已经使用的间接块的下一块
            block_idx = file_has_used_blocks;	  
            while (block_idx < file_will_use_blocks) {
                block_lba = block_bitmap_alloc(cur_part);
                if (block_lba == -1) {
                    printk("file_write: block_bitmap_alloc for situation 3 failed\n");
                    return -1;
                }
                all_blocks[block_idx++] = block_lba;

                // 每分配一个块就将位图同步到硬盘
                block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
                bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
            }
            // 同步一级间接块表到硬盘
            ide_write(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);
        }
    }

    // 含有剩余空间的扇区标识
    bool first_write_block = true;
    // 块地址已经收集到all_blocks中,下面开始写数据 
    file->fd_pos = file->fd_inode->i_size - 1;
    // 直到写完所有数据
    while (bytes_written < count) {
        memset(io_buf, 0, BLOCK_SIZE);
        sec_idx = file->fd_inode->i_size / BLOCK_SIZE;
        sec_lba = all_blocks[sec_idx];
        sec_off_bytes = file->fd_inode->i_size % BLOCK_SIZE;
        sec_left_bytes = BLOCK_SIZE - sec_off_bytes;

        // 判断此次写入硬盘的数据大小
        chunk_size = size_left < sec_left_bytes ? size_left : sec_left_bytes;
        if (first_write_block) {
            ide_read(cur_part->my_disk, sec_lba, io_buf, 1);
            first_write_block = false;
        }
        memcpy(io_buf + sec_off_bytes, src, chunk_size);
        ide_write(cur_part->my_disk, sec_lba, io_buf, 1);
        //调试,完成后去掉
        printk("file write at lba 0x%x\n", sec_lba);
        // 将指针推移到下个新数据
        src += chunk_size;
        // 更新文件大小
        file->fd_inode->i_size += chunk_size;
        // 更新文件指针
        file->fd_pos += chunk_size;
        // 更新已写入数据
        bytes_written += chunk_size;
        // 更新未写入数据
        size_left -= chunk_size;
    }
    // 更新文件的inode信息
    inode_sync(cur_part, file->fd_inode);
    // 释放内存
    sys_free(all_blocks);
    sys_free(io_buf);
    return bytes_written;
}
