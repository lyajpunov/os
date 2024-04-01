#include "fs.h"
#include "ide.h"
#include "dir.h"
#include "cmos.h"
#include "stdio.h"
#include "stdin.h"
#include "inode.h"
#include "memory.h"
#include "string.h"
#include "assert.h"
#include "super_block.h"

// 默认情况下的操作分区
struct partition* cur_part;

/* 在分区链表中找到名为part_name的分区,并将其指针赋值给cur_part */
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

/* 格式化分区,也就是初始化分区的元信息,创建文件系统 */
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


/* 在磁盘上搜索文件系统,若没有则格式化分区创建文件系统 */
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
}





