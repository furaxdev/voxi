#ifndef VOXI_SERIAL_H
#define VOXI_SERIAL_H

#include <voxi/types.h>

void serial_init(void);
void serial_putc(char c);
void serial_write(const char *s);

#endif
