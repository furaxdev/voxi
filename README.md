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

`make run`/`make run-nographic` also create `build/disk.img` (a blank
1MiB disk, only once — it's not regenerated on later builds, since the
whole point of `kernel/fs/sfs.c` is that files written to it survive)
and attach it as a real IDE drive. To start over with a blank disk,
just delete it: `rm build/disk.img`.

For a headless box:

```sh
qemu-system-x86_64 -cdrom build/voxi.iso \
    -drive file=build/disk.img,format=raw,if=ide,index=0,media=disk \
    -serial stdio -display none -m 512M
```

## Running a separate program: `run hello`

`run hello` loads `hello` — a tiny standalone ELF64 binary
(`userland/hello.S`, baked into the kernel image at build time via
`kernel/fs/ramfs_data.S` — the real on-disk filesystem is a later
section) — and runs it as a genuinely isolated process:

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

## fork(), exec(), and wait() are real now: `run forkdemo`

`forkdemo` (`userland/fork_demo.c` — the first actual *C* program on
Voxi, compiled against nothing but seven raw syscalls, see
`userland/syscalls.S` and `userland/crt0.S`) does exactly what its name
says: it calls `fork()`, the child `exec()`s into `hello`, the parent
`wait()`s for it, and both report the real exit code:

```
voxi> run forkdemo
[fork_demo] pid 6
[fork_demo] about to fork()...
[kernel] fork(): task 6 -> child task 7 (own copied address space)
[fork_demo] I am the parent. Waiting for my child to finish...
[fork_demo] I am the child. Replacing myself with 'hello' via exec()...
[kernel] exec(): task 7 now running 'hello'
[hello] I am a separate ELF64 binary, loaded from ramfs,
[hello] running in my own page tables with my own physical memory.
[kernel] task 7 exited via syscall (code 0)
[fork_demo] child exited cleanly. fork/exec/wait all work.
[kernel] task 6 exited via syscall (code 0)
```

How each one actually works:

- **`fork()`** (`kernel/syscall.c`, `SYS_FORK`): builds the child a
  fresh address space, then walks the parent's page table copying every
  present page into a freshly allocated physical frame at the same
  virtual address — a real copy, not a shared one. The child's first
  instruction is the *same* `int 0x80`'s return point as the parent's,
  reached through a new `enter_usermode_child()` (`usermode.S`) that's
  identical to the usual ring-3 entry except it zeroes `rax` first:
  `iretq` never touches general-purpose registers, so without that,
  the child would resume with whatever garbage was last in the
  physical `rax` instead of the `0` that's the entire signal C code
  uses to tell "I'm the child" from "I'm the parent, and this is my
  child's pid" (which the parent gets normally, since the kernel just
  writes it into `regs->rax` before an ordinary syscall return).
- **`exec()`** (`SYS_EXEC`): frees every page in the caller's own
  address space (`vmm_reset_user_window()`), loads a new ELF into the
  same now-empty space, and rewrites the pending syscall-return
  frame's `rip` (and, since crossing from kernel back to ring 3 always
  carries a saved user `rsp`/`ss` even though `registers_t` doesn't
  name them, see `registers_tail()` in `idt.h`) to point at the new
  program's entry and a fresh stack. The *same* task keeps running —
  it just isn't running the old program anymore, same as real `exec()`.
- **`wait()`** (`SYS_WAIT`): loops `yield()`ing until
  `scheduler_reap_child()` finds a dead, not-yet-reaped task whose
  `parent_id` matches, then returns its exit code.

Caught along the way: the exec() name string lives in the caller's own
user memory, which `vmm_reset_user_window()` immediately frees — reading
it *after* that reset (rather than copying it to a kernel buffer first)
silently read whatever the new program's memory happened to contain
instead, logging the wrong filename even though the load itself still
worked by coincidence.

## Pipes and signals: `run pipedemo`, `run spin` + `kill`

`pipedemo` (`userland/pipe_demo.c`) does real IPC: `pipe()`, then
`fork()`, then the child writes a message into the write end and
exits while the parent reads it back out the read end — the same
mechanism a shell uses for `cmd1 | cmd2`:

```
voxi> run pipedemo
run: spawned 'pipedemo' as task #6, entry=0x0000000040000000, own page tables (cr3=0x000000000052a000)
[kernel] fork(): task 6 -> child task 7 (own copied address space)
[kernel] task 7 exited via syscall (code 0)
[pipe_demo] parent received 50 bytes: [pipe_demo] hello through a pipe, from the child!
[kernel] task 6 exited via syscall (code 0)
```

A pipe (`kernel/syscall.c`) is a small kmalloc'd ring buffer. `SYS_PIPE`
hands back a read fd and a write fd in the caller's own fd table (new
in `task_t`: every task now has one, `fds[FD_MAX]`, defaulting to
console-in/console-out/console-out for 0/1/2). `SYS_WRITE`/`SYS_READ`
block (`yield()`-looping) when the buffer is full or empty, and return
EOF/short once the other end is closed — closing itself is reference
counted per end (`read_refs`/`write_refs`), which turned out to matter
immediately: `fork()` copies the whole fd table, so after a fork both
parent and child hold their own fd for the *same* write end. The first
version let the parent's own `close()` mark the whole pipe
write-closed — immediately, before the child had written anything —
because it didn't know the child still held a copy. `pipedemo` read 0
bytes every time until each end's close only takes effect once its
refcount actually reaches zero.

Signals are intentionally minimal: `SYS_KILL(pid, sig)` just sets a
`pending_signal` field on the target task. There's no asynchronous
delivery — it takes effect the next time the scheduler is about to
resume that task (`scheduler_switch()` checks for it right there,
before actually switching in), which in practice is "soon" since
everything here eventually passes back through the scheduler via
either `yield()` or the timer. `run spin` (`userland/spin.c`) loops
forever specifically to give `kill PID` something to aim at:

