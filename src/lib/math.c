#include "math.h"
#include "stdin.h"

/*******************************************************************************************************************
@author: liyajun
@data: 2024.3.31
@description: 64位除法，直接用除法会导致连接错误undefined reference to `__umoddi3' undefined reference to `__udivdi3
*******************************************************************************************************************/
uint64_t divide_u64_u32(uint64_t dividend, uint32_t divisor, uint64_t* remainder) {
    // 检查除数是否为0
    if (divisor == 0) {
        return 0; // 除数为0，返回0
    }

    // 用于存储商的变量
    uint64_t quotient = 0;

    // 从最高位开始逐步计算商
    for (int i = 63; i >= 0; i--) {
        // 将余数左移1位
        *remainder <<= 1;
        // 获取当前位的值
        uint64_t bit = (dividend >> i) & 1;
        // 将当前位的值加到余数的最低位
        *remainder |= bit;
        // 如果余数大于等于除数，则减去除数，并将对应位的商置1
        if (*remainder >= divisor) {
            *remainder -= divisor;
            quotient |= (1ULL << i);
        }
    }

    return quotient; // 返回商
}

/* 64位除法 */
uint64_t divide_u64_u32_no_mod(uint64_t dividend, uint32_t divisor) {
    // 检查除数是否为0
    if (divisor == 0) {
        return 0; // 除数为0，返回0
    }
    // 用于存储商的变量
    uint64_t quotient = 0;
    // 用于存储余数
    uint64_t remainder = 0;

    // 从最高位开始逐步计算商
    for (int i = 63; i >= 0; i--) {
        // 将余数左移1位
        remainder <<= 1;
        // 获取当前位的值
        uint64_t bit = (dividend >> i) & 1;
        // 将当前位的值加到余数的最低位
        remainder |= bit;
        // 如果余数大于等于除数，则减去除数，并将对应位的商置1
        if (remainder >= divisor) {
            remainder -= divisor;
            quotient |= (1ULL << i);
        }
    }
    return quotient; // 返回商
}

/* 64位整数求余数 */
uint64_t mod_u64_u32(uint64_t dividend, uint32_t divisor) {
    // 检查除数是否为0
    if (divisor == 0) {
        return 0; // 除数为0，返回0
    }
    uint64_t remainder = 0;

    // 用于存储商的变量
    uint64_t quotient = 0;

    // 从最高位开始逐步计算商
    for (int i = 63; i >= 0; i--) {
        // 将余数左移1位
        remainder <<= 1;
        // 获取当前位的值
        uint64_t bit = (dividend >> i) & 1;
        // 将当前位的值加到余数的最低位
        remainder |= bit;
        // 如果余数大于等于除数，则减去除数，并将对应位的商置1
        if (remainder >= divisor) {
            remainder -= divisor;
            quotient |= (1ULL << i);
        }
    }
    return remainder; // 返回商
}


/* 实现六十四位整形的除法 */
uint64_t div_u64_rem(uint64_t dividend, uint32_t divisor, uint32_t* remainder) {
    // 定义一个联合体以将64位被除数视为两个32位部分
    union {
        uint64_t v64;
        uint32_t v32[2];
    } d = { dividend };
    uint32_t upper;

    // 存储被除数的高32位
    upper = d.v32[1];
    d.v32[1] = 0;

    // 检查被除数的高部分是否大于或等于除数
    if (upper >= divisor) {
        // 计算高部分的商和余数
        d.v32[1] = upper / divisor;
        upper %= divisor;
    }

    // 使用除数对低32位进行除法运算
    asm("divl %2" : "=a" (d.v32[0]), "=d" (*remainder) :
        "rm" (divisor), "0" (d.v32[0]), "1" (upper));

    // 返回组合的64位结果
    return d.v64;
}
