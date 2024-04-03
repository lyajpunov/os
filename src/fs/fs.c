#include "fs.h"
#include "ide.h"
#include "dir.h"
#include "file.h"
#include "cmos.h"
#include "stdio.h"
#include "stdin.h"
#include "inode.h"
#include "memory.h"
#include "string.h"
#include "assert.h"
#include "console.h"
#include "super_block.h"

// 默认情况下的操作分区
struct partition* cur_part;

/**
 * @description: 在分区链表中找到名为part_name的分区,并将其指针赋值给cur_part
 * @param {list_elem*} pelem
 * @param {int} arg
 * @return {*}
 */
static bool mount_partition(struct list_elem* pelem, int arg) {
    // 获得分区名
    char* part_name = (char*)arg;
    // 获得链表遍历的分区
    struct partition* part = elem2entry(struct partition, part_tag, pelem);
    // 如果找到了链表中的分区
    if (!strcmp(part->name, part_name)) {
        // 默认情况下的分区改为当前分区
        cur_part = part;
        // 拿到当前分区的硬盘指针
        struct disk* hd = cur_part->my_disk;
        // sb_buf用来存储从硬盘上读入的超级块
        struct super_block* sb_buf = (struct super_block*)sys_malloc(SECTOR_SIZE);
        // 分区的超级块指针指向一块空白区域
        cur_part->sb = (struct super_block*)sys_malloc(sizeof(struct super_block));
        // 初始化buf
        memset(sb_buf, 0, SECTOR_SIZE);
        // 读入超级块
        ide_read(hd, cur_part->start_lba + 1, sb_buf, 1);
        // 把sb_buf中超级块的信息复制到分区的超级块sb中
        memcpy(cur_part->sb, sb_buf, sizeof(struct super_block));

        // 将块位图读入内存
        cur_part->block_bitmap.bits = (uint8_t*)sys_malloc(sb_buf->block_bitmap_sects * SECTOR_SIZE);
        if (cur_part->block_bitmap.bits == NULL) {
            PANIC("alloc memory failed!");
        }
        cur_part->block_bitmap.btmp_bytes_len = sb_buf->block_bitmap_sects * SECTOR_SIZE;
        ide_read(hd, sb_buf->block_bitmap_lba, cur_part->block_bitmap.bits, sb_buf->block_bitmap_sects);

        // 将inode位图读入内存
        cur_part->inode_bitmap.bits = (uint8_t*)sys_malloc(sb_buf->inode_bitmap_sects * SECTOR_SIZE);
        if (cur_part->inode_bitmap.bits == NULL) {
            PANIC("alloc memory failed!");
        }
        cur_part->inode_bitmap.btmp_bytes_len = sb_buf->inode_bitmap_sects * SECTOR_SIZE;
        ide_read(hd, sb_buf->inode_bitmap_lba, cur_part->inode_bitmap.bits, sb_buf->inode_bitmap_sects);

        // 初始化以打开inode链表
        list_init(&cur_part->open_inodes);

        // 打印挂载成功
        printk("mount %s done!\n", part->name);

        // 使list_traversal停止遍历
        return true;
    }
    // 使list_traversal继续遍历
    return false;
}

/**
 * @description: 格式化分区,也就是初始化分区的元信息,创建文件系统
 * @param {partition*} part 分区
 * @return {*}
 */
