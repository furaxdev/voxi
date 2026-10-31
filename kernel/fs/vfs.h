#ifndef VOXI_VFS_H
#define VOXI_VFS_H

#include <voxi/types.h>

/* Looks a name up in the ramfs first (the kernel-image-embedded demo
 * binaries), then the real on-disk filesystem. *needs_free tells the
 * caller whether *data was kmalloc'd for them (disk-backed: yes,
 * ramfs: no, it's a pointer straight into the kernel image) --
 * callers must kfree() it themselves when true, after they're done. */
bool vfs_find(const char *name, const uint8_t **data, size_t *size, bool *needs_free);

#endif
