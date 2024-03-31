#ifndef __DEVICE_CMOS_H
#define __DEVICE_CMOS_H

#include "io.h"
#include "stdin.h"

#define BCD_TO_BIN(val) ((val) = ((val) & 15) + ((val) >> 4) * 10)

struct tm {
    uint32_t tm_sec;    /* 秒 [0, 59] */
    uint32_t tm_min;    /* 分 [0, 59] */
    uint32_t tm_hour;   /* 时 [0, 23] */
    uint32_t tm_mday;   /* 一个月中的天 [1, 31] */
    uint32_t tm_mon;    /* 月份 [0, 11] */
    uint32_t tm_year;   /* 年份 - 1900 */
}__attribute__((packed));

struct timespec {
    uint64_t   tv_sec;  // 秒数
    uint64_t   tv_msec; // 毫秒数
};

// COMS中读取的开始时间
extern struct tm time;
// UNIX时间戳结构体
extern struct timespec times;


// 时间初始化
void time_init(void);
// 将年月日时分秒转换为 UNIX 时间戳
uint64_t datetime_to_timestamp(struct tm* tm);
/* 将 UNIX 时间戳转换为年月日时分秒 */
void timestamp_to_datetime(uint64_t timestamp, struct tm* datetime);
/* 获取当前的 unix 时间戳 */ 
uint64_t get_time(void);


#endif