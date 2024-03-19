// os/src/lib/string.h
#ifndef __LIB_STRING_H
#define __LIB_STRING_H

#include "stdin.h"

/* 将dst_起始的size个字节置为value */
void memset(void* dst_, uint8_t value, uint32_t size);
/* 将src_起始的size个字节复制到dst_ */
void memcpy(void* dst_, const void* src_, uint32_t size);
/* 连续比较以地址a_和地址b_开头的size个字节,若相等则返回0,若a_大于b_返回+1,否则返回-1 */
int memcmp(const void* a_, const void* b_, uint32_t size);
/* 将字符串从src_复制到dst_ */
char* strcpy(char* dst_, const char* src_);
/* 返回字符串长度 */
uint32_t strlen(const char* str);
/* 比较两个字符串,若a_中的字符大于b_中的字符返回1,相等时返回0,否则返回-1. */
int8_t strcmp (const char *a, const char *b); 
/* 从前往后查找字符串str中首次出现字符ch的地址(不是下标,是地址) */
char* strchr(const char* string, const uint8_t ch);
/* 从后往前查找字符串str中首次出现字符ch的地址(不是下标,是地址) */
char* strrchr(const char* string, const uint8_t ch);
/* 将字符串src_拼接到dst_后,将回拼接的串地址 */
char* strcat(char* dst_, const char* src_);
/* 在字符串str中查找指定字符ch出现的次数 */
uint32_t strchrs(const char* filename, uint8_t ch);

#endif