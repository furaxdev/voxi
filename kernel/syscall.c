#include "syscall.h"
#include "arch/x86_64/idt.h"
#include "scheduler.h"
#include "mm/heap.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "elf.h"
#include "fs/ramfs.h"
#include "drivers/keyboard.h"
#include "lib/string.h"
#include "lib/kprintf.h"

#define USER_STACK_PAGE (USER_BASE + 0x1FF000ull)
#define USER_STACK_TOP  (USER_BASE + 0x200000ull)

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

        case SYS_WRITE:
            kprintf("%s", (const char *)regs->rdi);
            break;

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
            char *buf = (char *)regs->rdi;
            uint64_t maxlen = regs->rsi;
            uint64_t n = 0;
            while (n < maxlen) {
                char c = keyboard_getchar(); /* blocks (sti;hlt) until a key arrives */
                if (c == '\n') break;
                if (c == '\b') { if (n > 0) n--; continue; }
                buf[n++] = c;
            }
            if (n < maxlen) buf[n] = '\0';
            regs->rax = n;
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
            if (!ramfs_find(namebuf, &data, &size)) {
                kprintf("[kernel] exec(): no such file: %s\n", namebuf);
                regs->rax = (uint64_t)-1;
                break;
            }

            vmm_reset_user_window(as);

            uint64_t entry;
            if (!elf_load(data, size, as, &entry)) {
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
    __asm__ volatile (
        "mov $1, %%rax\n"
        "mov %0, %%rdi\n"
        "int $0x80\n"
        :: "r"(s) : "rax", "rdi"
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
