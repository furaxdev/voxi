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

static void print_uint(uint64_t v, int base, int uppercase, int width, char pad, int left) {
    char buf[32];
    const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    int i = 0;
    if (v == 0) buf[i++] = '0';
    while (v > 0) {
        buf[i++] = digits[v % (uint64_t)base];
        v /= (uint64_t)base;
    }
    int len = i;
    if (left) {
        while (i > 0) out_char(buf[--i]);
        for (int k = len; k < width; k++) out_char(' ');
    } else {
        while (len < width) { out_char(pad); len++; }
        while (i > 0) out_char(buf[--i]);
    }
}

static void print_int(int64_t v, int width, char pad, int left) {
    char numbuf[24];
    int n = 0;
    int neg = v < 0;
    uint64_t uv = neg ? (uint64_t)(-v) : (uint64_t)v;

    if (uv == 0) numbuf[n++] = '0';
    while (uv > 0) { numbuf[n++] = (char)('0' + (uv % 10)); uv /= 10; }
    int len = n + (neg ? 1 : 0);

    if (left) {
        if (neg) out_char('-');
        while (n > 0) out_char(numbuf[--n]);
        for (int k = len; k < width; k++) out_char(' ');
    } else if (pad == '0') {
        if (neg) out_char('-');
        for (int k = len; k < width; k++) out_char('0');
        while (n > 0) out_char(numbuf[--n]);
    } else {
        for (int k = len; k < width; k++) out_char(' ');
        if (neg) out_char('-');
        while (n > 0) out_char(numbuf[--n]);
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
        int left = 0;
        if (*p == '-') { left = 1; p++; }
        char pad = ' ';
        if (*p == '0') { pad = '0'; p++; }
        int width = 0;
        while (*p >= '0' && *p <= '9') { width = width * 10 + (*p - '0'); p++; }

        int longflag = 0;
        if (*p == 'l') { longflag = 1; p++; if (*p == 'l') p++; }

        switch (*p) {
            case 's': {
                const char *s = va_arg(args, const char *);
                if (!s) s = "(null)";
                int len = 0;
                for (const char *q = s; *q; q++) len++;
                if (left) {
                    out_str(s);
                    for (int k = len; k < width; k++) out_char(' ');
                } else {
                    for (int k = len; k < width; k++) out_char(' ');
                    out_str(s);
                }
                break;
            }
            case 'c':
                out_char((char)va_arg(args, int));
                break;
            case 'd':
            case 'i':
                if (longflag) print_int(va_arg(args, int64_t), width, pad, left);
                else print_int(va_arg(args, int32_t), width, pad, left);
                break;
            case 'u':
                if (longflag) print_uint(va_arg(args, uint64_t), 10, 0, width, pad, left);
                else print_uint(va_arg(args, uint32_t), 10, 0, width, pad, left);
                break;
            case 'x':
                if (longflag) print_uint(va_arg(args, uint64_t), 16, 0, width, pad, left);
                else print_uint(va_arg(args, uint32_t), 16, 0, width, pad, left);
                break;
            case 'X':
                if (longflag) print_uint(va_arg(args, uint64_t), 16, 1, width, pad, left);
                else print_uint(va_arg(args, uint32_t), 16, 1, width, pad, left);
                break;
            case 'p':
                out_str("0x");
                print_uint((uint64_t)va_arg(args, void *), 16, 0, 16, '0', 0);
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
