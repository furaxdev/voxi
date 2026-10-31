#include "vfs.h"
#include "ramfs.h"
#include "sfs.h"

bool vfs_find(const char *name, const uint8_t **data, size_t *size, bool *needs_free) {
    if (ramfs_find(name, data, size)) {
        *needs_free = false;
        return true;
    }

    uint8_t *disk_data;
    if (sfs_find(name, &disk_data, size)) {
        *data = disk_data;
        *needs_free = true;
        return true;
    }

    return false;
}
