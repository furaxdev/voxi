# Voxi

A from-scratch x86_64 kernel, built by Furax (FuraxDev). Boots via GRUB2 /
Multiboot2, switches to 64-bit long mode, and drops into an interactive
shell — all written from the ground up with no borrowed kernel code.

## What's in it

- **Boot** (`boot/boot.S`): Multiboot2 header, identity-maps the first 1 GiB
  with 2 MiB pages, enables PAE/long mode, jumps into 64-bit code.
- **IDT/ISR/IRQ** (`kernel/arch/x86_64`): full 256-entry IDT, all 32 CPU
  exceptions handled (prints a diagnostic and halts on an unhandled fault),
  8259 PIC remapped to vectors 32-47.
- **Drivers** (`kernel/drivers`): VGA text console (with scrolling/colors),
  COM1 serial (mirrors everything `kprintf` writes, handy for debugging in
  QEMU with `-serial stdio`), PIT timer (100 Hz tick), PS/2 keyboard
  (US layout, shift/capslock, ring buffer).
- **Memory** (`kernel/mm`): physical frame allocator (bitmap, built from the
  Multiboot2 memory map), a first-fit `kmalloc`/`kfree` heap that grows on
  demand.
- **Shell** (`kernel/shell.c`): a tiny interactive shell — `help`, `clear`,
  `echo`, `meminfo`, `uptime`, `about`, `reboot`, `halt`.

## Building and running

Needs: `gcc`, `binutils` (`ld`/`as`), `grub-pc-bin`, `xorriso`, `mtools`,
`qemu-system-x86_64`.

```sh
make          # build build/voxi.bin
make iso      # produce build/voxi.iso (GRUB2 bootable)
make run      # boot it in QEMU (graphical window + serial on stdio)
```

For a headless box:

```sh
qemu-system-x86_64 -cdrom build/voxi.iso -serial stdio -display none -m 512M
```

## Current limits / what's next

This is a real, booting kernel, not a toy stub — but it's still early-stage
next to Linux or Windows. Honest next steps, roughly in order of how much
they'd unlock:

1. **Virtual memory manager**: only identity-mapped 1 GiB exists right now;
   add a real `map_page`/`unmap_page` and go higher-half.
2. **Multitasking**: a scheduler, task structs, context switching (the IDT
   and timer tick are already there to drive it).
3. **User mode**: TSS, ring 3, syscalls (`int 0x80` or `syscall`), separate
   kernel/user address spaces.
4. **Filesystem**: a VFS layer, then something concrete (initrd, FAT, or a
   custom FS) — needs a disk driver (ATA/AHCI) first.
5. **More drivers**: proper framebuffer graphics, PCI enumeration, mouse.

The code is organized (`kernel/arch`, `kernel/drivers`, `kernel/mm`,
`kernel/lib`) so each of these slots in without a rewrite.
