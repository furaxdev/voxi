#ifndef VOXI_HEARTBEAT_H
#define VOXI_HEARTBEAT_H

#include <voxi/types.h>

/* A background task that just spins incrementing a counter, with no
 * voluntary yield() at all. If the shell still responds to the keyboard
 * while this runs, that's proof the timer-driven preemptive scheduler
 * really is taking the CPU away from it periodically. */
void heartbeat_start(void);
uint64_t heartbeat_count(void);

#endif
