/*
 * @Author: lyajpunov 1961558693@qq.com
 * @Date: 2024-04-01 00:22:21
 * @LastEditors: lyajpunov 1961558693@qq.com
 * @LastEditTime: 2024-04-07 06:50:57
 * @FilePath: /os/src/fs/dir.c
 * @Description: 目录的基本操作
 *
 * Copyright (c) 2024 by ${git_name_email}, All Rights Reserved.
 */
#include "dir.h"
#include "ide.h"
#include "file.h"
#include "string.h"
#include "stdio.h"
#include "assert.h"
#include "super_block.h"

 // 根目录
struct dir root_dir;

/**
 * @description: 打开根目录
 * @param {partition*} part 打开根目录的分区
 * @return {*}
 */
void open_root_dir(struct partition* part) {
    root_dir.inode = inode_open(part, part->sb->root_inode_no);
    root_dir.dir_pos = 0;
}

/**
 * @description: 在分区part上打开i结点为inode_no的目录并返回目录指针
 * @param {partition*} part 分区
 * @param {uint32_t} inode_no 节点
 * @return {*}
 */
struct dir* dir_open(struct partition* part, uint32_t inode_no) {
    struct dir* pdir = (struct dir*)sys_malloc(sizeof(struct dir));
    pdir->inode = inode_open(part, inode_no);
    pdir->dir_pos = 0;
    return pdir;
}

/**
 * @description: 在part分区内的pdir目录内寻找名为name的文件或目录,找到后返回true并将其目录项存入dir_e,否则返回false
 * @param {partition*} part 分区
 * @param {dir*} pdir 从此目录查找
 * @param {char*} name 文件或者目录
 * @param {dir_entry*} dir_e 找到目录项存入dir_e
 * @return {*}
 */
bool search_dir_entry(struct partition* part, struct dir* pdir, const char* name, struct dir_entry* dir_e) {
    // 12个直接块大小+128个间接块,共560字节
    uint32_t* all_blocks = (uint32_t*)sys_malloc(560);
    // 申请内存
    if (all_blocks == NULL) {
        printk("search_dir_entry: sys_malloc for all_blocks failed");
        return false;
    }

    // 填充12个一级指针
    for (uint32_t block_idx = 0; block_idx < 12; block_idx++) {
        all_blocks[block_idx] = pdir->inode->i_sectors[block_idx];
    }
    // 再处理二级页表
    if (pdir->inode->i_sectors[12] != 0) {
        ide_read(part->my_disk, pdir->inode->i_sectors[12], all_blocks + 12, 1);
    }

    // 写目录项的时候保证在一个扇区，读的时候就可以从一个扇区读，这样浪费了一点空间，但是更方便了
    uint8_t* buf = (uint8_t*)sys_malloc(SECTOR_SIZE);
    // 指向目录项的指针,值为buf起始地址
    struct dir_entry* p_de = (struct dir_entry*)buf;
    // 目录项的大小
    uint32_t dir_entry_size = part->sb->dir_entry_size;
    // 1扇区内可容纳的目录项个数
    uint32_t dir_entry_cnt = SECTOR_SIZE / dir_entry_size;

    // 开始在所有块中查找目录项
    for (uint32_t block_idx = 0; block_idx < 140; block_idx++) {
        // 块地址为0时表示该块中无数据,继续在其它块中找
        if (all_blocks[block_idx] == 0) {
            block_idx++;
            continue;
        }
        // 读取有目录的块
        ide_read(part->my_disk, all_blocks[block_idx], buf, 1);
        // 遍历扇区中所有目录项
        for (uint32_t dir_entry_idx = 0; dir_entry_idx < dir_entry_cnt; dir_entry_idx++) {
            // 若找到了,就直接复制整个目录项
            if (!strcmp(p_de->filename, name)) {
                memcpy(dir_e, p_de, dir_entry_size);
                sys_free(buf);
                sys_free(all_blocks);
                return true;
            }
            p_de++;
        }
        // 此时p_de已经指向扇区内最后一个完整目录项了,需要恢复p_de指向为buf
        p_de = (struct dir_entry*)buf;
        // 将buf清0,下次再用
        memset(buf, 0, SECTOR_SIZE);
    }
    sys_free(buf);
    sys_free(all_blocks);
    return false;
}

