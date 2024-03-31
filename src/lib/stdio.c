#include "stdio.h"
#include "interrupt.h"
#include "global.h"
#include "string.h"
#include "syscall.h"
#include "print.h"
#include "syscall_init.h"
#include "math.h"
#include "assert.h"

#define va_start(ap, v) ap = (va_list)&v  // 把ap指向第一个固定参数v
#define va_arg(ap, t) *((t*)(ap += 4))	  // ap指向下一个参数并返回其值
#define va_end(ap) ap = NULL		      // 清除ap

const char cache[16] = "0123456789ABCDEF";
/* 将整型转换成字符(integer to ascii) */
static void itoa(uint32_t value, char** buf_ptr_addr, uint8_t base) {
    uint32_t m = value % base; // 求模,最先掉下来的是最低位
    uint32_t i = value / base; // 取整
    if (i) {                   // 倍数不为0则递归调用
        itoa(i, buf_ptr_addr, base);
    }
    *((*buf_ptr_addr)++) = cache[m];
}

/* 将长整型转换成字符(integer to ascii) */
static void itoa64(uint64_t value, char** buf_ptr_addr, uint8_t base) {
    uint64_t m=0, i=0;
    i = divide_u64_u32(value, base, &m);
    if (i) {                   // 倍数不为0则递归调用
        itoa64(i, buf_ptr_addr, base);
    }
    *((*buf_ptr_addr)++) = cache[m];
}

/* 将参数ap按照格式format输出到字符串str,并返回替换后str长度 */
uint32_t vsprintf(char* str, const char* format, va_list ap) {
    char* buf_ptr = str;
    const char* index_ptr = format;
    char index_char = *index_ptr;
    int32_t arg_int;
    int64_t arg_long;
    char* arg_str;
    while (index_char) {
        if (index_char != '%') {
            *(buf_ptr++) = index_char;
            index_char = *(++index_ptr);
            continue;
        }
        index_char = *(++index_ptr); // 得到%后面的字符
        switch (index_char) {
        case 's':
            arg_str = va_arg(ap, char*);
            strcpy(buf_ptr, arg_str);
            buf_ptr += strlen(arg_str);
            index_char = *(++index_ptr);
            break;

        case 'c':
            *(buf_ptr++) = va_arg(ap, char);
            index_char = *(++index_ptr);
            break;

        case 'd':
            arg_int = va_arg(ap, int);
            if (arg_int < 0) {           // 若是负数, 将其转为正数后,再正数前面输出个负号'-'
                arg_int = 0 - arg_int;
                *buf_ptr++ = '-';
            }
            itoa(arg_int, &buf_ptr, 10);
            index_char = *(++index_ptr);
            break;

        case 'x':
            arg_int = va_arg(ap, int);
            itoa(arg_int, &buf_ptr, 16);
            index_char = *(++index_ptr); // 跳过格式字符并更新index_char
            break;

        case 'o':
            arg_int = va_arg(ap, int);
            itoa(arg_int, &buf_ptr, 8);
            index_char = *(++index_ptr); // 跳过格式字符并更新index_char
            break;

        case 'l':
            index_char = *(++index_ptr); // 得到%l后面的字符
            arg_long = va_arg(ap, uint64_t);
            if (index_char == 'd') {
                if (arg_long < 0) {
                    arg_long = 0 - arg_long;
                    *buf_ptr++ = '-'; 
                }
                itoa64(arg_long, &buf_ptr, 10);
            }
            else if (index_char == 'u') {
                itoa64(arg_long, &buf_ptr, 10);
            }
            arg_int = va_arg(ap, int);   // 指针接着向后4位
            index_char = *(++index_ptr); // 跳过格式字符并更新index_char
            break;
        }
    }
    return strlen(str);
}

/* 格式化输出字符串，内核使用 */
uint32_t printk(const char* format, ...) {
    va_list args;
    va_start(args, format);	       // 使args指向format
    char buf[1024] = { 0 };	       // 用于存储拼接后的字符串
    vsprintf(buf, format, args);   // 按照format格式输出到buf
    va_end(args);                  // 销毁指针
    return sys_write(buf);         // 写入控制台
}

/* 格式化字符串到buf中 */
uint32_t sprintf(char* buf, const char* format, ...) {
   va_list args;
   uint32_t retval;
   va_start(args, format);
   retval = vsprintf(buf, format, args);
   va_end(args);
   return retval;
}


/* 格式化输出字符串format */
uint32_t printf(const char* format, ...) {
    va_list args;                                                // char* args
    va_start(args, format);	       // 使args指向format            // args = (char*) &format;
    char buf[1024] = { 0 };	       // 用于存储拼接后的字符串
    vsprintf(buf, format, args);   // 按照format格式输出到buf
    va_end(args);                  // 销毁指针                    // args = NULL
    return write(buf);             // 写入控制台
}