static void partition_format(struct partition* part) {
    // 导引块占一个块
    uint32_t boot_sector_sects = 1;
    // 超级块占一个块
    uint32_t super_block_sects = 1;
    // inode位图占的块数，这里也是一个块
    uint32_t inode_bitmap_sects = DIV_ROUND_UP(MAX_FILES_PER_PART, BITS_PER_SECTOR);
    // inode占的块数
    uint32_t inode_table_sects = DIV_ROUND_UP(((sizeof(struct inode) * MAX_FILES_PER_PART)), SECTOR_SIZE);
    // 已使用的块数
    uint32_t used_sects = boot_sector_sects + super_block_sects + inode_bitmap_sects + inode_table_sects;
    // 空闲块数
    uint32_t free_sects = part->sec_cnt - used_sects;
    // 空闲块位图占据的块数
    uint32_t block_bitmap_sects = DIV_ROUND_UP(free_sects, BITS_PER_SECTOR);
    // 空闲块位图长度
    uint32_t block_bitmap_bit_len = DIV_ROUND_UP((free_sects - block_bitmap_sects), BITS_PER_SECTOR);


    /* 超级块初始化 */
    struct super_block sb;
    sb.magic = SUPER_BLOCK_MAGIC;
    sb.sec_cnt = part->sec_cnt;
    sb.inode_cnt = MAX_FILES_PER_PART;
    sb.part_lba_base = part->start_lba;
    // 第0块是引导块，第一块是超级块，之后是空闲块位图
    sb.block_bitmap_lba = sb.part_lba_base + 2;
    sb.block_bitmap_sects = block_bitmap_sects;
    sb.inode_bitmap_lba = sb.block_bitmap_lba + sb.block_bitmap_sects;
    sb.inode_bitmap_sects = inode_bitmap_sects;
    sb.inode_table_lba = sb.inode_bitmap_lba + sb.inode_bitmap_sects;
    sb.inode_table_sects = inode_table_sects;
    sb.data_start_lba = sb.inode_table_lba + sb.inode_table_sects;
    sb.root_inode_no = 0;
    sb.dir_entry_size = sizeof(struct dir_entry);

    printk("%s info:\n", part->name);
    printk("magic:0x%x\n   \
            part_lba_base:0x%x\n   \
            all_sectors:0x%x\n   \
            inode_cnt:0x%x\n   \
            block_bitmap_lba:0x%x\n   \
            block_bitmap_sectors:0x%x\n   \
            inode_bitmap_lba:0x%x\n   \
            inode_bitmap_sectors:0x%x\n   \
            inode_table_lba:0x%x\n   \
            inode_table_sectors:0x%x\n   \
            data_start_lba:0x%x\n", \
        sb.magic, \
        sb.part_lba_base, \
        sb.sec_cnt, \
        sb.inode_cnt, \
        sb.block_bitmap_lba, \
        sb.block_bitmap_sects, \
        sb.inode_bitmap_lba, \
        sb.inode_bitmap_sects, \
        sb.inode_table_lba, \
        sb.inode_table_sects, \
        sb.data_start_lba);

    // 拿到硬盘的指针
    struct disk* hd = part->my_disk;
    // 将超级块写入本分区的第一扇区
    ide_write(hd, part->start_lba + 1, &sb, 1);
    // 超级块的lba
    printk("   super_block_lba:0x%x\n", part->start_lba + 1);

    // 找出数据量最大的元信息,用其尺寸做存储缓冲区
    uint32_t buf_size = (sb.block_bitmap_sects >= sb.inode_bitmap_sects ? sb.block_bitmap_sects : sb.inode_bitmap_sects);
    buf_size = (buf_size >= sb.inode_table_sects ? buf_size : sb.inode_table_sects) * SECTOR_SIZE;
    // 申请的内存由内存管理系统清0后返回
    uint8_t* buf = (uint8_t*)sys_malloc(buf_size);
    // 将块位图初始化并写入sb.block_bitmap_lba,第0个块留给根目录
    buf[0] |= 0x01;
    uint32_t block_bitmap_last_byte = block_bitmap_bit_len / 8;
    uint8_t  block_bitmap_last_bit = block_bitmap_bit_len % 8;
    uint32_t last_size = SECTOR_SIZE - (block_bitmap_last_byte % SECTOR_SIZE);
    // 先将位图最后一字节到其所在的扇区的结束全置为1,即超出实际块数的部分直接置为已占用
    memset(&buf[block_bitmap_last_byte], 0xff, last_size);
    // 再将上一步中覆盖的最后一字节内的有效位重新置0
    for (uint8_t bit_idx = 0; bit_idx <= block_bitmap_last_bit; bit_idx++) {
        buf[block_bitmap_last_byte] &= ~(1 << bit_idx);
    }
    // 写入块位图
    ide_write(hd, sb.block_bitmap_lba, buf, sb.block_bitmap_sects);

    // 清空缓冲区
    memset(buf, 0, buf_size);
    // 第0个inode节点是根目录
    buf[0] |= 0x01;
    // 写入inode节点位图，这里直接写入的原因是一共就支持4096个节点，正好一个扇区
    ide_write(hd, sb.inode_bitmap_lba, buf, sb.inode_bitmap_sects);

    // 清空缓冲区
    memset(buf, 0, buf_size);
    // 将缓冲区指正改为指向inode节点
    struct inode* inode = (struct inode*)buf;
    // 修改第0个inode的节点大小
    inode->i_size = sb.dir_entry_size * 2;
    // 修改第0个inode的节点编号
    inode->i_no = 0;
    // 修改第0个inode的第一个扇区指针
    inode->i_sectors[0] = sb.data_start_lba;
    // 修改第0个inode的时间
    inode->ctime = get_time();
    inode->mtime = get_time();
    inode->atime = get_time();
    // 修改第0个inode的权限
    inode->privilege = 4;
    // 写入inode数组
    ide_write(hd, sb.inode_table_lba, buf, sb.inode_table_sects);

    // 写入根目录的两个目录项.和..
    // 清空缓冲区
    memset(buf, 0, buf_size);
    // 目录项指正指向buf
    struct dir_entry* p_de = (struct dir_entry*)buf;
    // 初始化当前目录
    memcpy(p_de->filename, ".", 1);
    p_de->i_no = 0;
    p_de->f_type = FT_DIRECTORY;
    p_de++;
    // 初始化当前目录的父目录
    memcpy(p_de->filename, "..", 2);
    p_de->i_no = 0;   // 根目录的父目录依然是根目录自己
    p_de->f_type = FT_DIRECTORY;
    // 将目录项写入硬盘
    ide_write(hd, sb.data_start_lba, buf, 1);

    // 完成了一个分区的全部初始化工作，释放buf
    printk("   root_dir_lba:0x%x\n", sb.data_start_lba);
    printk("%s format done\n", part->name);
    sys_free(buf);
}

