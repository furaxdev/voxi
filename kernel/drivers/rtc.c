#include "rtc.h"
#include <voxi/io.h>

#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

static uint8_t cmos_read(uint8_t reg) {
    outb(CMOS_ADDR, reg);
    return inb(CMOS_DATA);
}

static int update_in_progress(void) {
    return cmos_read(0x0A) & 0x80;
}

static uint8_t bcd_to_bin(uint8_t v) {
    return (uint8_t)((v & 0x0F) + ((v >> 4) * 10));
}

void rtc_read(uint8_t *hours, uint8_t *minutes, uint8_t *seconds) {
    uint8_t sec, min, hour, reg_b;

    /* Avoid sampling mid-update: wait for any in-progress update to
     * finish, then read everything "atomically enough" by re-reading
     * seconds and retrying once if it changed underneath us. */
    while (update_in_progress());
    sec = cmos_read(0x00);
    min = cmos_read(0x02);
    hour = cmos_read(0x04);
    if (cmos_read(0x00) != sec) {
        while (update_in_progress());
        sec = cmos_read(0x00);
        min = cmos_read(0x02);
        hour = cmos_read(0x04);
    }

    reg_b = cmos_read(0x0B);
    int pm = hour & 0x80;
    hour &= 0x7F;
    if (!(reg_b & 0x04)) { /* BCD mode (the common default) */
        sec = bcd_to_bin(sec);
        min = bcd_to_bin(min);
        hour = bcd_to_bin(hour);
    }
    if (!(reg_b & 0x02) && pm) { /* 12-hour mode, PM bit set */
        hour = (uint8_t)((hour % 12) + 12);
    }

    *hours = hour;
    *minutes = min;
    *seconds = sec;
}
