#include "pci.h"
#include <voxi/io.h>
#include "../lib/kprintf.h"

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

static uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)1 << 31 |
                        (uint32_t)bus << 16 |
                        (uint32_t)slot << 11 |
                        (uint32_t)func << 8 |
                        (uint32_t)(offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

static const char *pci_class_name(uint8_t class_code, uint8_t subclass) {
    switch (class_code) {
        case 0x00: return "unclassified";
        case 0x01: return "mass storage controller";
        case 0x02: return "network controller";
        case 0x03: return "display controller";
        case 0x04: return "multimedia controller";
        case 0x05: return "memory controller";
        case 0x06:
            switch (subclass) {
                case 0x00: return "host bridge";
                case 0x01: return "ISA bridge";
                case 0x80: return "other bridge";
                default:   return "bridge";
            }
        case 0x07: return "communication controller";
        case 0x08: return "system peripheral";
        case 0x09: return "input device controller";
        case 0x0C: return "serial bus controller";
        default:   return "unknown";
    }
}

void pci_list(void) {
    int found = 0;
    kprintf("bus:dev.fn vendor:device  class  description\n");
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            for (uint8_t func = 0; func < 8; func++) {
                uint32_t id = pci_config_read32((uint8_t)bus, slot, func, 0x00);
                uint16_t vendor = id & 0xFFFF;
                if (vendor == 0xFFFF) {
                    if (func == 0) break; /* no device at all in this slot */
                    continue;
                }
                uint16_t device = (id >> 16) & 0xFFFF;
                uint32_t classreg = pci_config_read32((uint8_t)bus, slot, func, 0x08);
                uint8_t class_code = (classreg >> 24) & 0xFF;
                uint8_t subclass   = (classreg >> 16) & 0xFF;
                kprintf("%03u:%02u.%u   %04x:%04x    %02x:%02x  %s\n",
                        bus, slot, func, vendor, device, class_code, subclass,
                        pci_class_name(class_code, subclass));
                found++;

                /* bit 7 of the header-type byte (offset 0x0E) means this is
                 * a multi-function device -- only then bother with func>0. */
                uint32_t hdrreg = pci_config_read32((uint8_t)bus, slot, func, 0x0C);
                uint8_t header_type = (hdrreg >> 16) & 0xFF;
                if (func == 0 && !(header_type & 0x80)) break;
            }
        }
    }
    kprintf("%d PCI device(s) found (CONFIG_ADDRESS/CONFIG_DATA brute-force scan).\n", found);
}
