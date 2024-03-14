#ifndef __LIB_KERNEL_PRINT_H
#define __LIB_KERNEL_PRINT_H

#include "stdin.h"

/* 把一个字符写到控制台光标处 */
void put_char(uint8_t char_asci);
/* 把一个字符串写到控制台光标处 */
void put_str(char* _str);
/* 把一个32位无符号数写到控制台光标处，十进制 */
void put_int(uint32_t num);
/* 把一个32位无符号数写到控制台光标处，十六进制 */
void put_hex(uint32_t num);

#endif