/**
 * @description: 关闭目录
 * @param {dir*} dir 要关闭的目录
 * @return {*}
 */
void dir_close(struct dir* dir) {
    // 如果是根目录是不能关闭的，根目录打开的空间直接在内核。不在堆中，不能free
    if (dir == &root_dir) return;
    inode_close(dir->inode);
    sys_free(dir);
}

/**
 * @description: 在内存中初始化目录项p_de
 * @param {char*} filename 文件名
 * @param {uint32_t} inode_no inode编号
 * @param {uint8_t} file_type 文件类型
 * @param {dir_entry*} p_de 目录项
 * @return {*}
 */
void create_dir_entry(char* filename, uint32_t inode_no, uint8_t file_type, struct dir_entry* p_de) {
    memcpy(p_de->filename, filename, strlen(filename));
    p_de->i_no = inode_no;
    p_de->f_type = file_type;
}

/**
 * @description: 将目录项p_de写入父目录parent_dir中,io_buf由主调函数提供
 * @param {dir*} parent_dir 父目录
 * @param {dir_entry*} p_de 要写入的目录项
 * @param {void*} io_buf    缓存，由主调函数提供
 * @return {*}
 */
bool sync_dir_entry(struct dir* parent_dir, struct dir_entry* p_de, void* io_buf) {
    // 根目录的inode
    struct inode* dir_inode = parent_dir->inode;
    // 根目录的文件大小
    uint32_t dir_size = dir_inode->i_size;
    // 目录项的大小，现在是
    uint32_t dir_entry_size = cur_part->sb->dir_entry_size;
    // 保证根目录的文件大小是目录项的整数倍
    ASSERT(dir_size % dir_entry_size == 0);
    // 每个扇区最大的目录项树木
    uint32_t dir_entrys_per_sec = (512 / dir_entry_size);
    int32_t block_lba = -1;

    // 将该目录的所有扇区地址存入all_blocks
    uint32_t all_blocks[140];
    for (uint8_t block_idx = 0; block_idx < 12; block_idx++) {
        all_blocks[block_idx] = dir_inode->i_sectors[block_idx];
    }

    // 创建指向iobuf的指针用来遍历
    struct dir_entry* dir_e = (struct dir_entry*)io_buf;
    int32_t block_bitmap_idx = -1;

    // 开始遍历所有块以寻找目录项空位,若已有扇区中没有空闲位,在不超过文件大小的情况下申请新扇区来存储新目录项
    for (uint8_t block_idx = 0; block_idx < 140; block_idx++) {
        block_bitmap_idx = -1;
        if (all_blocks[block_idx] == 0) {
            // 找到了一个没有地址的指针
            block_lba = block_bitmap_alloc(cur_part);
            if (block_lba == -1) {
                printk("sync_dir_entry error: alloc block bitmap failed\n");
                return false;
            }

            // 每分配一个块就同步一次block_bitmap
            block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
            bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);


            block_bitmap_idx = -1;
            // 若是直接块，那么将这个地址赋值给直接块指正
            if (block_idx < 12) {
                dir_inode->i_sectors[block_idx] = all_blocks[block_idx] = block_lba;
            }
            // 若是尚未分配一级间接块表(block_idx等于12表示第0个间接块地址为0)
            else if (block_idx == 12) {
                // 将上面分配的块做为一级间接块表地址
                dir_inode->i_sectors[12] = block_lba;
                block_lba = -1;
                // 再分配一个块做为第0个间接块，也就是第13个块
                block_lba = block_bitmap_alloc(cur_part);
                // 如果分配失败
                if (block_lba == -1) {
                    // 找到第一个分配的块
                    block_bitmap_idx = dir_inode->i_sectors[12] - cur_part->sb->data_start_lba;
                    // 将其恢复
                    bitmap_set(&cur_part->block_bitmap, block_bitmap_idx, 0);
                    // 将一节间接表指正恢复
                    dir_inode->i_sectors[12] = 0;
                    // 打印报错信息
                    printk("sync_dir_entry error: alloc block bitmap failed\n");
                    return false;
                }

                // 每分配一个块就同步一次block_bitmap
                block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
                bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
                // 将指针指向新分配的块
                all_blocks[12] = block_lba;
                // 把新分配的第0个间接块地址写入一级间接块表
                ide_write(cur_part->my_disk, dir_inode->i_sectors[12], all_blocks + 12, 1);
            }
            // 若是间接块未分配
            else {
                all_blocks[block_idx] = block_lba;
                // 把新分配的第(block_idx-12)个间接块地址写入一级间接块表
                ide_write(cur_part->my_disk, dir_inode->i_sectors[12], all_blocks + 12, 1);
            }

            // 再将新目录项p_de写入新分配的间接块
            memset(io_buf, 0, 512);
            memcpy(io_buf, p_de, dir_entry_size);
            ide_write(cur_part->my_disk, all_blocks[block_idx], io_buf, 1);
            dir_inode->i_size += dir_entry_size;
            return true;
        }

        // 若第block_idx块已存在,将其读进内存,然后在该块中查找空目录项
        ide_read(cur_part->my_disk, all_blocks[block_idx], io_buf, 1);
        // 在扇区内查找空目录项 
        for (uint8_t dir_entry_idx = 0; dir_entry_idx < dir_entrys_per_sec; dir_entry_idx++) {
            // FT_UNKNOWN为0,无论是初始化或是删除文件后,都会将f_type置为FT_UNKNOWN.
            if ((dir_e + dir_entry_idx)->f_type == FT_UNKNOWN) {
                memcpy(dir_e + dir_entry_idx, p_de, dir_entry_size);
                ide_write(cur_part->my_disk, all_blocks[block_idx], io_buf, 1);
                dir_inode->i_size += dir_entry_size;
                return true;
            }
        }
    }
    printk("directory is full!\n");
    return false;
}

