#include "ata.h"
#include <voxi/io.h>

#define ATA_IO_BASE 0x1F0
#define ATA_DATA    (ATA_IO_BASE + 0)
#define ATA_SECCNT  (ATA_IO_BASE + 2)
#define ATA_LBA_LO  (ATA_IO_BASE + 3)
#define ATA_LBA_MID (ATA_IO_BASE + 4)
#define ATA_LBA_HI  (ATA_IO_BASE + 5)
#define ATA_DRIVE   (ATA_IO_BASE + 6)
#define ATA_STATUS  (ATA_IO_BASE + 7)
#define ATA_COMMAND (ATA_IO_BASE + 7)

#define ATA_CMD_READ  0x20
#define ATA_CMD_WRITE 0x30
#define ATA_CMD_FLUSH 0xE7

#define ATA_SR_BSY 0x80
#define ATA_SR_DRQ 0x08
#define ATA_SR_ERR 0x01

static void ata_wait_not_busy(void) {
    while (inb(ATA_STATUS) & ATA_SR_BSY);
}

static bool ata_wait_drq(void) {
    uint8_t status;
    do {
        status = inb(ATA_STATUS);
        if (status & ATA_SR_ERR) return false;
    } while (!(status & ATA_SR_DRQ));
    return true;
}

static void ata_select_lba(uint32_t lba) {
    outb(ATA_DRIVE, (uint8_t)(0xE0 | ((lba >> 24) & 0x0F))); /* master, LBA mode */
    outb(ATA_SECCNT, 1);
    outb(ATA_LBA_LO, (uint8_t)(lba & 0xFF));
    outb(ATA_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_LBA_HI, (uint8_t)((lba >> 16) & 0xFF));
}

bool ata_read_sector(uint32_t lba, uint8_t *buf512) {
    ata_wait_not_busy();
    ata_select_lba(lba);
    outb(ATA_COMMAND, ATA_CMD_READ);
    ata_wait_not_busy();
    if (!ata_wait_drq()) return false;

    for (int i = 0; i < ATA_SECTOR_SIZE / 2; i++) {
        uint16_t word = inw(ATA_DATA);
        buf512[i * 2] = (uint8_t)(word & 0xFF);
        buf512[i * 2 + 1] = (uint8_t)((word >> 8) & 0xFF);
    }
    return true;
}

bool ata_write_sector(uint32_t lba, const uint8_t *buf512) {
    ata_wait_not_busy();
    ata_select_lba(lba);
    outb(ATA_COMMAND, ATA_CMD_WRITE);
    ata_wait_not_busy();
    if (!ata_wait_drq()) return false;

    for (int i = 0; i < ATA_SECTOR_SIZE / 2; i++) {
        uint16_t word = (uint16_t)buf512[i * 2] | ((uint16_t)buf512[i * 2 + 1] << 8);
        outw(ATA_DATA, word);
    }

    outb(ATA_COMMAND, ATA_CMD_FLUSH);
    ata_wait_not_busy();
    return true;
}
