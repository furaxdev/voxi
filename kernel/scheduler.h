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

/* Called when the *current* task exits cleanly via SYS_EXIT: marks it
 * dead and switches away, same mechanics as a kill but counted
 * separately so 'killcount' isn't lying about what actually happened. */
void scheduler_exit_current_and_switch(void);

/* How many tasks have been killed by a hardware fault so far. */
uint64_t scheduler_kill_count(void);

/* How many tasks have exited cleanly via SYS_EXIT so far. */
uint64_t scheduler_exit_count(void);

/* If some dead, not-yet-reaped task has parent_id == parent_id, reports
 * its exit code via *exit_code_out, marks it reaped, and returns true.
 * Otherwise returns false without blocking -- SYS_WAIT loops on this
 * with a yield() between tries. */
bool scheduler_reap_child(int parent_id, int *exit_code_out);

/* How many context switches (yield-driven or timer-driven, all of
 * them) have happened so far. */
uint64_t scheduler_switch_count(void);

/* Prints every task currently known to the scheduler and its state --
 * the complete process table, exhaustively, on one screen. */
void scheduler_dump(void);

/* Finds a task by id, or NULL. */
task_t *scheduler_find_task(int id);

/* Marks `id` for signal delivery; takes effect the next time the
 * scheduler would resume it (see the loop in scheduler_switch()).
 * Returns false if no such task exists or it's already dead. */
bool scheduler_signal(int id, int sig);

#endif
