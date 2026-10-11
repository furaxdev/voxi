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
  `echo`, `meminfo`, `uptime`, `about`, `heartbeat`, `reboot`, `halt`.
- **Preemptive multitasking** (`kernel/task.c`, `kernel/scheduler.c`,
  `kernel/arch/x86_64/context.S`): kernel tasks, each with its own stack,
  round-robin scheduled. `switch_to()` swaps the callee-saved registers
  and `rsp` between tasks and ends in a plain `ret` (deliberately *not*
  an `iretq` — see the note below). `yield()` calls it voluntarily; the
  PIT timer handler calls the exact same function every tick, so a task
  that never yields still gets preempted. `switch_to` unconditionally
  executes `sti` on the way into whichever task it resumes, since that
  task might be resuming mid-interrupt (`IF` won't be restored until its
  own pending `iretq`, which could be a while) or might be brand new and
  have never enabled interrupts at all — miss that and the first
  never-yielding task to run permanently wedges the timer. `kernel_main`
  spawns three demo tasks that interleave their output to prove the
  context switch works, then a persistent `heartbeat` task that *never*
  yields at all, to prove preemption (not voluntary cooperation) is what
  keeps the shell responsive.
- **Ring 3 + syscalls** (`kernel/arch/x86_64/gdt.c`,
  `kernel/arch/x86_64/usermode.S`, `kernel/syscall.c`): a real GDT with
  user code/data segments and a 64-bit TSS (`rsp0` is repointed to the
  current task's own kernel stack on every switch, so a ring3→ring0 trap
  always lands somewhere safe). `enter_usermode()` drops to CPL3 via a
  hand-built `iretq` frame. User code calls into the kernel with
  `int 0x80` (`SYS_WRITE`, `SYS_EXIT`). The demo user task writes a
  couple of lines, then deliberately executes `cli` — a privileged
  instruction — to prove CPL3 is real: it faults, and the kernel's
  exception handler catches it, kills just that task, and keeps running
  (checked by looking at the faulting frame's `cs`). There's no real
  memory isolation yet (same page tables, just marked user-accessible) —
  that's the next layer, see below.

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

1. **Real memory isolation**: ring3 works, but every task shares the same
   page tables (the whole 1 GiB identity map is just marked
   user-accessible). A real process needs its own address space: a
   per-task `CR3`, a proper `map_page`/`unmap_page`, and a higher-half
   kernel so user and kernel addresses don't collide.
2. **A process loader**: the ring3 demo is a C function compiled straight
   into the kernel binary. Loading and running a *separate* ELF binary
   needs the VFS/filesystem below, plus `execve`-style setup.
3. **Filesystem**: a VFS layer, then something concrete (initrd, FAT, or a
   custom FS) — needs a disk driver (ATA/AHCI) first.
4. **More drivers**: proper framebuffer graphics, PCI enumeration, mouse.
5. **SMP**: everything here assumes one CPU core.

The code is organized (`kernel/arch`, `kernel/drivers`, `kernel/mm`,
`kernel/lib`) so each of these slots in without a rewrite.

### A debugging note worth keeping

The first version of the preemptive scheduler used `iretq` to restore a
saved register frame for *every* task switch (not just real interrupt
returns), on the theory that it would generalize cleanly to ring3 tasks
too. It reliably raised a spurious `#GP` under QEMU/TCG the moment a
switch happened from somewhere other than immediately after the
hardware's own interrupt-entry push — i.e. exactly the case a scheduler
needs. Dropping back to the classic technique (save/restore the
callee-saved registers and `rsp`, return with a plain `ret`) fixed it
outright. `iretq` is still used, just only for the paths real hardware
already exercises constantly: the normal interrupt-return epilogue, and
the one-way ring0→ring3 jump in `enter_usermode`.
