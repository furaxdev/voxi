#include "heartbeat.h"
#include "scheduler.h"

static volatile uint64_t counter = 0;

static void heartbeat_task(void *arg) {
    (void)arg;
    for (;;) counter++; /* no yield(): relies entirely on timer preemption */
}

void heartbeat_start(void) {
    scheduler_spawn(heartbeat_task, NULL);
}

uint64_t heartbeat_count(void) {
    return counter;
}
