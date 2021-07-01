#ifndef STRING_H_

#include <sys/types.h>

extern int __far _fmemcmp(
	const void __far *l, const void __far *r, size_t n); 


extern void *memcpy(void *d, void *s, size_t n);

extern void __far * __far _fmemcpy(
	const void __far *d, const void __far *s, size_t n);

extern void __far * __far _fmemmove(
	const void __far *d, const void __far *s, size_t n);

extern void __far * __far _fmemset(
	const void __far *d, int c, size_t n);

extern void * memset(
	const void *d, int c, size_t n);

extern size_t strlen(const char *s);
extern size_t __far _fstrlen(const char __far *s);
extern char * strcpy(const char *d, const char *s);
extern char __far * __far _fstrcpy(const char __far *d, const char __far *s);
extern char * strrchr(const char *s, int ch);

extern int strcmp(const char *p, const char *q);
#endif

