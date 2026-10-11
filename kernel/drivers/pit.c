#include "pit.h"
#include <voxi/io.h>
#include "../arch/x86_64/idt.h"
#include "../scheduler.h"

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43
#define PIT_BASE_FREQ 1193182

static volatile uint64_t ticks = 0;

static void pit_handler(registers_t *regs) {
    (void)regs;
    ticks++;
    if (scheduler_started()) scheduler_switch();
}

void pit_init(uint32_t frequency_hz) {
    register_interrupt_handler(32, pit_handler);

    uint32_t divisor = PIT_BASE_FREQ / frequency_hz;
    if (divisor == 0) divisor = 1;
    if (divisor > 0xFFFF) divisor = 0xFFFF;

    outb(PIT_COMMAND, 0x36); /* channel 0, lobyte/hibyte, mode 3 (square wave) */
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));
}

uint64_t pit_ticks(void) {
    return ticks;
}
