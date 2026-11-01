#ifndef VOXI_PCI_H
#define VOXI_PCI_H

#include <voxi/types.h>

/* PCI config space access via the legacy CONFIG_ADDRESS (0xCF8) /
 * CONFIG_DATA (0xCFC) I/O ports -- works on every PC chipset that has
 * ever existed, no ACPI/MCFG parsing needed. Brute-force scans every
 * bus/device/function and prints whatever answers (vendor id != 0xFFFF). */
void pci_list(void);

#endif
