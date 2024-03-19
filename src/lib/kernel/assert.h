// os/src/lib/kernel/assert.h

#ifndef __LIB_KERNEL_ASSERT_H
#define __LIB_KERNEL_ASSERT_H
void panic_spin(char* filename, int line, const char* func, const char* condition);

/***************************  __VA_ARGS__  *******************************
__FILE__：这是一个预定义的宏，在编译时会被当前源文件的文件名所替代。
__LINE__：同样是一个预定义的宏，在编译时会被当前源文件的行号所替代。
__func__：也是一个预定义的宏，会被当前函数的名称所替代。
__VA_ARGS__：表示可变参数列表，允许宏在调用时接受不定数量的参数在这里，
             它用于接收传递给 PANIC 宏的额外参数。
***********************************************************************/
#define PANIC(...) panic_spin (__FILE__, __LINE__, __func__, __VA_ARGS__)

#ifdef NDEBUG
    #define ASSERT(CONDITION) ((void)0)
#else
    #define ASSERT(CONDITION) if (CONDITION) {} else {PANIC(#CONDITION);}
#endif /*__NDEBUG */

#endif /*__KERNEL_ASSERT_H*/