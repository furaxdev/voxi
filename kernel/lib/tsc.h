#ifndef VOXI_TSC_H
#define VOXI_TSC_H

#include <voxi/types.h>

static inline uint64_t tsc_read(void) {
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

#endif