/**
 * @description: 把分区part目录pdir中编号为inode_no的目录项删除
 * @param {partition*} part 分区
 * @param {dir*} pdir 路径
 * @param {uint32_t} inode_no inode编号
 * @param {void*} io_buf io缓存
 * @return {*}
 */
bool delete_dir_entry(struct partition* part, struct dir* pdir, uint32_t inode_no, void* io_buf) {
    // 目录所在块的结构
    struct inode* dir_inode = pdir->inode;
    // 准备数据
    uint32_t block_idx = 0;
    uint32_t all_blocks[140] = { 0 };
    // 收集目录全部块地址
    while (block_idx < 12) {
        all_blocks[block_idx] = dir_inode->i_sectors[block_idx];
        block_idx++;
    }
    if (dir_inode->i_sectors[12]) {
        ide_read(part->my_disk, dir_inode->i_sectors[12], all_blocks + 12, 1);
    }

    // 目录项大小
    uint32_t dir_entry_size = part->sb->dir_entry_size;
    // 每个扇区可以存储的目录项数目
    uint32_t dir_entrys_per_sec = (SECTOR_SIZE / dir_entry_size);
    // 目录项
    struct dir_entry* dir_e = (struct dir_entry*)io_buf;
    // 找到的inode_no目录项
    struct dir_entry* dir_entry_found = NULL;
    // 是否是目录的第1个块 
    bool is_dir_first_block = false;

    // 1、遍历所有块,寻找目录项
    for (block_idx = 0; block_idx < 140; block_idx++) {
        is_dir_first_block = false;
        if (all_blocks[block_idx] == 0) continue;

        // 初始化
        uint8_t dir_entry_cnt = 0;
        memset(io_buf, 0, SECTOR_SIZE);

        // 1.1、读取扇区,获得目录项
        ide_read(part->my_disk, all_blocks[block_idx], io_buf, 1);

        // 1.2、遍历所有的目录项,统计该扇区的目录项数量及是否有待删除的目录项
        for (uint8_t dir_entry_idx = 0; dir_entry_idx < dir_entrys_per_sec; dir_entry_idx++) {
            if ((dir_e + dir_entry_idx)->f_type != FT_UNKNOWN) {
                // 是否是目录的第一个块，检测是否是‘.‘或者’..’目录
                if (!strcmp((dir_e + dir_entry_idx)->filename, ".")) {
                    is_dir_first_block = true;
                }
                // 不是‘.‘或者’..’目录
                else if (strcmp((dir_e + dir_entry_idx)->filename, ".") && strcmp((dir_e + dir_entry_idx)->filename, "..")) {
                    // 统计此扇区内的目录项个数,用来判断删除目录项后是否回收该扇区
                    dir_entry_cnt++;
                    // 如果找到此i结点,就将其记录在dir_entry_found
                    if ((dir_e + dir_entry_idx)->i_no == inode_no) {
                        // 确保目录中只有一个编号为inode_no的inode,找到一次后dir_entry_found就不再是NULL
                        ASSERT(dir_entry_found == NULL);
                        // 找到后也继续遍历,统计总共的目录项数
                        dir_entry_found = dir_e + dir_entry_idx;
                    }
                }
            }
        }

        // 1.3、若此扇区未找到该目录项,继续在下个扇区中找
        if (dir_entry_found == NULL) continue;

        // 1.4、在此扇区中找到目录项后,清除该目录项并判断是否回收扇区,随后退出循环直接返回
        ASSERT(dir_entry_cnt >= 1);
        // 1.5、若除目录第1个扇区外,若该扇区上只有该目录项自己,则将整个扇区回收
        if (dir_entry_cnt == 1 && !is_dir_first_block) {
            // 1.5.1、在块位图中回收该块
            uint32_t block_bitmap_idx = all_blocks[block_idx] - part->sb->data_start_lba;
            bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
            bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);

            // 1.5.2、将块地址从数组i_sectors或索引表中去掉
            if (block_idx < 12) {
                dir_inode->i_sectors[block_idx] = 0;
            }
            else {
                uint32_t indirect_blocks = 0;
                for (uint32_t i = 12; i < 140; i++) {
                    if (all_blocks[i]) indirect_blocks++;
                }

                if (indirect_blocks > 1) {
                    // 间接索引表中还包括其它间接块,仅在索引表中擦除当前这个间接块地址
                    all_blocks[block_idx] = 0;
                    ide_write(part->my_disk, dir_inode->i_sectors[12], all_blocks + 12, 1);
                }
                else {
                    // 间接索引表中就当前这1个间接块,直接把间接索引表所在的块回收,然后擦除间接索引表块地址
                    block_bitmap_idx = dir_inode->i_sectors[12] - part->sb->data_start_lba;
                    bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
                    bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
                    dir_inode->i_sectors[12] = 0;
                }
            }
        }
        // 仅将该目录项清空
        else {
            memset(dir_entry_found, 0, dir_entry_size);
            ide_write(part->my_disk, all_blocks[block_idx], io_buf, 1);
        }

        // 更新i结点信息并同步到硬盘
        ASSERT(dir_inode->i_size >= dir_entry_size);
        dir_inode->i_size -= dir_entry_size;
        inode_sync(part, dir_inode);

        return true;
    }
    // 所有块中未找到则返回false,若出现这种情况应该是serarch_file出错了
    return false;
}

