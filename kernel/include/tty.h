#ifndef TTY_H_
#define TTY_H_

#include "ktypes.h"
#include <sys/types.h>

typedef struct tty_iob_t tty_iob_t;
struct tty_iob_t {
	tty_iob_t *next;
	uint8_t __far *data;
	size_t ptr;
	size_t size;
};

extern void tty_dump(void);

#endif
