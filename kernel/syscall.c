#include "syscall.h"
#include "arch/x86_64/idt.h"
#include "scheduler.h"
#include "mm/heap.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "elf.h"
#include "fs/vfs.h"
#include "drivers/keyboard.h"
#include "lib/string.h"
#include "lib/kprintf.h"

#define USER_STACK_PAGE (USER_BASE + 0x1FF000ull)
#define USER_STACK_TOP  (USER_BASE + 0x200000ull)

#define PIPE_BUF_SIZE 256

/* A pipe is a tiny shared ring buffer, kmalloc'd once by SYS_PIPE and
 * referenced by both fd table entries it creates. fork() copies the fd
 * table, not the pipe -- so after a fork, both parent and child hold
 * their own fd pointing at the SAME pipe_t, exactly like real fork().
 * That's why read_refs/write_refs exist: the parent closing "its"
 * write end must not mark the pipe write-closed while the child still
 * holds a copy of that same end and hasn't written yet. Only when the
 * last holder of a given end closes it does that end actually close.
 * Never freed once both ends are gone -- a real kernel would refcount
 * the whole object and free it; this one just leaks it. */
typedef struct {
    uint8_t buf[PIPE_BUF_SIZE];
    uint32_t read_pos, write_pos, count;
    int read_refs, write_refs;
    bool read_closed, write_closed;
} pipe_t;

extern void enter_usermode(void (*entry)(void), uint64_t user_stack_top) __attribute__((noreturn));
extern void enter_usermode_child(void (*entry)(void), uint64_t user_stack_top) __attribute__((noreturn));

typedef struct {
    uint64_t entry_rip;
    uint64_t user_rsp;
} fork_ctx_t;

/* A forked child's first "resumption" isn't a normal task_trampoline
 * call -- there's no entry(arg) to run, just the parent's own ring3
 * code continuing from wherever int 0x80 was executed, with an
 * identical (copied) stack. enter_usermode()'s cross-privilege iretq
 * is the right tool for that same reason it's right for the first
 * entry into any process: it's the proven, hardware-exercised path,
 * never the same-privilege relocated one that QEMU/TCG mishandles. */
static void fork_child_trampoline(void *arg) {
    fork_ctx_t *ctx = (fork_ctx_t *)arg;
    uint64_t rip = ctx->entry_rip;
    uint64_t rsp = ctx->user_rsp;
    kfree(ctx);
    enter_usermode_child((void (*)(void))rip, rsp); /* never returns */
}

