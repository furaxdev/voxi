#ifndef VOXI_GDT_H
#define VOXI_GDT_H

#include <voxi/types.h>

#define KERNEL_CODE_SEL 0x08
#define KERNEL_DATA_SEL 0x10
#define USER_DATA_SEL   0x1B /* index 3, RPL 3 */
#define USER_CODE_SEL   0x23 /* index 4, RPL 3 */

void gdt_init(void);

/* Updates the TSS's rsp0: the stack the CPU loads automatically whenever
 * an interrupt or exception crosses from ring3 into ring0. Called by the
 * scheduler every time it switches to a new current task. */
void tss_set_rsp0(uint64_t rsp0);

#endif
