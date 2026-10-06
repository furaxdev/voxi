#include "scheduler.h"
#include "lib/kprintf.h"

extern void switch_to(uint64_t *old_rsp_store, uint64_t new_rsp);

static task_t *current = NULL;
static int next_id = 1;

void scheduler_init(void) {
    current = task_wrap_current(0);
    current->next = current; /* circular list of one */
}

task_t *scheduler_spawn(void (*entry)(void *), void *arg) {
    task_t *t = task_create(entry, arg, next_id++);
    if (!t) return NULL;

    /* insert right after `current` in the ring */
    t->next = current->next;
    current->next = t;
    return t;
}

void yield(void) {
    if (!current) return;

    task_t *prev = current;
    task_t *next = prev->next;

    /* find the next non-dead task; if none, just return to ourselves */
    while (next->state == TASK_DEAD && next != prev) next = next->next;
    if (next == prev) return;

    if (prev->state != TASK_DEAD) prev->state = TASK_READY;
    next->state = TASK_RUNNING;
    current = next;

    switch_to(&prev->rsp, next->rsp);
}

bool scheduler_all_done(void) {
    if (!current) return true;
    task_t *t = current->next;
    while (t != current) {
        if (t->state != TASK_DEAD) return false;
        t = t->next;
    }
    return true;
}

task_t *scheduler_current(void) {
    return current;
}
