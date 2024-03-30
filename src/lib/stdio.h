#ifndef __LIB_STDIO_H
#define __LIB_STDIO_H

#include "stdin.h"

#define va_list char*

/* 用户打印函数 */
uint32_t printf(const char* str, ...);
/* 内核打印函数 */
uint32_t printk(const char* format, ...);
/* 格式化字符串函数 */
uint32_t vsprintf(char* str, const char* format, va_list ap);
/* 格式化字符串到buf中 */
uint32_t sprintf(char* buf, const char* format, ...);

#endif
