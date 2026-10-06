#ifndef VOXI_SCHEDULER_H
#define VOXI_SCHEDULER_H

#include <voxi/types.h>
#include "task.h"

/* Wraps the currently-executing context (the caller) as task #0 so it can
 * later be switched away from and back to. Call once, early. */
void scheduler_init(void);

/* Creates a new task and adds it to the round-robin ready queue. */
task_t *scheduler_spawn(void (*entry)(void *), void *arg);

/* Cooperatively hands the CPU to the next READY task. A task must call
 * this itself (or return from its entry function, which calls it in a
 * loop via task_trampoline) to let others run -- this is a cooperative
 * scheduler, there is no timer-driven preemption yet. */
void yield(void);

/* True once every spawned task has finished (died). */
bool scheduler_all_done(void);

task_t *scheduler_current(void);

#endif