```
voxi> run spin
[spin] pid 8
[spin] looping forever -- try 'kill <that pid>' from the shell
voxi> kill 8
sent SIGKILL to task 8 (takes effect next time it's scheduled)
[kernel] task 8 killed by signal 9
```

## A real filesystem on a real disk: `diskls`, `diskwrite`, `diskread`

Everything above (`ls`, `run hello`, ...) reads from the ramfs — demo
binaries embedded in the kernel image at build time, gone the moment
you'd change them without recompiling. This is the real thing: an ATA
PIO driver (`kernel/drivers/ata.c`) talking to an actual block device
(`-drive file=build/disk.img,format=raw,if=ide`, wired up by `make
run`), and a from-scratch on-disk format (`kernel/fs/sfs.c`, "SFS" —
not FAT, not ext2, just complex enough to need a real superblock, a
real file table, and real contiguous sector allocation):

```
LBA 0:   superblock (magic "SFS1", file count, next free LBA)
LBA 1:   file table (16 entries x 32 bytes: name, start LBA, size)
LBA 2+:  file data, contiguous per file, no fragmentation support
```

First boot against a blank disk formats it and seeds one file,
`hello_disk` (literally the `hello` binary, copied onto the disk —
hence it still prints "loaded from ramfs", since the binary itself
has no idea where it was read from this time):

```
[sfs] formatted a blank disk (no 'SFS1' magic found)
[sfs] wrote 'hello_disk': 984 bytes at lba 2
```

And then, the part that actually matters — persistence that survives
more than an in-VM `reboot`, verified by killing the QEMU *process*
entirely and starting a fresh one against the same `build/disk.img`:

```
voxi> diskwrite greeting hey furax ca banger
[sfs] wrote 'greeting': 19 bytes at lba 4
voxi> diskread greeting
hey furax ca banger
```
*(qemu killed, a brand new qemu-system-x86_64 started against the same disk.img)*
```
[sfs] mounted existing filesystem: 2 file(s)
voxi> diskls
hello_disk          984 bytes  (lba 2)
greeting             19 bytes  (lba 4)
voxi> diskread greeting
hey furax ca banger
```

`run NAME` and `exec()` both go through a small VFS dispatcher
(`kernel/fs/vfs.c`) now: ramfs first, then SFS — so `run hello_disk`
loads and runs an ELF binary read sector-by-sector off the disk, in
its own address space, exactly like the ramfs-backed ones.

## Can it run bash or zsh?

No, and this is worth being direct about rather than letting the
question hang: not the real binaries, and not even their source code
without substantial further work. Every item in the last three
sections *was* genuinely part of the "multi-week" estimate from
earlier versions of this section — turns out each one fits in an
afternoon once the address-space and scheduler work underneath it
already exists to build on. What's still actually missing:

- **A real filesystem on real storage.** There's a ramfs with four
  hand-written demo files; there's no disk driver, no directory
  structure, nothing resembling `/bin` or `/etc`. This blocks loading
  anything you didn't bake into the kernel image at compile time.
- **A libc.** Bash and zsh are written against glibc or a similar
  POSIX libc — `malloc`, `printf`, `readline`, locale support, all of
  it. `userland/ulib.h` has none of that; it hand-rolls a 2-digit
  integer-to-string conversion because there's no `itoa`, let alone
  `printf`. Getting to "enough of POSIX that bash's own source
  compiles" is a project in its own right.
- **Job control and `SIGCHLD`.** Signal delivery exists now, but only
  `SIGKILL`-style "stop this task"; there's no signal *handling* (a
  process catching its own signal and running a handler), no process
  groups, no foreground/background, nothing `Ctrl-C`/`Ctrl-Z` or a
  shell's job table needs.
- **A tty layer** with `termios` and the line discipline `readline`
  expects. Voxi's "terminal" is a VGA text buffer and a keyboard IRQ
  (with a blocking `SYS_READ` for user processes); there's no pty, no
  `SIGWINCH`, nothing a real shell's interactive line editing assumes
  exists.

The realistic path from here: filesystem, then a libc port, then
job control/signal handling, landing on something dash/ash-sized
rather than bash or zsh specifically — not the stock Linux binary,
which would need Voxi to *be* Linux-ABI-compatible to run at all.

## Current limits / what's next

This is a real, booting kernel, not a toy stub — but it's still early-stage
next to Linux or Windows. Honest next steps, roughly in order of how much
they'd unlock:

1. **A libc port**: the real prerequisite for running anything written
   against POSIX expectations, bash/zsh included (see above).
2. **A real directory structure + a bigger/better on-disk format**: SFS
   has no subdirectories, no fragmentation support, no deletion-and-reuse
   of freed space, and a flat 16-file table. Fine for proving the
   driver and the concept; a real `/bin`, `/etc`, `/home` needs more.
3. **Signal handling + job control**: delivery exists (`SYS_KILL`), but
   only as "stop this task" -- no handler a process can install for its
   own signals, no process groups, no foreground/background.
4. **A higher-half kernel + general-purpose `map_page`/`unmap_page`**: the
   current per-process window is one fixed-size, fixed-address 2MiB
   region good enough for a demo binary (or two, fork-copied), not a
   general allocator for arbitrary processes with arbitrary memory needs.
5. **More drivers**: proper framebuffer graphics, PCI enumeration, mouse,
   AHCI (the ATA PIO driver above works but is the simplest, slowest
   way to talk to a disk).
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
