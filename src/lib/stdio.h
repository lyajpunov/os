#ifndef __LIB_STDIO_H
#define __LIB_STDIO_H

#include "stdin.h"

#define va_list char*

uint32_t printf(const char* str, ...);
uint32_t printk(const char* format, ...);
uint32_t vsprintf(char* str, const char* format, va_list ap);

#endif
