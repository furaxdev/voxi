#include "sfs.h"
#include "../drivers/ata.h"
#include "../mm/heap.h"
#include "../lib/string.h"
#include "../lib/kprintf.h"

#define SFS_SUPER_LBA 0
#define SFS_TABLE_LBA 1
#define SFS_DATA_START_LBA 2

typedef struct {
    char magic[4]; /* "SFS1" */
    uint32_t file_count;
    uint32_t next_free_lba;
} __attribute__((packed)) sfs_superblock_t;

typedef struct {
    char name[SFS_NAME_MAX + 1];
    uint32_t start_lba;
    uint32_t size_bytes;
} __attribute__((packed)) sfs_entry_t;

static sfs_superblock_t super;
static sfs_entry_t table[SFS_MAX_FILES];
static bool just_formatted = false;

static void load_table(void) {
    uint8_t buf[ATA_SECTOR_SIZE];
    ata_read_sector(SFS_TABLE_LBA, buf);
    memcpy(table, buf, sizeof(table));
}

static void save_table(void) {
    uint8_t buf[ATA_SECTOR_SIZE];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, table, sizeof(table));
    ata_write_sector(SFS_TABLE_LBA, buf);
}

static void save_super(void) {
    uint8_t buf[ATA_SECTOR_SIZE];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, &super, sizeof(super));
    ata_write_sector(SFS_SUPER_LBA, buf);
}

static void format(void) {
    memcpy(super.magic, "SFS1", 4);
    super.file_count = 0;
    super.next_free_lba = SFS_DATA_START_LBA;
    memset(table, 0, sizeof(table));
    save_super();
    save_table();
    just_formatted = true;
    kprintf("[sfs] formatted a blank disk (no 'SFS1' magic found)\n");
}

void sfs_init(void) {
    uint8_t buf[ATA_SECTOR_SIZE];
    if (!ata_read_sector(SFS_SUPER_LBA, buf)) {
        kprintf("[sfs] no disk found (ATA read failed) -- disk commands won't work\n");
        return;
    }
    memcpy(&super, buf, sizeof(super));

    if (memcmp(super.magic, "SFS1", 4) != 0) {
        format();
    } else {
        load_table();
        kprintf("[sfs] mounted existing filesystem: %lu file(s)\n", (unsigned long)super.file_count);
    }
}

bool sfs_was_just_formatted(void) {
    return just_formatted;
}

void sfs_list(void) {
    if (super.file_count == 0) {
        kprintf("(empty)\n");
        return;
    }
    for (int i = 0; i < SFS_MAX_FILES; i++) {
        if (table[i].name[0] == '\0') continue;
        kprintf("%-16s %6lu bytes  (lba %lu)\n", table[i].name,
                (unsigned long)table[i].size_bytes, (unsigned long)table[i].start_lba);
    }
}

static sfs_entry_t *find_entry(const char *name) {
    for (int i = 0; i < SFS_MAX_FILES; i++) {
        if (table[i].name[0] != '\0' && strcmp(table[i].name, name) == 0) return &table[i];
    }
    return NULL;
}

bool sfs_find(const char *name, uint8_t **data_out, size_t *size_out) {
    sfs_entry_t *e = find_entry(name);
    if (!e) return false;

    uint32_t sectors = (e->size_bytes + ATA_SECTOR_SIZE - 1) / ATA_SECTOR_SIZE;
    uint8_t *buf = kmalloc(sectors * ATA_SECTOR_SIZE);
    if (!buf) return false;

    for (uint32_t i = 0; i < sectors; i++) {
        if (!ata_read_sector(e->start_lba + i, buf + i * ATA_SECTOR_SIZE)) {
            kfree(buf);
            return false;
        }
    }

    *data_out = buf;
    *size_out = e->size_bytes;
    return true;
}

bool sfs_write(const char *name, const uint8_t *data, size_t size) {
    if (find_entry(name)) {
        kprintf("[sfs] '%s' already exists\n", name);
        return false;
    }

    int slot = -1;
    for (int i = 0; i < SFS_MAX_FILES; i++) {
        if (table[i].name[0] == '\0') { slot = i; break; }
    }
    if (slot < 0) {
        kprintf("[sfs] file table is full\n");
        return false;
    }

    uint32_t sectors = (uint32_t)((size + ATA_SECTOR_SIZE - 1) / ATA_SECTOR_SIZE);
    uint32_t start = super.next_free_lba;

    uint8_t sector_buf[ATA_SECTOR_SIZE];
    for (uint32_t i = 0; i < sectors; i++) {
        size_t offset = (size_t)i * ATA_SECTOR_SIZE;
        size_t chunk = size - offset;
        if (chunk > ATA_SECTOR_SIZE) chunk = ATA_SECTOR_SIZE;

        memset(sector_buf, 0, sizeof(sector_buf));
        memcpy(sector_buf, data + offset, chunk);

        if (!ata_write_sector(start + i, sector_buf)) {
            kprintf("[sfs] write failed at sector %lu\n", (unsigned long)(start + i));
            return false;
        }
    }

    strncpy(table[slot].name, name, SFS_NAME_MAX);
    table[slot].name[SFS_NAME_MAX] = '\0';
    table[slot].start_lba = start;
    table[slot].size_bytes = (uint32_t)size;

    super.file_count++;
    super.next_free_lba = start + sectors;

    save_table();
    save_super();

    kprintf("[sfs] wrote '%s': %lu bytes at lba %lu\n", name, (unsigned long)size, (unsigned long)start);
    return true;
}
