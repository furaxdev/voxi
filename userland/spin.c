/*
 * Loops forever on purpose -- something to aim the shell's `kill`
 * command at. Preempted by the timer like anything else (it never
 * calls yield() itself), so it doesn't block the rest of the system;
 * it just never finishes on its own.
 */
#include "ulib.h"

int main(void) {
    uputs(1, "[spin] pid ");
    pid_t me = sys_getpid();
    char buf[4] = {0};
    buf[0] = (char)('0' + (me / 10) % 10);
    buf[1] = (char)('0' + me % 10);
    buf[2] = '\n';
    uputs(1, buf[0] == '0' ? &buf[1] : &buf[0]);
    uputs(1, "[spin] looping forever -- try 'kill <that pid>' from the shell\n");

    for (;;) {
        /* nothing: the timer preempts this regardless of whether it
         * ever yields, same as the kernel's own heartbeat task */
    }

    return 0;
}
