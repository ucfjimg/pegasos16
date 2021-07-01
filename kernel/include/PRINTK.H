#ifndef _PRINTK
#define _PRINTK

#include <stdarg.h>

typedef void (*putc_t)(void *arg, int ch);

extern void vprintf(putc_t fn, void *arg, char __far *fmt, va_list va);
extern void kprintf(char __far *fmt, ...);
extern void kputc(int ch);

#endif

