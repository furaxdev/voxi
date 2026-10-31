#ifndef VOXI_ELF_H
#define VOXI_ELF_H

#include <voxi/types.h>
#include "mm/vmm.h"

/* Parses a static ELF64 executable and maps its PT_LOAD segments into
 * `as` (fresh physical frames, copied from `data`, zero-extended for
 * memsz > filesz). Only supports segments inside the one 2MiB window
 * vmm.c carves out per process -- plenty for a tiny demo binary.
 * On success, *entry_out is the ELF's entry point (a USER_BASE-relative
 * virtual address, valid once `as` is active). */
bool elf_load(const uint8_t *data, size_t size, address_space_t *as, uint64_t *entry_out);

#endif
