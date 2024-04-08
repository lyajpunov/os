/*
 * @Author: lyajpunov 1961558693@qq.com
 * @Date: 2024-03-21 21:00:06
 * @LastEditors: lyajpunov 1961558693@qq.com
 * @LastEditTime: 2024-04-08 01:47:44
 * @FilePath: /os/src/lib/kernel/print.c
 * @Description: 内核打印函数
 * 
 * Copyright (c) 2024 by ${git_name_email}, All Rights Reserved. 
 */
#include "print.h"
#include "stdin.h"

/* 打印字符串 */
void put_str(char* _str) {
    while(*_str) {
        put_char(*_str);
        _str++;
    }
}

static const char int_digits[] = "0123456789";
/* 把一个32位无符号数写到控制台光标处，十进制 */
void put_int(uint32_t num) {
    char int_string[20];  // 无符号整数转换为十六进制最多占据8位字符，再加上字符串结束符'\0'
    int i = 0;

    if (num == 0) {
        int_string[0] = '0';
        int_string[1] = '\0';
    } else {
        while (num != 0) {
            int_string[i] = int_digits[num % 10];
            num /= 10;
            i++;
        }
        int_string[i] = '\0';

        // 反转字符串
        int start = 0, end = i - 1;
        while (start < end) {
            char temp = int_string[start];
            int_string[start] = int_string[end];
            int_string[end] = temp;
            start++;
            end--;
        }
    }
    put_str(int_string);
}

static const char hex_digits[] = "0123456789ABCDEF";
/* 把一个32位无符号数写到控制台光标处，十六进制 */
void put_hex(uint32_t num) {
    char hex_string[9];  // 无符号整数转换为十六进制最多占据8位字符，再加上字符串结束符'\0'
    int i = 0;

    if (num == 0) {
        hex_string[0] = '0';
        hex_string[1] = '\0';
    } else {
        while (num != 0) {
            hex_string[i] = hex_digits[num % 16];
            num /= 16;
            i++;
        }
        hex_string[i] = '\0';

        // 反转字符串
        int start = 0, end = i - 1;
        while (start < end) {
            char temp = hex_string[start];
            hex_string[start] = hex_string[end];
            hex_string[end] = temp;
            start++;
            end--;
        }
    }
    put_char('0');
    put_char('x');
    put_str(hex_string);
}
