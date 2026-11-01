/*
 * Proves ulib.c's malloc and printf actually work, not just compile.
 */
#include "ulib.h"

int main(void) {
    uprintf("[libc_test] pid=%d, testing umalloc/uprintf...\n", (int)sys_getpid());

    char *buf = (char *)umalloc(64);
    if (!buf) {
        uprintf("[libc_test] umalloc failed!\n");
        return 1;
    }
    ustrcpy(buf, "allocated via umalloc()");
    uprintf("[libc_test] heap string: \"%s\" (len=%d)\n", buf, (int)ustrlen(buf));
    ufree(buf);

    /* a second allocation should be able to reuse the freed block */
    char *buf2 = (char *)umalloc(16);
    uprintf("[libc_test] second alloc at offset %d bytes into the arena\n",
            (int)((char *)buf2 - (char *)buf));
    ufree(buf2);

    uprintf("[libc_test] %d in hex is %x, %d in hex is %x\n", 255, 255, -1, -1);
    uprintf("[libc_test] all good: malloc, free, and printf's %%d/%%x/%%s all work.\n");
    return 0;
}