static void syscall_handler(registers_t *regs) {
    switch (regs->rax) {
        case SYS_NOP:
            break;

        case SYS_WRITE: {
            int fd = (int)regs->rdi;
            const char *buf = (const char *)regs->rsi;
            uint64_t len = regs->rdx;
            task_t *self = scheduler_current();

            if (fd < 0 || fd >= FD_MAX) { regs->rax = (uint64_t)-1; break; }
            fd_entry_t *e = &self->fds[fd];

            if (e->type == FD_CONSOLE_OUT) {
                for (uint64_t i = 0; i < len; i++) kprintf("%c", buf[i]);
                regs->rax = len;
            } else if (e->type == FD_PIPE_WRITE) {
                pipe_t *p = (pipe_t *)e->obj;
                uint64_t written = 0;
                while (written < len) {
                    if (p->read_closed) break; /* broken pipe: stop, return short count */
                    if (p->count >= PIPE_BUF_SIZE) { yield(); continue; }
                    p->buf[p->write_pos] = (uint8_t)buf[written];
                    p->write_pos = (p->write_pos + 1) % PIPE_BUF_SIZE;
                    p->count++;
                    written++;
                }
                regs->rax = written;
            } else {
                regs->rax = (uint64_t)-1;
            }
            break;
        }

        case SYS_EXIT: {
            task_t *self = scheduler_current();
            self->exit_code = (int)regs->rdi;
            kprintf("[kernel] task %d exited via syscall (code %d)\n", self->id, self->exit_code);
            scheduler_exit_current_and_switch(); /* never returns */
            break;
        }

        case SYS_GETPID:
            regs->rax = (uint64_t)scheduler_current()->id;
            break;

        case SYS_READ: {
            int fd = (int)regs->rdi;
            char *buf = (char *)regs->rsi;
            uint64_t maxlen = regs->rdx;
            task_t *self = scheduler_current();

            if (fd < 0 || fd >= FD_MAX) { regs->rax = (uint64_t)-1; break; }
            fd_entry_t *e = &self->fds[fd];

            if (e->type == FD_CONSOLE_IN) {
                uint64_t n = 0;
                while (n < maxlen) {
                    char c = keyboard_getchar(); /* blocks (sti;hlt) until a key arrives */
                    if (c == '\n') break;
                    if (c == '\b') { if (n > 0) n--; continue; }
                    buf[n++] = c;
                }
                if (n < maxlen) buf[n] = '\0';
                regs->rax = n;
            } else if (e->type == FD_PIPE_READ) {
                pipe_t *p = (pipe_t *)e->obj;
                uint64_t n = 0;
                while (n < maxlen) {
                    if (p->count == 0) {
                        if (p->write_closed) break; /* EOF */
                        yield();
                        continue;
                    }
                    buf[n++] = (char)p->buf[p->read_pos];
                    p->read_pos = (p->read_pos + 1) % PIPE_BUF_SIZE;
                    p->count--;
                }
                regs->rax = n;
            } else {
                regs->rax = (uint64_t)-1;
            }
            break;
        }

        case SYS_PIPE: {
            task_t *self = scheduler_current();
            int rfd = -1, wfd = -1;
            for (int i = 0; i < FD_MAX; i++) {
                if (self->fds[i].type == FD_NONE) { rfd = i; break; }
            }
            for (int i = 0; i < FD_MAX; i++) {
                if (i != rfd && self->fds[i].type == FD_NONE) { wfd = i; break; }
            }
            if (rfd < 0 || wfd < 0) { regs->rax = (uint64_t)-1; break; }

            pipe_t *p = kmalloc(sizeof(pipe_t));
            if (!p) { regs->rax = (uint64_t)-1; break; }
            memset(p, 0, sizeof(*p));
            p->read_refs = 1;
            p->write_refs = 1;

            self->fds[rfd].type = FD_PIPE_READ;
            self->fds[rfd].obj = p;
            self->fds[wfd].type = FD_PIPE_WRITE;
            self->fds[wfd].obj = p;

            int *out = (int *)regs->rdi;
            out[0] = rfd;
            out[1] = wfd;
            regs->rax = 0;
            break;
        }

        case SYS_CLOSE: {
            int fd = (int)regs->rdi;
            task_t *self = scheduler_current();
            if (fd < 0 || fd >= FD_MAX) { regs->rax = (uint64_t)-1; break; }
            fd_entry_t *e = &self->fds[fd];
            if (e->type == FD_PIPE_READ) {
                pipe_t *p = (pipe_t *)e->obj;
                if (--p->read_refs <= 0) p->read_closed = true;
            } else if (e->type == FD_PIPE_WRITE) {
                pipe_t *p = (pipe_t *)e->obj;
                if (--p->write_refs <= 0) p->write_closed = true;
            }
            e->type = FD_NONE;
            e->obj = NULL;
            regs->rax = 0;
            break;
        }

        case SYS_KILL: {
            int pid = (int)regs->rdi;
            int sig = (int)regs->rsi;
            regs->rax = scheduler_signal(pid, sig) ? 0 : (uint64_t)-1;
            break;
        }

        case SYS_FORK: {
            task_t *parent = scheduler_current();
            address_space_t *parent_as = (address_space_t *)parent->as;
            if (!parent_as) {
                kprintf("[kernel] fork() from a non-process task is not supported\n");
                regs->rax = (uint64_t)-1;
                break;
            }

            address_space_t *child_as = vmm_create_address_space();
            if (!child_as) {
                regs->rax = (uint64_t)-1;
                break;
            }

            for (uint64_t va = USER_BASE; va < USER_BASE + 0x200000ull; va += PAGE_SIZE) {
                uint64_t src_phys = vmm_get_user_page(parent_as, va);
                if (!src_phys) continue;
                uint64_t dst_phys = pmm_alloc_frame();
                if (!dst_phys) break;
                memcpy((void *)dst_phys, (void *)src_phys, PAGE_SIZE);
                vmm_map_user_page(child_as, va, dst_phys, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
            }

            fork_ctx_t *ctx = kmalloc(sizeof(fork_ctx_t));
            if (!ctx) {
                regs->rax = (uint64_t)-1;
                break;
            }
            ctx->entry_rip = regs->rip;
            ctx->user_rsp = registers_tail(regs)->rsp;

            task_t *child = scheduler_spawn(fork_child_trampoline, ctx);
            if (!child) {
                kfree(ctx);
                regs->rax = (uint64_t)-1;
                break;
            }
            child->cr3 = child_as->pml4_phys;
            child->as = child_as;
            child->parent_id = parent->id;
            memcpy(child->fds, parent->fds, sizeof(parent->fds)); /* shares pipe
                                               * objects by pointer, same as real fork() */
            for (int i = 0; i < FD_MAX; i++) {
                if (child->fds[i].type == FD_PIPE_READ) ((pipe_t *)child->fds[i].obj)->read_refs++;
                else if (child->fds[i].type == FD_PIPE_WRITE) ((pipe_t *)child->fds[i].obj)->write_refs++;
            }

            kprintf("[kernel] fork(): task %d -> child task %d (own copied address space)\n",
                    parent->id, child->id);
            regs->rax = (uint64_t)child->id; /* parent sees the child's pid; the child
                                               * resumes via fork_child_trampoline with
                                               * rax=0 (zeroed below in enter_usermode's
                                               * fresh register state) */
            break;
        }

        case SYS_EXEC: {
            /* Copy the name out *before* resetting the address space:
             * the string itself lives in the caller's own user memory,
             * which vmm_reset_user_window() is about to free. Reading
             * regs->rdi after that points at memory that's already
             * been handed back to the frame allocator (or remapped to
             * the new program's own data) -- not a crash, since it's
             * still a valid physical page, just silently the wrong
             * bytes. */
            char namebuf[32];
            strncpy(namebuf, (const char *)regs->rdi, sizeof(namebuf) - 1);
            namebuf[sizeof(namebuf) - 1] = '\0';

            task_t *self = scheduler_current();
            address_space_t *as = (address_space_t *)self->as;
            if (!as) {
                regs->rax = (uint64_t)-1;
                break;
            }

            const uint8_t *data;
            size_t size;
            bool needs_free;
            if (!vfs_find(namebuf, &data, &size, &needs_free)) {
                kprintf("[kernel] exec(): no such file: %s\n", namebuf);
                regs->rax = (uint64_t)-1;
                break;
            }

            vmm_reset_user_window(as);

            uint64_t entry;
            bool loaded = elf_load(data, size, as, &entry);
            if (needs_free) kfree((void *)data);
            if (!loaded) {
                regs->rax = (uint64_t)-1;
                break;
            }

            uint64_t stack_phys = pmm_alloc_frame();
            if (!stack_phys) {
                regs->rax = (uint64_t)-1;
                break;
            }
            vmm_map_user_page(as, USER_STACK_PAGE, stack_phys, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);

            kprintf("[kernel] exec(): task %d now running '%s'\n", self->id, namebuf);

            /* No "return" from a successful exec(): rewrite where the
             * normal syscall-return iretq lands, and the old program is
             * simply gone -- the new one starts at its own entry point
             * with a fresh stack, as if it had always been running. */
            regs->rip = entry;
            registers_tail(regs)->rsp = USER_STACK_TOP;
            break;
        }

        case SYS_WAIT: {
            task_t *self = scheduler_current();
            int code = 0;
            while (!scheduler_reap_child(self->id, &code)) {
                yield();
            }
            regs->rax = (uint64_t)code;
            break;
        }

        default:
            kprintf("[kernel] unknown syscall %lu from task %d\n",
                    (unsigned long)regs->rax, scheduler_current()->id);
            break;
    }
}

void syscall_init(void) {
    register_interrupt_handler(128, syscall_handler);
}

void sys_write(const char *s) {
    /* Convenience wrapper kept for the kernel's own ring3 demos
     * (ring3_main below, diag.c's crashtest): computes the length
     * itself and issues the real 3-argument SYS_WRITE (fd=1) so
     * callers don't need updating just because the syscall ABI grew
     * an fd and an explicit length. */
    size_t len = 0;
    while (s[len]) len++;
    __asm__ volatile (
        "mov $1, %%rax\n"
        "mov $1, %%rdi\n"
        "mov %0, %%rsi\n"
        "mov %1, %%rdx\n"
        "int $0x80\n"
        :: "r"(s), "r"(len) : "rax", "rdi", "rsi", "rdx"
    );
}

void sys_exit(void) {
    __asm__ volatile (
        "mov $2, %%rax\n"
        "int $0x80\n"
        ::: "rax"
    );
    for (;;); /* unreachable */
}

static void ring3_main(void) {
    sys_write("[ring3] Hello from user mode! Running at CPL=3.\n");
    sys_write("[ring3] Trying a privileged instruction (cli) -- the kernel should catch the fault...\n");
    __asm__ volatile ("cli");
    sys_write("[ring3] you should NEVER see this line\n");
    sys_exit();
}

void usermode_demo_entry(void *arg) {
    (void)arg;
    uint8_t *user_stack = kmalloc(8192);
    uint64_t stack_top = (uint64_t)(user_stack + 8192);
    enter_usermode(ring3_main, stack_top);
}
