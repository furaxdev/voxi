#include "scheduler.h"
#include "lib/kprintf.h"
#include "arch/x86_64/gdt.h"
#include "syscall.h" /* SIGKILL */

extern void switch_to(uint64_t *old_rsp_store, uint64_t new_rsp);

static task_t *current = NULL;
static int next_id = 1;
static bool started = false;
static uint64_t kill_count = 0;
static uint64_t exit_count = 0;
static uint64_t switch_count = 0;
static uint8_t main_task_rsp0_stack[4096];

static const char *state_name(task_state_t s) {
    switch (s) {
        case TASK_READY: return "ready";
        case TASK_RUNNING: return "running";
        case TASK_DEAD: return "dead";
    }
    return "?";
}

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
    switch_count++;
    task_t *prev = current;
    task_t *next = pick_next();

    /* Signal delivery happens here, not asynchronously: a pending
     * signal only takes effect the next time its target would
     * otherwise be resumed. That's late by real-OS standards (a task
     * stuck without ever yielding or being preempted could delay it
     * indefinitely) but every task here does end up back through the
     * scheduler eventually, via the timer if nothing else. */
    while (next->pending_signal != 0 && next->state != TASK_DEAD) {
        int sig = next->pending_signal;

        /* SIGKILL can never be ignored -- same rule real POSIX uses. */
        if (sig != SIGKILL && (next->ignored_signals & (1u << sig))) {
            kprintf("[kernel] task %d ignored signal %d (SIG_IGN)\n", next->id, sig);
            next->pending_signal = 0;
            break; /* resume this task normally */
        }

        kprintf("[kernel] task %d killed by signal %d\n", next->id, sig);
        next->state = TASK_DEAD;
        next->pending_signal = 0;
        kill_count++;
        current = next; /* so pick_next() searches onward from here */
        next = pick_next();
    }

    if (prev->state != TASK_DEAD) prev->state = TASK_READY;
    current = next;
    current->state = TASK_RUNNING;
    set_rsp0_for(current);

    /* Switch address spaces if this task has its own (a process); kernel
     * code is mapped identically in every address space (see vmm.c), so
     * this is safe to do from here regardless of which one was active a
     * moment ago. */
    if (current->cr3 != prev->cr3) {
        __asm__ volatile ("mov %0, %%cr3" :: "r"(current->cr3) : "memory");
    }

    switch_to(&prev->rsp, current->rsp);
}

void scheduler_kill_current_and_switch(void) {
    current->state = TASK_DEAD;
    kill_count++;
    scheduler_switch(); /* never resumes this task again */
}

void scheduler_exit_current_and_switch(void) {
    current->state = TASK_DEAD;
    exit_count++;
    scheduler_switch(); /* never resumes this task again */
}

uint64_t scheduler_kill_count(void) {
    return kill_count;
}

uint64_t scheduler_exit_count(void) {
    return exit_count;
}

uint64_t scheduler_switch_count(void) {
    return switch_count;
}

bool scheduler_reap_child(int parent_id, int *exit_code_out) {
    if (!current) return false;
    task_t *t = current;
    do {
        if (t->parent_id == parent_id && t->state == TASK_DEAD && !t->reaped) {
            t->reaped = true;
            if (exit_code_out) *exit_code_out = t->exit_code;
            return true;
        }
        t = t->next;
    } while (t != current);
    return false;
}

void scheduler_dump(void) {
    if (!current) {
        kprintf("scheduler not started\n");
        return;
    }
    kprintf("id   state    kstack\n");
    task_t *t = current;
    do {
        kprintf("%-4d %-8s %p%s\n", t->id, state_name(t->state),
                (void *)t->kstack, t == current ? "  <- current" : "");
        t = t->next;
    } while (t != current);
    kprintf("tasks killed by hardware faults so far: %lu\n", (unsigned long)kill_count);
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

task_t *scheduler_find_task(int id) {
    if (!current) return NULL;
    task_t *t = current;
    do {
        if (t->id == id) return t;
        t = t->next;
    } while (t != current);
    return NULL;
}

bool scheduler_signal(int id, int sig) {
    task_t *t = scheduler_find_task(id);
    if (!t || t->state == TASK_DEAD) return false;
    t->pending_signal = sig;
    return true;
}
