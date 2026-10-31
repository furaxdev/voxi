#ifndef VOXI_BOOTTIME_H
#define VOXI_BOOTTIME_H

#include <voxi/types.h>

/* Call once, as the very first thing in kernel_main: snapshots the TSC. */
void boottime_mark_start(void);

/* Call once interrupts + PIT are up: busy-waits a few PIT ticks to
 * calibrate cycles-per-millisecond against the hardware timer, then
 * reports (and prints) how long it took from boottime_mark_start() to
 * right now, in milliseconds and in raw CPU cycles. */
void boottime_report(void);

/* Microseconds from boottime_mark_start() to the boottime_report() call
 * -- kept around so the shell can brag about it later ("about" command). */
uint64_t boottime_ms(void);
uint64_t boottime_cycles(void);

#endif
