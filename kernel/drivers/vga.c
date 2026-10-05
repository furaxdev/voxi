#include "vga.h"
#include <voxi/io.h>

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_MEM    ((volatile uint16_t *)0xB8000)

static size_t row = 0;
static size_t col = 0;
static uint8_t color = (VGA_BLACK << 4) | VGA_LGRAY;

static inline uint16_t vga_entry(char c, uint8_t clr) {
    return (uint16_t)(uint8_t)c | ((uint16_t)clr << 8);
}

static void move_cursor(void) {
    uint16_t pos = (uint16_t)(row * VGA_WIDTH + col);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void vga_set_color(enum vga_color fg, enum vga_color bg) {
    color = (uint8_t)((bg << 4) | (fg & 0x0F));
}

void vga_clear(void) {
    for (size_t y = 0; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA_MEM[y * VGA_WIDTH + x] = vga_entry(' ', color);
    row = 0;
    col = 0;
    move_cursor();
}

void vga_init(void) {
    vga_set_color(VGA_LGRAY, VGA_BLACK);
    vga_clear();
}

static void scroll(void) {
    for (size_t y = 1; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA_MEM[(y - 1) * VGA_WIDTH + x] = VGA_MEM[y * VGA_WIDTH + x];
    for (size_t x = 0; x < VGA_WIDTH; x++)
        VGA_MEM[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', color);
    row = VGA_HEIGHT - 1;
}

void vga_putc(char c) {
    if (c == '\n') {
        col = 0;
        row++;
    } else if (c == '\r') {
        col = 0;
    } else if (c == '\b') {
        if (col > 0) {
            col--;
            VGA_MEM[row * VGA_WIDTH + col] = vga_entry(' ', color);
        }
    } else if (c == '\t') {
        col = (col + 8) & ~7u;
    } else {
        VGA_MEM[row * VGA_WIDTH + col] = vga_entry(c, color);
        col++;
    }

    if (col >= VGA_WIDTH) {
        col = 0;
        row++;
    }
    if (row >= VGA_HEIGHT) {
        scroll();
    }
    move_cursor();
}

void vga_write(const char *s) {
    while (*s) vga_putc(*s++);
}