/**
 * @description: 读取目录,成功返回1个目录项,失败返回NULL
 * @param {dir*} dir 目录
 * @return {*}
 */
struct dir_entry* dir_read(struct dir* dir) {
    struct dir_entry* dir_e = (struct dir_entry*)dir->dir_buf;
    struct inode* dir_inode = dir->inode;
    uint32_t all_blocks[140] = { 0 };
    uint32_t block_cnt = 12;
    // 先处理直接块
    for (uint8_t i = 0; i < 12; i++) {
        all_blocks[i] = dir_inode->i_sectors[i];
    }
    // 若含有一级间接块表
    if (dir_inode->i_sectors[12] != 0) {
        ide_read(cur_part->my_disk, dir_inode->i_sectors[12], all_blocks + 12, 1);
        block_cnt = 140;
    }

    // 当前目录项的偏移,此项用来判断是否是之前已经返回过的目录项
    uint32_t cur_dir_entry_pos = 0;
    // 目录项大小
    uint32_t dir_entry_size = cur_part->sb->dir_entry_size;
    // 1扇区内可容纳的目录项个数
    uint32_t dir_entrys_per_sec = SECTOR_SIZE / dir_entry_size;
    // 因为此目录内可能删除了某些文件或子目录,所以要遍历所有块
    for (uint8_t block_idx = 0; block_idx < block_cnt; block_idx++) {
        if (dir->dir_pos >= dir_inode->i_size) {
            return NULL;
        }
        // 如果此块地址为0,即空块,继续读出下一块
        if (all_blocks[block_idx] == 0) {
            continue;
        }
        memset(dir_e, 0, SECTOR_SIZE);
        ide_read(cur_part->my_disk, all_blocks[block_idx], dir_e, 1);
        // 遍历扇区内所有目录项
        for (uint32_t dir_entry_idx = 0; dir_entry_idx < dir_entrys_per_sec; dir_entry_idx++) {
            if ((dir_e + dir_entry_idx)->f_type) {
                // 如果f_type不等于0,即不等于FT_UNKNOWN
                if (cur_dir_entry_pos < dir->dir_pos) {
                    // 判断是不是最新的目录项,避免返回曾经已经返回过的目录项
                    cur_dir_entry_pos += dir_entry_size;
                    continue;
                }
                ASSERT(cur_dir_entry_pos == dir->dir_pos);
                // 更新为新位置,即下一个返回的目录项地址
                dir->dir_pos += dir_entry_size;
                return dir_e + dir_entry_idx;
            }
        }
    }
    return NULL;
}

