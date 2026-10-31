#ifndef VOXI_DIAG_H
#define VOXI_DIAG_H

#include <voxi/types.h>

void diag_memmap(void);
void diag_peek(uint64_t addr);
void diag_poke(uint64_t addr, uint8_t val);
void diag_hexdump(uint64_t addr, uint64_t len);
void diag_inport(uint16_t port);
void diag_outport(uint16_t port, uint8_t val);
void diag_cpuid(void);
void diag_size(void);
void diag_sloc(void);
void diag_switchbench(void);
void diag_syscallbench(void);
void diag_freeze(void);

/* Spawns a ring3 task that deliberately divides by zero, to show the
 * fault-isolation path works for more than just the boot-time demo. */
void diag_crashtest(void);

#endif
