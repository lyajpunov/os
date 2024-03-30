#ifndef __DEVICE_CMOS_H
#define __DEVICE_CMOS_H

#include "io.h"
#include "stdin.h"

#define BCD_TO_BIN(val) ((val) = ((val) & 15) + ((val) >> 4) * 10)

struct tm {
    int tm_sec;    /* 秒 [0, 59] */
    int tm_min;    /* 分 [0, 59] */
    int tm_hour;   /* 时 [0, 23] */
    int tm_mday;   /* 一个月中的天 [1, 31] */
    int tm_mon;    /* 月份 [0, 11] */
    int tm_year;   /* 年份 - 1900 */
    int tm_wday;   /* 一周中的天 [0, 6]，周日为 0 */
    int tm_yday;   /* 一年中的天 [0, 365] */
    int tm_isdst;  /* 夏令时标志（非0表示启用夏令时）*/
}__attribute__((packed));

void time_init(void);

#endif