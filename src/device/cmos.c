#include "cmos.h"
#include "print.h"
#include "stdio.h"

struct tm time;

uint8_t CMOS_READ(addr) {
    outb(0x70, (0x80 | (addr)));
    return inb(0x71);
};

void time_init(void) {
    put_str("time_init begin!\n");
    do {
        time.tm_sec = CMOS_READ(0);
        time.tm_min = CMOS_READ(2);
        time.tm_hour = CMOS_READ(4);
        time.tm_mday = CMOS_READ(7);
        time.tm_mon = CMOS_READ(8);
        time.tm_year = CMOS_READ(9);
    } while (time.tm_sec != CMOS_READ(0));
    BCD_TO_BIN(time.tm_sec);
    BCD_TO_BIN(time.tm_min);
    BCD_TO_BIN(time.tm_hour);
    BCD_TO_BIN(time.tm_mday);
    BCD_TO_BIN(time.tm_mon);
    BCD_TO_BIN(time.tm_year);
    printk("%d-%d-%d,%d:%d:%d\n", time.tm_year, time.tm_mon, time.tm_mday, time.tm_hour, time.tm_min, time.tm_sec);
    put_str("time_init end!\n");
}