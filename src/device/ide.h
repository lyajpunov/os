#ifndef __DEVICE_IDE_H
#define __DEVICE_IDE_H

#include "stdin.h"
#include "sync.h"
#include "bitmap.h"

/* 定义硬盘各寄存器的端口号 */
#define reg_data(channel)	    (channel->port_base + 0)
#define reg_error(channel)	    (channel->port_base + 1)
#define reg_sect_cnt(channel)   (channel->port_base + 2)
#define reg_lba_l(channel)	    (channel->port_base + 3)
#define reg_lba_m(channel)	    (channel->port_base + 4)
#define reg_lba_h(channel)	    (channel->port_base + 5)
#define reg_dev(channel)	    (channel->port_base + 6)
#define reg_status(channel)	    (channel->port_base + 7)
#define reg_cmd(channel)	    (reg_status(channel))
#define reg_alt_status(channel) (channel->port_base + 0x206)
#define reg_ctl(channel)	    (reg_alt_status(channel))

/* reg_alt_status寄存器的一些关键位 */
#define BIT_STAT_BSY	 0x80	      // 硬盘忙
#define BIT_STAT_DRDY	 0x40	      // 驱动器准备好	 
#define BIT_STAT_DRQ	 0x8	      // 数据传输准备好了

/* device寄存器的一些关键位 */
#define BIT_DEV_MBS	0xa0	          // 第7位和第5位固定为1
#define BIT_DEV_LBA	0x40              // 第6位为1，LBA寻址方式
#define BIT_DEV_MASTER	0x00          // 第4位为1，从盘
#define BIT_DEV_SLAVE	0x10          // 第4位为1，从盘

/* 一些硬盘操作的指令 */
#define CMD_IDENTIFY	   0xec	      // identify指令
#define CMD_READ_SECTOR	   0x20       // 读扇区指令
#define CMD_WRITE_SECTOR   0x30	      // 写扇区指令

/* 定义可读写的最大扇区数,调试用的 */
#define max_lba ((100*1024*1024/512) - 1)

/* 一个扇区多少字节 */
#define SEC_BIT 512

/* 分区结构 */
struct partition {
    uint32_t start_lba;		    // 起始扇区
    uint32_t sec_cnt;		    // 扇区数
    struct disk* my_disk;	    // 分区所属的硬盘
    struct list_elem part_tag;	// 用于队列中的标记,分区都会被加入分区链表
    char name[8];		        // 分区名称
    struct super_block* sb;	    // 本分区的超级块
    struct bitmap block_bitmap;	// 块位图
    struct bitmap inode_bitmap;	// i结点位图
    struct list open_inodes;	// 本分区打开的i结点队列
};

/* 硬盘结构 */
struct disk {
    char name[8];			         // 本硬盘的名称，如sda等
    struct ide_channel* my_channel;	 // 此块硬盘归属于哪个ide通道
    uint8_t dev_no;			         // 本硬盘是主0还是从1
    struct partition prim_parts[4];	 // 主分区顶多是4个
    struct partition logic_parts[8]; // 逻辑分区数量无限,但总得有个支持的上限,那就支持8个
};

/* ata通道结构 */
struct ide_channel {
    char name[8];	    	    // 本ata通道名称 
    uint16_t port_base;		    // 本通道的起始端口号
    uint8_t irq_no;		        // 本通道所用的中断号
    struct lock lock;		    // 通道锁
    bool expecting_intr;		// 表示等待硬盘的中断
    struct semaphore disk_done;	// 用于阻塞、唤醒驱动程序
    struct disk devices[2];	    // 一个通道上连接两个硬盘，一主一从
};


extern uint8_t channel_cnt;
extern struct ide_channel channels[];
extern struct list partition_list;

/* ide硬盘初始化 */
void ide_init(void);
/* 硬盘hd的lba起始扇区，读取sec_cnt个扇区到buf */
void ide_read(struct disk* hd, uint32_t lba, void* buf, uint32_t sec_cnt);
/* 将buf中sec_cnt扇区数据写入硬盘 */
void ide_write(struct disk* hd, uint32_t lba, void* buf, uint32_t sec_cnt);

#endif
