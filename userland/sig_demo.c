/*
 * Proves signal *disposition* works, not just delivery: this process
 * ignores SIGTERM (sys_sigignore), so `kill PID` (which sends SIGTERM
 * by default -- see shell.c) does nothing to it; only `kill -9 PID`
 * (SIGKILL, which can never be ignored) actually ends it.
 */
#include "ulib.h"
#include <stdint.h>

int main(void) {
    long me = sys_getpid();
    uprintf("[sig_demo] pid=%d, ignoring SIGTERM (try 'kill %d' -- nothing happens)\n", (int)me, (int)me);
    uprintf("[sig_demo] then try 'kill -9 %d' -- SIGKILL always works\n", (int)me);

    sys_sigignore(SIGTERM);

    /* loops forever on purpose, same as spin.c -- only SIGKILL ends
     * this one, which is the whole point of the demo */
    long counter = 0;
    uint64_t spin_target = 0;
    for (;;) {
        if (++spin_target % 50000000 == 0) {
            counter++;
            uprintf("[sig_demo] still alive, tick %d (SIGTERM did nothing)\n", (int)counter);
        }
    }

    return 0;
}
