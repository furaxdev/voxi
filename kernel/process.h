#ifndef VOXI_PROCESS_H
#define VOXI_PROCESS_H

#include <voxi/types.h>

/* Loads a ramfs file as a real, isolated process: its own page tables
 * (kernel/mm/vmm.c), its own physical frames for code/data/stack
 * (kernel/elf.c), scheduled as a normal task but entering ring3 in
 * that private address space instead of the shared kernel one. Returns
 * false if the file doesn't exist or fails to load. */
bool process_spawn(const char *name);

#endif
