#include "scheduler.h"
#include "lib/kprintf.h"
#include "arch/x86_64/gdt.h"

extern void switch_to(uint64_t *old_rsp_store, uint64_t new_rsp);

static task_t *current = NULL;
static int next_id = 1;
static bool started = false;
static uint8_t main_task_rsp0_stack[4096];

static void set_rsp0_for(task_t *t) {
    uint64_t top = t->kstack
        ? (uint64_t)(t->kstack + TASK_KSTACK_SIZE)
        : (uint64_t)(main_task_rsp0_stack + sizeof(main_task_rsp0_stack));
    tss_set_rsp0(top);
}

void scheduler_init(void) {
    current = task_wrap_current(0);
    current->next = current; /* circular list of one */
    set_rsp0_for(current);
    started = true;
}

bool scheduler_started(void) {
    return started;
}

task_t *scheduler_spawn(void (*entry)(void *), void *arg) {
    task_t *t = task_create(entry, arg, next_id++);
    if (!t) return NULL;

    /* insert right after `current` in the ring */
    t->next = current->next;
    current->next = t;
    return t;
}

static task_t *pick_next(void) {
    task_t *next = current->next;
    while (next->state == TASK_DEAD && next != current) next = next->next;
    return next;
}

void scheduler_switch(void) {
    task_t *prev = current;
    task_t *next = pick_next();

    if (prev->state != TASK_DEAD) prev->state = TASK_READY;
    current = next;
    current->state = TASK_RUNNING;
    set_rsp0_for(current);

    switch_to(&prev->rsp, current->rsp);
}

void scheduler_kill_current_and_switch(void) {
    current->state = TASK_DEAD;
    scheduler_switch(); /* never resumes this task again */
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
