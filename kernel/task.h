#ifndef VOXI_TASK_H
#define VOXI_TASK_H

#include <voxi/types.h>

#define TASK_KSTACK_SIZE (16 * 1024)

typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_DEAD,
} task_state_t;

typedef struct task {
    uint64_t rsp;           /* saved stack pointer when not running */
    uint8_t *kstack;        /* base of this task's kernel stack (for freeing) */
    void (*entry)(void *);
    void *arg;
    int id;
    task_state_t state;
    struct task *next;      /* circular ready-queue link */
} task_t;

/* Allocates a kernel stack and primes it so that switching to this task
 * for the first time lands in task_trampoline(), which calls entry(arg). */
task_t *task_create(void (*entry)(void *), void *arg, int id);

/* Wraps whatever is currently executing (e.g. the boot thread) into a
 * task_t so the scheduler can switch away from it and back. */
task_t *task_wrap_current(int id);

#endif
