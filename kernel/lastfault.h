#ifndef VOXI_LASTFAULT_H
#define VOXI_LASTFAULT_H

#include <voxi/types.h>

void lastfault_record(int vector, uint64_t rip, uint64_t cs, int task_id);
void lastfault_print(void);

#endif
