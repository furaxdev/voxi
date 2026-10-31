#ifndef VOXI_SFS_H
#define VOXI_SFS_H

#include <voxi/types.h>

/* "SFS" -- Simple File System. Not FAT, not ext2: a from-scratch,
 * on-disk format just complex enough to need a real superblock, a
 * real file table, and real contiguous block allocation, and no more.
 * No directories, no fragmentation support, no deletion-and-reuse --
 * each file just gets the next run of free sectors, forever.
 *
 * Layout (512-byte sectors):
 *   LBA 0:        superblock (magic, file_count, next_free_lba)
 *   LBA 1:        file table (16 entries x 32 bytes = 512 bytes)
 *   LBA 2..:      file data, contiguous per file
 */

#define SFS_MAX_FILES 16
#define SFS_NAME_MAX  23

/* Reads the superblock; formats (zeroes the table, writes a fresh
 * superblock) if the magic isn't there. Call once, early. */
void sfs_init(void);

/* True if the filesystem was freshly formatted by this sfs_init() call
 * (i.e. the disk was blank/unrecognized) -- used once, at boot, to
 * decide whether to seed a demo file. */
bool sfs_was_just_formatted(void);

void sfs_list(void);

/* Reads a whole file into a freshly kmalloc'd buffer (rounded up to a
 * sector), which the caller must kfree(). Returns false if not found. */
bool sfs_find(const char *name, uint8_t **data_out, size_t *size_out);

/* Creates a new file (fails if the name already exists or the table
 * is full) and writes `data` to disk for it, sector by sector. */
bool sfs_write(const char *name, const uint8_t *data, size_t size);

#endif
