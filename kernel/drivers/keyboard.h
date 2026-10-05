#ifndef VOXI_KEYBOARD_H
#define VOXI_KEYBOARD_H

#include <voxi/types.h>

void keyboard_init(void);
/* Blocks (busy-waits with hlt) until a character is available. */
char keyboard_getchar(void);
bool keyboard_has_char(void);

#endif
