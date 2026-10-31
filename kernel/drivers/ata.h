#ifndef VOXI_ATA_H
#define VOXI_ATA_H

#include <voxi/types.h>

#define ATA_SECTOR_SIZE 512

/* PIO driver for the primary ATA bus, master drive, 28-bit LBA.
 * No IDENTIFY, no error recovery beyond a single status check -- this
 * is the minimum that actually moves real bytes to and from a real
 * block device, not a production driver. QEMU's `-drive ...,if=ide`
 * is exactly this. */
bool ata_read_sector(uint32_t lba, uint8_t *buf512);
bool ata_write_sector(uint32_t lba, const uint8_t *buf512);

#endif
