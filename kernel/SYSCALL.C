#include <sys/syscall.h>

#include "exec.h"
#include "memory.h"
#include "panic.h"
#include "printk.h"
#include "process.h"
#include "task.h"
#include "ufileio.h"

#include <errno.h>

extern long exit(int rc);

extern void sc_hook(void);

typedef long (*syscall)();

typedef struct syscall_t syscall_t;
struct syscall_t
{
	long (*fn)();
	unsigned argw;
};

syscall_t syscalls[] =
{
	{ &_exit, 1 },		/*  0 */
    { &open, 3 },		/*  1 */
	{ &close, 1 },		/*  2 */
    { &read, 4 },		/*  3 */
    { &write, 4 },		/*  4 */
	{ &exec, 2 },       /*  5 */
	{ &_wait, 2 },      /*  6 */
	{ &dup, 1 },		/*  7 */
	{ &sleep, 2 },		/*  8 */
};

size_t nsyscall = sizeof(syscalls)/sizeof(syscalls[0]);

/* MUST MATCH SCDSP.ASM!!! */
#define KSTACK_SIZE 512

typedef struct freestk_t freestk_t;
struct freestk_t {
	freestk_t *next;
	uint8_t *top;
};

freestk_t *freestk;

typedef struct ustack_t ustack_t;
struct ustack_t {
	uint16_t di;
	uint16_t si;
	uint16_t bp;
	uint16_t sp;
	uint16_t bx;
	uint16_t dx;
	uint16_t cx;
	uint16_t ax;
	uint16_t ds;
	uint16_t es;
	uint16_t ip;
	uint16_t cs;
	uint16_t fl;
};

void sc_init(void)
{
	int i;

	freestk = NULL;
	for (i = 0; i < 4; i++) {
		freestk_t *st = near_malloc(KSTACK_SIZE);
		if (st == NULL) {
			panic("sc_init: too little memory for kernel stacks");
		}
		st->next = freestk;
		st->top = ((uint8_t*)st) + KSTACK_SIZE;
		freestk = st;
	}

	sc_hook();
}

int sc_disp(ustack_t __far *ustack)
{
	uint16_t nsys;
	long ret = 0;

	nsys = ustack->ax;

	if (nsys >= nsyscall) {
        kprintf("syscall %d out of range\n", nsys); 	
		ustack->ax = (uint16_t)-EINVAL;
		return nsys;
	}

	switch (syscalls[nsys].argw) {
	case 1:
		ret = (*syscalls[nsys].fn)(ustack->cx);
		break;

	case 2:
		ret = (*syscalls[nsys].fn)(ustack->cx, ustack->dx);
		break;

	case 3:
		ret = (*syscalls[nsys].fn)(ustack->cx, ustack->dx, ustack->bx);
		break;

	case 4:
		ret = (*syscalls[nsys].fn)(ustack->cx, ustack->dx, ustack->bx, ustack->si);
		break;

	default:
		nsys = (uint16_t)-EINVAL;
	}

	ustack->ax = (uint16_t)(ret & 0xffff);
	ustack->dx = (uint16_t)((ret >> 16) & 0xffff);

	return nsys;
}

