#ifndef VOXI_RAMFS_H
#define VOXI_RAMFS_H

#include <voxi/types.h>

/* Looks up a file embedded into the kernel binary at build time (see
 * ramfs_data.S). There's no real storage here -- these bytes have
 * been part of the kernel image since link time -- but it's enough to
 * prove the loader/process path end to end before a real disk driver
 * and filesystem exist. */
bool ramfs_find(const char *name, const uint8_t **data, size_t *size);

/* Lists what's available, for the shell's `ls` command. */
void ramfs_list(void);

#endif
