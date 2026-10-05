#ifndef VOXI_PIT_H
#define VOXI_PIT_H

#include <voxi/types.h>

#define PIT_HZ 100

void pit_init(uint32_t frequency_hz);
uint64_t pit_ticks(void);

#endif
