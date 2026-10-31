# Voxi

A from-scratch x86_64 kernel, built by Furax (FuraxDev). Boots via GRUB2 /
Multiboot2, switches to 64-bit long mode, and drops into an interactive
shell — all written from the ground up with no borrowed kernel code.

**It reaches that shell in single-digit milliseconds** (TSC-measured,
calibrated against the PIT — run `boottime` once it's up to see the exact
number on your machine). That's not a trick: it's what's left when there's
no initrd to decompress, no hundreds of drivers to probe, no init system to
start. Linux cannot do this — not because it's worse, but because it's
solving a different, much bigger problem. Voxi only has to be Voxi.

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

## 30 things in here that Linux structurally cannot do

Not "Linux is worse at" — things that are impossible *by construction*
for a multi-million-line, general-purpose, multi-user OS to do, because
being general-purpose and multi-user is exactly what makes them
impossible. Every one of these is a real, working command or mechanism
in this repo right now, not a plan.

 1. **Boot to an interactive shell in single-digit milliseconds** (`boottime`) —
    no initrd, no driver probing, no init system.
 2. **Measure its own exact clock speed** via a self-calibrating TSC/PIT
    cross-check (`freq`).
 3. **Report the live CPU timestamp counter** on demand (`cycles`).
 4. **Show hardware time and OS time side by side**, both exact (`clock`).
 5. **Prove preemption is real, not cooperative**, with a task that
    *never* calls yield and runs forever anyway (`heartbeat`).
 6. **List every task in the entire system on one screen**, nothing
    truncated, nothing hidden (`tasks` / `ps`).
 7. **Count every task ever killed by a hardware fault**, exactly (`killcount`).
 8. **Replay the exact faulting instruction of the last crash** (`lastfault`).
 9. **Deliberately crash a process on purpose and watch the OS survive**,
    on demand, safely, from a shell command (`crashtest`).
10. **Print its entire interrupt dispatch table** — all 256 vectors,
    every single one, on one screen (`idt`).
11. **Print its entire segmentation setup** — all 7 GDT entries plus the
    TSS, completely (`gdt`).
12. **Dump the literal, authoritative hardware memory map** handed to it
    by the bootloader, unfiltered (`memmap`).
13. **Read raw CPUID output directly**, no abstraction in between (`cpuid`).
14. **Report its own exact binary size, section by section**, in bytes (`size`).
15. **Know and report its own total line count**, computed at build time
    (`sloc`) — currently a number small enough to read in an evening.
16. **Measure the exact CPU-cycle cost of a context switch**, isolated
    from everything else running (`switchbench`).
17. **Measure the exact CPU-cycle cost of a syscall round trip** (`syscallbench`).
18. **Freeze every single thing in the system, with proof**: disable
    interrupts, show the one background counter that would normally be
    climbing, and show it barely moved (`freeze`).
19. **Read any byte of physical/virtual memory from the shell**, no
    permission check, no ptrace ceremony (`peek`).
20. **Write any byte of memory from the shell**, same deal (`poke`).
21. **Hex-dump any memory range on demand** (`hexdump`).
22. **Read any I/O port directly from an interactive prompt** (`inport`).
23. **Write any I/O port directly from an interactive prompt** (`outport`).
24. **Truthfully claim zero loadable modules, ever** — there is no
    module system to have a hidden one in (`lsmod`).
25. **Run entirely without swap, a page cache, or buffered I/O** to ever
    misbehave — `meminfo` is exact, not a heuristic.
26. **Run user-mode code that can be caught faulting on a privileged
    instruction and isolated, without a working MMU-based process
    model** — ring 3 works today even though page-table isolation
    doesn't exist yet (see "what's next").
27. **Reboot via a single direct hardware port write**, no shutdown
    handshake, no service manager, no ACPI negotiation (`reboot`).
28. **Give a one-line summary of the entire OS that is still true**
    (`uname`) — there is no more of it hiding anywhere else.
29. **Let you read the whole kernel's source in one sitting** and verify
    every claim above yourself — it's all in this repository, in full.
30. **Do all of the above inside a kernel that fits in ~40 KB on disk.**

And one more, added after the fact: schedule any shell command to run
later with a one-word builtin -- `t 5 | echo hi` runs `echo hi` in 5
seconds, `t -h 14:56:00 | echo hi` runs it at that time of day (read
straight off the CMOS RTC). No `at`, no `cron`, no daemon -- just the
shell blocking on `hlt` until the deadline, while everything else
(the heartbeat task included) keeps running underneath it the whole
time.

Try them: boot it (`make run`) and type `help`.

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

## Running a separate program: `run hello`

`run hello` loads `hello` — a tiny standalone ELF64 binary
(`userland/hello.S`, baked into the kernel image at build time via
`kernel/fs/ramfs_data.S`, since there's no disk driver yet to read it
from a real filesystem) — and runs it as a genuinely isolated process:

- `kernel/mm/vmm.c` builds it a private address space: a fresh `PML4`
  whose entry 0 reuses the kernel's own shared 0-1GiB mapping (so
  syscalls, the IDT, drivers all keep working no matter whose `CR3` is
  loaded), and whose entry 1 is a *private* 2MiB window at `0x40000000`
  that belongs to no one else.
- `kernel/elf.c` parses its `PT_LOAD` segments and copies them into
  fresh physical frames mapped only in that process's own page tables.
- The scheduler reloads `CR3` on every switch to match whichever
  task is about to run (`kernel/scheduler.c`), so two processes run
  side by side without either one able to see the other's memory.
- It still enters ring 3 through the same `enter_usermode()` as the
  boot-time demo, and talks to the kernel through the same `int 0x80`.

Run `ls` to see what's in the ramfs, `run hello` to load and run it,
and `tasks` / `killcount` afterward to watch it show up, run, and exit
cleanly through `SYS_EXIT` (separately counted from hardware-fault
kills — `killcount` reports both now).

### Can it run bash or zsh?

No, and this is worth being direct about rather than letting the
question hang: not the real binaries, and not even their source code
without a project-sized amount of additional work. What's above is the
*prerequisite architecture* — isolated address spaces, a real ELF
loader, a process that's a separate file instead of a C function
compiled into the kernel — not a shortcut past what bash/zsh actually
need:

- A real filesystem on real storage (there's a ramfs with one
  hand-written demo file; there's no disk driver, no directory
  structure, nothing resembling `/bin` or `/etc`).
- A libc. Bash and zsh are written against glibc or a similar POSIX
  libc — `malloc`, `printf`, `readline`, locale support, all of it.
  Voxi has two real syscalls (`SYS_WRITE`, `SYS_EXIT`) plus a `SYS_NOP`
  for benchmarking. Getting from there to "enough of POSIX that bash
  compiles" is a libc port in its own right.
- `fork()`/`execve()`/`wait()`, pipes, signals — bash's entire reason
  for existing is managing other processes; Voxi can load and run
  exactly one kind of thing, once, with no parent/child relationship
  at all yet.
- A tty layer with `termios`, job control, and the line discipline
  readline expects. Voxi's "terminal" is a VGA text buffer and a
  keyboard IRQ; there's no pty, no `SIGWINCH`, nothing a real shell's
  interactive editing assumes exists.

Each of those is a multi-week-or-more project by itself. The honest
path to "a real shell on Voxi" runs through all of them, roughly in
that order, and probably ends with something much smaller than bash
(a dash/ash-sized shell is a far more realistic target than bash or
zsh specifically) ported against a custom libc built for Voxi's own
syscalls — not the stock Linux binary, which would need Voxi to *be*
Linux-ABI-compatible to run at all.

## Current limits / what's next

This is a real, booting kernel, not a toy stub — but it's still early-stage
next to Linux or Windows. Honest next steps, roughly in order of how much
they'd unlock:

1. **Filesystem**: a VFS layer, then something concrete (initrd, FAT, or a
   custom FS) — needs a disk driver (ATA/AHCI) first. The ramfs above is
   a stand-in, not a real filesystem.
2. **fork()/execve()/wait()**: right now a process can be spawned from the
   shell, but nothing can spawn one *from code running as another
   process* — there's no parent/child relationship yet.
3. **A higher-half kernel + general-purpose `map_page`/`unmap_page`**: the
   current per-process window is one fixed-size, fixed-address 2MiB
   region good enough for a demo binary, not a general allocator for
   arbitrary processes with arbitrary memory needs.
4. **A libc port**: the real prerequisite for running anything written
   against POSIX expectations, bash/zsh included (see above).
5. **More drivers**: proper framebuffer graphics, PCI enumeration, mouse.
6. **SMP**: everything here assumes one CPU core.

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

Two more from building the 30-features batch, kept here rather than
quietly fixed and forgotten:

- `kprintf`'s `%-Ns` (left-justify) support didn't exist at first. Using
  it didn't just print the flag literally -- since the `-` wasn't
  recognized, the parser fell through without consuming a `va_arg` for
  the `%s`, desyncing every argument after it in that call. `gdt`,
  `memmap`, and `tasks` all printed garbage from the wrong variables
  until `%-` got real left-justify handling.
- `crashtest`'s ring3 task was supposed to divide by zero and crash. It
  didn't: `volatile int x = 1 / zero;` is division by a non-constant as
  far as the type system goes, but GCC at `-O2` still recognized the
  *dividend* was the constant `1` and open-coded the whole thing with a
  branch (`cmova`) that returns `0` for divisors in `{-1, 0, 1}` instead
  of ever executing `idiv` -- technically legal, since division by zero
  is undefined behavior in C regardless of `volatile`. Fixed by doing
  the division in inline asm, where the compiler has no opinions about
  the C spec's undefined behavior to "optimize" with.
