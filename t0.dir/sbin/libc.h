#ifndef _LIBC_H_
#define _LIBC_H_

#include "sys.h"

#define MISSING() do { \
    putstr("\n*** missing code at"); \
    putstr(__FILE__); \
    putdec(__LINE__); \
} while (0)

extern void* malloc(size_t size);
extern void free(void*);
extern void* realloc(void* ptr, size_t newSize);

void* memset(void* p, int val, size_t sz);
void* memcpy(void* dest, void* src, size_t n);

extern int putchar(int c);
extern int puts(const char *p);

extern int isdigit(int c);
extern int printf(const char* fmt, ...);

extern void cp(int from, int to);

#endif
