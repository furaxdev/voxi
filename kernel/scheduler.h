#ifndef VOXI_SCHEDULER_H
#define VOXI_SCHEDULER_H

#include <voxi/types.h>
#include "task.h"

/* Wraps the currently-executing context (the caller -- the boot thread)
 * as task #0. Call once, early, after the heap and GDT/TSS are up. */
void scheduler_init(void);

/* Creates a new task and adds it to the round-robin ready queue. */
task_t *scheduler_spawn(void (*entry)(void *), void *arg);

/* Switches to the next ready task. Called both voluntarily (yield(), a
 * plain function call) and from the timer IRQ handler (preemption) --
 * either way it's just a register/stack-pointer swap (switch_to, which
 * ends in a plain `ret`), never an iretq, so it works identically
 * whether or not we're currently inside an interrupt handler. */
void scheduler_switch(void);

static inline void yield(void) { scheduler_switch(); }

/* True once every task besides the current one has finished (died). */
bool scheduler_all_done(void);

task_t *scheduler_current(void);

/* True once scheduler_init() has run -- the timer handler checks this
 * before attempting to preempt, since nothing is set up before then. */
bool scheduler_started(void);

/* Called when the *current* task crashes (always a ring3 task in
 * practice): marks it dead and switches away for good. */
void scheduler_kill_current_and_switch(void);

#endif
