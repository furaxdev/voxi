#include "process.h"
#include "elf.h"
#include "fs/ramfs.h"
#include "mm/vmm.h"
#include "mm/pmm.h"
#include "mm/heap.h"
#include "scheduler.h"
#include "lib/kprintf.h"

#define USER_STACK_PAGE (USER_BASE + 0x1FF000ull)
#define USER_STACK_TOP  (USER_BASE + 0x200000ull)

extern void enter_usermode(void (*entry)(void), uint64_t user_stack_top) __attribute__((noreturn));

typedef struct {
    uint64_t entry;
    uint64_t stack_top;
} process_ctx_t;

static void process_trampoline(void *arg) {
    process_ctx_t *ctx = (process_ctx_t *)arg;
    void (*entry)(void) = (void (*)(void))ctx->entry;
    uint64_t stack_top = ctx->stack_top;
    kfree(ctx);
    enter_usermode(entry, stack_top); /* never returns */
}

bool process_spawn(const char *name) {
    const uint8_t *data;
    size_t size;
    if (!ramfs_find(name, &data, &size)) {
        kprintf("run: no such file: %s (try 'ls')\n", name);
        return false;
    }

    address_space_t *as = vmm_create_address_space();
    if (!as) {
        kprintf("run: out of address spaces\n");
        return false;
    }

    uint64_t entry;
    if (!elf_load(data, size, as, &entry)) {
        kprintf("run: failed to load %s\n", name);
        return false;
    }

    uint64_t stack_phys = pmm_alloc_frame();
    if (!stack_phys) {
        kprintf("run: out of physical memory\n");
        return false;
    }
    vmm_map_user_page(as, USER_STACK_PAGE, stack_phys, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);

    process_ctx_t *ctx = kmalloc(sizeof(process_ctx_t));
    if (!ctx) {
        kprintf("run: out of memory\n");
        return false;
    }
    ctx->entry = entry;
    ctx->stack_top = USER_STACK_TOP;

    task_t *t = scheduler_spawn(process_trampoline, ctx);
    if (!t) {
        kprintf("run: failed to spawn task\n");
        return false;
    }
    t->cr3 = as->pml4_phys;
    t->as = as;

    kprintf("run: spawned '%s' as task #%d, entry=%p, own page tables (cr3=%p)\n",
            name, t->id, (void *)entry, (void *)t->cr3);
    return true;
}
