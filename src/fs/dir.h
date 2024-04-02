#ifndef __FS_DIR_H
#define __FS_DIR_H

#include "fs.h"
#include "ide.h"
#include "stdin.h"
#include "inode.h"


#define MAX_FILE_NAME_LEN  16	 // 最大文件名长度

/* 目录结构 */
struct dir {
    struct inode* inode;
    uint32_t dir_pos;	        // 记录在目录内的偏移
    uint8_t dir_buf[512];       // 目录的数据缓存
};

/* 目录项结构 */
struct dir_entry {
    char filename[MAX_FILE_NAME_LEN];  // 普通文件或目录名称
    uint32_t i_no;		               // 普通文件或目录对应的inode编号
    enum file_types f_type;	           // 文件类型
};

extern struct dir root_dir;             // 根目录

/* 打开根目录 */
void open_root_dir(struct partition* part);
/* 打开目录 */
struct dir* dir_open(struct partition* part, uint32_t inode_no);
/* 关闭目录 */
void dir_close(struct dir* dir);
/* 查找目录项 */
bool search_dir_entry(struct partition* part, struct dir* pdir, const char* name, struct dir_entry* dir_e);
/* 创建目录项，并初始化 */
void create_dir_entry(char* filename, uint32_t inode_no, uint8_t file_type, struct dir_entry* p_de);
/* 将目录项p_de写入父目录parent_dir中,io_buf由主调函数提供 */
bool sync_dir_entry(struct dir* parent_dir, struct dir_entry* p_de, void* io_buf);


#endif
