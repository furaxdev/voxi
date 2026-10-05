#include "keyboard.h"
#include <voxi/io.h>
#include "../arch/x86_64/idt.h"

#define KBD_DATA_PORT 0x60
#define BUF_SIZE 256

static char ring[BUF_SIZE];
static volatile uint32_t head = 0, tail = 0;
static bool shift = false;
static bool caps = false;

/* US QWERTY scancode set 1, unshifted / shifted */
static const char map_lower[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0 /*ctrl*/,'a','s','d','f','g','h','j','k','l',';','\'','`',
    0 /*lshift*/,'\\','z','x','c','v','b','n','m',',','.','/', 0 /*rshift*/,
    '*', 0 /*alt*/, ' ', 0 /*capslock*/,
};

static const char map_upper[128] = {
    0, 27, '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,'A','S','D','F','G','H','J','K','L',':','"','~',
    0,'|','Z','X','C','V','B','N','M','<','>','?', 0,
    '*', 0, ' ', 0,
};

static void push_char(char c) {
    uint32_t next = (head + 1) % BUF_SIZE;
    if (next == tail) return; /* buffer full, drop */
    ring[head] = c;
    head = next;
}

static void keyboard_handler(registers_t *regs) {
    (void)regs;
    uint8_t sc = inb(KBD_DATA_PORT);

    if (sc == 0x2A || sc == 0x36) { shift = true; return; }
    if (sc == 0xAA || sc == 0xB6) { shift = false; return; }
    if (sc == 0x3A) { caps = !caps; return; } /* capslock press */

    if (sc & 0x80) return; /* key release, ignore otherwise */

    if (sc >= 128) return;
    char c = (shift ^ caps) ? map_upper[sc] : map_lower[sc];
    if (c) push_char(c);
}

void keyboard_init(void) {
    register_interrupt_handler(33, keyboard_handler);
}

bool keyboard_has_char(void) {
    return head != tail;
}

char keyboard_getchar(void) {
    while (head == tail) {
        __asm__ volatile ("sti; hlt");
    }
    char c = ring[tail];
    tail = (tail + 1) % BUF_SIZE;
    return c;
}