/**
 * @description: 将最上层路径名称解析出来
 * @param {char*} pathname 输入路径
 * @param {char*} name_store 返回的最上层名字
 * @return {*} 返回减去最上层名字后的路径字符串指针
 */
static char* path_parse(char* pathname, char* name_store) {
    // 根目录不需要单独解析
    if (pathname[0] == '/') {
        // 路径中出现1个或多个连续的字符'/',将这些'/'跳过,如"///a/b"
        while (*(++pathname) == '/');
    }

    // 开始一般的路径解析
    while (*pathname != '/' && *pathname != 0) {
        *name_store++ = *pathname++;
    }
    // 若路径字符串为空则返回NULL
    if (pathname[0] == 0) return NULL;
    // 返回去处最顶层目录的目录
    return pathname;
}

/**
 * @description: 路径深度
 * @param {char*} pathname 路径
 * @return {*} 路径深度
 */
int32_t path_depth_cnt(char* pathname) {
    if (pathname == NULL) return 0;
    char* p = pathname;
    // 用于path_parse的参数做路径解析
    char name[MAX_FILE_NAME_LEN];
    // 深度
    uint32_t depth = 0;
    // 解析路径,从中拆分出各级名称
    p = path_parse(p, name);
    while (name[0]) {
        depth++;
        memset(name, 0, MAX_FILE_NAME_LEN);
        // 如果p不等于NULL,继续分析路径 #TODO
        if (p != NULL) p = path_parse(p, name);
    }
    return depth;
}

/**
 * @description: 搜索文件pathname,若找到则返回其inode号,否则返回-1
 * @param {char*} pathname 路径名
 * @param {path_search_record*} searched_record 搜索的过程中保存的上层信息
 * @return {*}
 */
int32_t search_file(const char* pathname, struct path_search_record* searched_record) {
    // 如果待查找的是根目录,为避免下面无用的查找,直接返回已知根目录信息
    if (!strcmp(pathname, "/") || !strcmp(pathname, "/.") || !strcmp(pathname, "/..")) {
        // 找到此目录的父目录为根目录
        searched_record->parent_dir = &root_dir;
        // 将找到的文件类型标为目录
        searched_record->file_type = FT_DIRECTORY;
        // 搜索路径置空
        searched_record->searched_path[0] = 0;
        return 0;
    }

    // 路径长度需要保证小于最长的路径长度，并且以 / 开头
    uint32_t path_len = strlen(pathname);
    ASSERT(pathname[0] == '/' && path_len > 1 && path_len < MAX_PATH_LEN);

    // 子路径    
    char* sub_path = (char*)pathname;
    // 根目录
    struct dir* parent_dir = &root_dir;
    // 目录项
    struct dir_entry dir_e;
    // 解析出来的各级路径
    char name[MAX_FILE_NAME_LEN] = { 0 };
    // 设置父目录
    searched_record->parent_dir = parent_dir;
    // 设置文件类型为未知
    searched_record->file_type = FT_UNKNOWN;
    // 父目录的inode号
    uint32_t parent_inode_no = 0;
    // 将最上层路劲解析出来给name，同事sub_path指针指向下一个目录
    sub_path = path_parse(sub_path, name);
    // 若第一个字符就是结束符,结束循环
    while (name[0]) {
        ASSERT(strlen(searched_record->searched_path) < 512);
        // 记录已存在的父目录
        strcat(searched_record->searched_path, "/");
        strcat(searched_record->searched_path, name);
        // 在所给的目录中查找文件
        if (search_dir_entry(cur_part, parent_dir, name, &dir_e)) {
            memset(name, 0, MAX_FILE_NAME_LEN);
            // 若sub_path不等于NULL,也就是未结束时继续拆分路径
            if (sub_path) {
                sub_path = path_parse(sub_path, name);
            }
            // 如果被打开的是目录
            if (FT_DIRECTORY == dir_e.f_type) {
                // 父目录的inode编号
                parent_inode_no = parent_dir->inode->i_no;
                // 关闭当前目录
                dir_close(parent_dir);
                // 更新父目录
                parent_dir = dir_open(cur_part, dir_e.i_no);
                // 将新的父目录记录在结构体
                searched_record->parent_dir = parent_dir;
                continue;
            }
            // 被打开的是普通文件
            else if (FT_REGULAR == dir_e.f_type) {
                // 更新搜索路径的文件类型
                searched_record->file_type = FT_REGULAR;
                // 直接返回路径的inode编号
                return dir_e.i_no;
            }
        }
        else {
            // 找不到目录项时,要留着parent_dir不要关闭,若是创建新文件的话需要在parent_dir中创建
            return -1;
        }
    }

    // 执行到此,必然是遍历了完整路径并且最后一项不是文件而是目录。
    dir_close(searched_record->parent_dir);
    // 保存被查找目录的直接父目录
    searched_record->parent_dir = dir_open(cur_part, parent_inode_no);
    // 搜索路径的文件类型置为目录
    searched_record->file_type = FT_DIRECTORY;
    // 返回目录项的inode
    return dir_e.i_no;
}