/**
 * @description: 判断目录是否为空
 * @param {dir*} dir
 * @return {*}
 */
bool dir_is_empty(struct dir* dir) {
    struct inode* dir_inode = dir->inode;
    // 若目录下只有.和..这两个目录项则目录为空
    return (dir_inode->i_size == cur_part->sb->dir_entry_size * 2);
}

/**
 * @description: 在父目录parent_dir中删除child_dir
 * @param {dir*} parent_dir 父目录
 * @param {dir*} child_dir 要删除的目录
 * @return {*}
 */
int32_t dir_remove(struct dir* parent_dir, struct dir* child_dir) {
    struct inode* child_dir_inode = child_dir->inode;
    // 空目录只在inode->i_sectors[0]中有扇区,其它扇区都应该为空
    int32_t block_idx = 1;
    while (block_idx < 13) {
        ASSERT(child_dir_inode->i_sectors[block_idx] == 0);
        block_idx++;
    }
    void* io_buf = sys_malloc(SECTOR_SIZE * 2);
    if (io_buf == NULL) {
        printk("dir_remove: malloc for io_buf failed\n");
        return -1;
    }

    // 在父目录parent_dir中删除子目录child_dir对应的目录项
    delete_dir_entry(cur_part, parent_dir, child_dir_inode->i_no, io_buf);

    // 回收inode中i_secotrs中所占用的扇区,并同步inode_bitmap和block_bitmap
    inode_release(cur_part, child_dir_inode->i_no);
    sys_free(io_buf);
    return 0;
}














