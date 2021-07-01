#ifndef STDLIB_H_
#define STDLIB_H_

#include <sys/types.h>

#ifndef _NULL_DEFINED
#define _NULL_DEFINED
# define NULL (0)
#endif

extern void exit(int);

extern void *malloc(size_t bytes);
extern void *calloc(size_t n, size_t size);
extern void *realloc(void *p, size_t bytes);
extern void free(void *p);

extern void sleep(unsigned long ms);

#endif