/**
 * @description: 打开或创建文件成功后,返回文件描述符,否则返回-1
 * @param {char*} pathname 路径
 * @param {uint8_t} flags 打开标识，只读，只写，读写，创建 只支持文件打开，不支持目录打开,以/结尾的都是目录
 * @return {*}
 */
int32_t sys_open(const char* pathname, uint8_t flags) {
    // 如果结尾是 ‘/’ 则最后是目录
    if (pathname[strlen(pathname) - 1] == '/') {
        printk("sys_open error: can`t open a directory %s\n", pathname);
        return -1;
    }
    if (flags > 7) {
        printk("sys_open error: flag is %d > 7\n", flags);
        return -1;
    }

    // 文件描述符默认为-1，找不到
    int32_t fd = -1;

    // 初始化搜索路径结构体
    struct path_search_record searched_record;
    memset(&searched_record, 0, sizeof(struct path_search_record));

    // 记录目录深度.帮助判断中间某个目录不存在的情况
    uint32_t pathname_depth = path_depth_cnt((char*)pathname);

    // 先检查文件是否存在
    int inode_no = search_file(pathname, &searched_record);
    bool found = inode_no != -1 ? true : false;

    // 最后找到的文件是目录
    if (searched_record.file_type == FT_DIRECTORY) {
        printk("%d\n", searched_record.parent_dir->inode->i_no);
        printk("%s\n", searched_record.searched_path);
        printk("can`t open a direcotry with open(), use opendir() to instead\n");
        dir_close(searched_record.parent_dir);
        return -1;
    }

    // 找到目录深度
    uint32_t path_searched_depth = path_depth_cnt(searched_record.searched_path);

    // 访问路径的目录深度和搜索的实际目录深度不等，有些目录没访问到
    if (pathname_depth != path_searched_depth) {
        printk("cannot access %s: Not a directory, subpath %s is`t exist\n", pathname, searched_record.searched_path);
        dir_close(searched_record.parent_dir);
        return -1;
    }

    // 若是在最后一个路径上没找到,并且并不是要创建文件,直接返回-1
    if (!found && !(flags & O_CREAT)) {
        printk("in path %s, file %s is`t exist\n", \
            searched_record.searched_path, \
            (strrchr(searched_record.searched_path, '/') + 1));
        dir_close(searched_record.parent_dir);
        return -1;
    }
    // 若要创建的文件已存在
    else if (found && flags & O_CREAT) {
        printk("%s has already exist!\n", pathname);
        dir_close(searched_record.parent_dir);
        return -1;
    }

    switch (flags & O_CREAT) {
    case O_CREAT:
        // 这个标志位是创建文件
        fd = file_create(searched_record.parent_dir, (strrchr(pathname, '/') + 1), flags);
        dir_close(searched_record.parent_dir);
        break;
    default:
        // 其余标志位是打开文件
        fd = file_open(inode_no, flags);
    }

    // 返回文件描述符
    return fd;
}

/**
 * @description: 将文件描述符转化为文件表的下标 
 * @param {uint32_t} local_fd 本地的文件描述符下标
 * @return {*}
 */
