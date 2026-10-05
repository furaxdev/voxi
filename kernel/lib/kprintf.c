#include "kprintf.h"
#include <voxi/types.h>
#include <stdarg.h>
#include "../drivers/vga.h"
#include "../drivers/serial.h"

static void out_char(char c) {
    vga_putc(c);
    serial_putc(c);
}

static void out_str(const char *s) {
    while (*s) out_char(*s++);
}

static void print_uint(uint64_t v, int base, int uppercase, int width, char pad) {
    char buf[32];
    const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    int i = 0;
    if (v == 0) buf[i++] = '0';
    while (v > 0) {
        buf[i++] = digits[v % (uint64_t)base];
        v /= (uint64_t)base;
    }
    while (i < width) buf[i++] = pad;
    while (i > 0) out_char(buf[--i]);
}

static void print_int(int64_t v, int width, char pad) {
    if (v < 0) {
        out_char('-');
        print_uint((uint64_t)(-v), 10, 0, width, pad);
    } else {
        print_uint((uint64_t)v, 10, 0, width, pad);
    }
}

void kprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            out_char(*p);
            continue;
        }
        p++;
        char pad = ' ';
        int width = 0;
        if (*p == '0') { pad = '0'; p++; }
        while (*p >= '0' && *p <= '9') { width = width * 10 + (*p - '0'); p++; }

        int longflag = 0;
        if (*p == 'l') { longflag = 1; p++; if (*p == 'l') p++; }

        switch (*p) {
            case 's': {
                const char *s = va_arg(args, const char *);
                out_str(s ? s : "(null)");
                break;
            }
            case 'c':
                out_char((char)va_arg(args, int));
                break;
            case 'd':
            case 'i':
                if (longflag) print_int(va_arg(args, int64_t), width, pad);
                else print_int(va_arg(args, int32_t), width, pad);
                break;
            case 'u':
                if (longflag) print_uint(va_arg(args, uint64_t), 10, 0, width, pad);
                else print_uint(va_arg(args, uint32_t), 10, 0, width, pad);
                break;
            case 'x':
                if (longflag) print_uint(va_arg(args, uint64_t), 16, 0, width, pad);
                else print_uint(va_arg(args, uint32_t), 16, 0, width, pad);
                break;
            case 'X':
                if (longflag) print_uint(va_arg(args, uint64_t), 16, 1, width, pad);
                else print_uint(va_arg(args, uint32_t), 16, 1, width, pad);
                break;
            case 'p':
                out_str("0x");
                print_uint((uint64_t)va_arg(args, void *), 16, 0, 16, '0');
                break;
            case '%':
                out_char('%');
                break;
            default:
                out_char('%');
                out_char(*p);
                break;
        }
    }

    va_end(args);
}
