/*
 * The implementation half of ulib.h: a tiny malloc and a tiny printf,
 * the first two pieces of an actual libc for Voxi userland (as opposed
 * to every program hand-rolling its own integer-to-string loop).
 */
#include "ulib.h"
#include <stdarg.h>
#include <stdint.h>

/* ---- malloc: bump + freelist over a static arena ---- */

#define UHEAP_SIZE (32 * 1024)
static unsigned char uheap[UHEAP_SIZE];

typedef struct ublock {
    long size; /* usable bytes after this header */
    int free;
    struct ublock *prev;
    struct ublock *next;
} ublock_t;

static ublock_t *uheap_head = 0;
static ublock_t *uheap_tail = 0;

static void uheap_init(void) {
    uheap_head = (ublock_t *)uheap;
    uheap_head->size = UHEAP_SIZE - (long)sizeof(ublock_t);
    uheap_head->free = 1;
    uheap_head->prev = 0;
    uheap_head->next = 0;
    uheap_tail = uheap_head;
}

void *umalloc(long size) {
    if (!uheap_head) uheap_init();
    if (size <= 0) return 0;
    size = (size + 7) & ~7L;

    for (ublock_t *b = uheap_head; b; b = b->next) {
        if (!b->free || b->size < size) continue;

        if (b->size >= size + (long)sizeof(ublock_t) + 8) {
            ublock_t *nb = (ublock_t *)((unsigned char *)(b + 1) + size);
            nb->size = b->size - size - (long)sizeof(ublock_t);
            nb->free = 1;
            nb->prev = b;
            nb->next = b->next;
            if (b->next) b->next->prev = nb;
            else uheap_tail = nb;
            b->next = nb;
            b->size = size;
        }
        b->free = 0;
        return (void *)(b + 1);
    }
    return 0; /* out of (arena) memory */
}

void ufree(void *ptr) {
    if (!ptr) return;
    ublock_t *b = (ublock_t *)ptr - 1;
    b->free = 1;

    if (b->next && b->next->free) {
        b->size += (long)sizeof(ublock_t) + b->next->size;
        b->next = b->next->next;
        if (b->next) b->next->prev = b;
        else uheap_tail = b;
    }
    if (b->prev && b->prev->free) {
        ublock_t *p = b->prev;
        p->size += (long)sizeof(ublock_t) + b->size;
        p->next = b->next;
        if (b->next) b->next->prev = p;
        else uheap_tail = p;
    }
}

/* ---- string.h basics ---- */

int ustrcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

char *ustrcpy(char *dst, const char *src) {
    char *d = dst;
    while ((*d++ = *src++));
    return dst;
}

void *umemcpy(void *dst, const void *src, long n) {
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    for (long i = 0; i < n; i++) d[i] = s[i];
    return dst;
}

void *umemset(void *dst, int val, long n) {
    unsigned char *d = (unsigned char *)dst;
    for (long i = 0; i < n; i++) d[i] = (unsigned char)val;
    return dst;
}

/* ---- printf: %d %u %x %c %s %%, no width/precision/length modifiers ---- */

static void print_uint(uint64_t v, int base, int uppercase) {
    char buf[24];
    const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    int i = 0;
    if (v == 0) buf[i++] = '0';
    while (v > 0) {
        buf[i++] = digits[v % (unsigned)base];
        v /= (unsigned)base;
    }
    while (i > 0) { char c = buf[--i]; sys_write(1, &c, 1); }
}

static void print_int(int64_t v) {
    if (v < 0) {
        char c = '-';
        sys_write(1, &c, 1);
        print_uint((uint64_t)(-v), 10, 0);
    } else {
        print_uint((uint64_t)v, 10, 0);
    }
}

void uprintf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            char c = *p;
            sys_write(1, &c, 1);
            continue;
        }
        p++;
        switch (*p) {
            case 's': { const char *s = va_arg(ap, const char *); uputs(1, s ? s : "(null)"); break; }
            case 'd': case 'i': print_int((int64_t)va_arg(ap, int)); break;
            case 'u': print_uint((uint64_t)va_arg(ap, unsigned int), 10, 0); break;
            case 'x': print_uint((uint64_t)va_arg(ap, unsigned int), 16, 0); break;
            case 'X': print_uint((uint64_t)va_arg(ap, unsigned int), 16, 1); break;
            case 'c': { char c = (char)va_arg(ap, int); sys_write(1, &c, 1); break; }
            case '%': { char c = '%'; sys_write(1, &c, 1); break; }
            default: { char c1 = '%', c2 = *p; sys_write(1, &c1, 1); sys_write(1, &c2, 1); break; }
        }
    }

    va_end(ap);
}