static uint32_t fd_local2global(uint32_t local_fd) {
    struct task_struct* cur = running_thread();
    int32_t global_fd = cur->fd_table[local_fd];
    ASSERT(global_fd >= 0 && global_fd < MAX_FILE_OPEN);
    return (uint32_t)global_fd;
}

/**
 * @description: 关闭文件描述符fd指向的文件,成功返回0,否则返回-1
 * @param {int32_t} fd 文件描述符
 * @return {*}
 */
int32_t sys_close(int32_t fd) {
    // 返回值默认为-1,即失败
    int32_t ret = -1;
    if (fd > 2) {
        uint32_t _fd = fd_local2global(fd);
        // 关闭文件
        ret = file_close(&file_table[_fd]);
        // 使该文件描述符位可用
        running_thread()->fd_table[fd] = -1;
    }
    return ret;
}

/**
 * @description: 将buf中连续count个字节写入文件描述符fd,成功则返回写入的字节数,失败返回-1
 * @param {int32_t} fd 文件描述符
 * @param {void*} buf 写入字符串
 * @param {uint32_t} count 写入长度
 * @return {*}
 */
int32_t sys_write(int32_t fd, const void* buf, uint32_t count) {
    if (fd < 0) {
        console_put_str("sys_write: fd error\n");
        return -1;
    }
    if (fd == stdout_no) {
        char tmp_buf[1024] = { 0 };
        memcpy(tmp_buf, buf, count);
        console_put_str(tmp_buf);
        return count;
    }
    uint32_t _fd = fd_local2global(fd);
    struct file* wr_file = &file_table[_fd];
    if (wr_file->fd_flag & O_WRONLY || wr_file->fd_flag & O_RDWR) {
        uint32_t bytes_written = file_write(wr_file, buf, count);
        return bytes_written;
    }
    else {
        console_put_str("sys_write: not allowed to write file without flag O_RDWR or O_WRONLY\n");
        return -1;
    }
}


/**
 * @description: 从文件描述符fd指向的文件中读取count个字节到buf,若成功则返回读出的字节数,到文件尾则返回-1 
 * @param {int32_t} fd 文件描述符
 * @param {void*} buf 读取字符串缓冲
 * @param {uint32_t} count 字节数
 * @return {*}
 */
int32_t sys_read(int32_t fd, void* buf, uint32_t count) {
    if (fd < 0) {
        printk("sys_read: fd error\n");
        return -1;
    }
    ASSERT(buf != NULL);
    uint32_t _fd = fd_local2global(fd);
    return file_read(&file_table[_fd], buf, count);   
}



/**
 * @description: 在磁盘上搜索文件系统,若没有则格式化分区创建文件系统
 * @return {*}
 */
void filesys_init() {
    // sb_buf用来存储从硬盘上读入的超级块
    struct super_block* sb_buf = (struct super_block*)sys_malloc(SECTOR_SIZE);
    for (uint8_t channel_no = 0; channel_no < channel_cnt; channel_no++) {
        for (uint8_t dev_no = 0; dev_no < 2; dev_no++) {
            // 跨过存放操作系统的主盘
            if (dev_no == 0) continue;
            // 拿到硬盘指针
            struct disk* hd = &channels[channel_no].devices[dev_no];
            // 拿到分区指针
            struct partition* part = hd->prim_parts;

            for (uint8_t part_idx = 0; part_idx < 12; part_idx++) {
                // 4个主分区处理完了就开始处理四个逻辑分区
                if (part_idx == 4) part = hd->logic_parts;
                // 如果分区存在
                if (part->sec_cnt != 0) {
                    // 初始化buf
                    memset(sb_buf, 0, SECTOR_SIZE);
                    // 读出分区的超级块,根据魔数是否正确来判断是否存在文件系统
                    ide_read(hd, part->start_lba + 1, sb_buf, 1);
                    // 如果魔数正确，那么说明分区已经正确初始化
                    if (sb_buf->magic == SUPER_BLOCK_MAGIC) {
                        printk("%s has filesystem\n", part->name);
                    }
                    // 魔数不正确，直接初始化分区
                    else {
                        printk("formatting %s`s partition %s......\n", hd->name, part->name);
                        partition_format(part);
                    }
                }
                // 下一个分区
                part++;
            }
        }
    }
    // 释放buf
    sys_free(sb_buf);
    // 确定默认的操作分区
    char default_part[8] = "sdb1";
    // 挂载分区
    list_traversal(&partition_list, mount_partition, (int)default_part);

    // 将当前分区的根目录打开
    open_root_dir(cur_part);
    // 初始化文件表
    for (int i = 0; i < MAX_FILE_OPEN; i++) {
        file_table[i].fd_inode = NULL;
    }
}

