#ifndef VOXI_RTC_H
#define VOXI_RTC_H

#include <voxi/types.h>

/* Reads the current wall-clock time of day from the CMOS RTC. Values
 * are plain binary (already converted out of BCD if the RTC is in BCD
 * mode), 24-hour. No date handling -- just hours/minutes/seconds. */
void rtc_read(uint8_t *hours, uint8_t *minutes, uint8_t *seconds);

#endif
