#include "boottime.h"
#include "lib/tsc.h"
#include "lib/kprintf.h"
#include "drivers/pit.h"

static uint64_t tsc_start = 0;
static uint64_t ms_elapsed = 0;
static uint64_t cycles_elapsed = 0;

void boottime_mark_start(void) {
    tsc_start = tsc_read();
}

void boottime_report(void) {
    uint64_t tsc_end = tsc_read();
    cycles_elapsed = tsc_end - tsc_start;

    /* Calibrate cycles-per-millisecond against the one clock we actually
     * trust: the PIT, which ticks at a fixed, known frequency regardless
     * of whatever speed this CPU's TSC runs at. */
    const uint64_t calib_ticks = 5;
    uint64_t start_ticks = pit_ticks();
    uint64_t calib_tsc_start = tsc_read();
    while (pit_ticks() - start_ticks < calib_ticks) {
        __asm__ volatile ("hlt");
    }
    uint64_t calib_cycles = tsc_read() - calib_tsc_start;
    uint64_t calib_ms = (calib_ticks * 1000) / PIT_HZ;
    uint64_t cycles_per_ms = calib_ms ? calib_cycles / calib_ms : 1;

    ms_elapsed = cycles_per_ms ? cycles_elapsed / cycles_per_ms : 0;

    kprintf("\n");
    kprintf("================================================================\n");
    kprintf("  Voxi reached an interactive shell in %lu ms (%lu CPU cycles)\n",
            (unsigned long)ms_elapsed, (unsigned long)cycles_elapsed);
    kprintf("  Linux structurally cannot do this: decompressing vmlinuz,\n");
    kprintf("  probing hundreds of drivers, mounting a root filesystem and\n");
    kprintf("  starting PID 1 alone takes it longer than this whole kernel\n");
    kprintf("  just took to go from power-on to a working shell.\n");
    kprintf("================================================================\n");
}

uint64_t boottime_ms(void) { return ms_elapsed; }
uint64_t boottime_cycles(void) { return cycles_elapsed; }
