#ifndef __KERNEL_SHELL_H
#define __KERNEL_SHELL_H

#include "fs.h"

// 最大支持键入128个字符的命令行输入
#define cmd_len 128	       
// 加上命令名外,最多支持15个参数
#define MAX_ARG_NR 16	 

extern char final_path[MAX_PATH_LEN];

void print_prompt(void);
void my_shell(void);

#endif
