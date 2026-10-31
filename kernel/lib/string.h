#ifndef VOXI_STRING_H
#define VOXI_STRING_H

#include <voxi/types.h>

void *memset(void *dst, int val, size_t len);
void *memcpy(void *dst, const void *src, size_t len);
void *memmove(void *dst, const void *src, size_t len);
int memcmp(const void *a, const void *b, size_t len);

size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
char *strcpy(char *dst, const char *src);
char *strncpy(char *dst, const char *src, size_t n);
char *strcat(char *dst, const char *src);
char *strchr(const char *s, int c);

/* Parses a hex number, with or without a leading "0x". */
uint64_t parse_hex(const char *s);

/* Parses a plain decimal number. */
uint64_t parse_dec(const char *s);

/* Splits on whitespace: returns the next token from *cursor (advancing
 * it past it and any trailing whitespace), or NULL if nothing is left.
 * Mutates the underlying buffer (writes a NUL after each token), same
 * as strtok. */
char *next_token(char **cursor);

#endif
