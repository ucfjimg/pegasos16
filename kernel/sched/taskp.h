#ifndef TASK_P_H_
#define TASK_P_H_

#include "task.h"

/* TODO this is a small fixed limit for now
 */
#define MAX_TASK 32
#define MAX_STACK 2048

/**** disable warnings we trigger all over the place in the scheduler ****/

/* casting a function <-> data pointer. 
 */
#pragma warning( disable : 4054 )

/* inline asm precludes optimization
 */
#pragma warning( disable: 4704 )


/* TODO use these from intr.h 
 */
#define DISABLE_INTR() _asm cli
#define ENABLE_INTR() _asm sti

#define FL_INTR_ENABLED 0x0200

/* the format of registers as on the stack for
 * context switch
 */
typedef struct taskreg_t taskreg_t;
struct taskreg_t {
	uint16_t es;
	uint16_t ds;
	uint16_t di;
	uint16_t si;
	uint16_t bp;
	uint16_t sp;
	uint16_t bx;
	uint16_t dx;
	uint16_t cx;
	uint16_t ax;
	uint16_t ip;
	uint16_t cs;
	uint16_t fl;
};

extern task_t *curtask;
extern taskq_t tqueue[TASK_NPRI];

extern void task_yield_to(task_t *to);
extern task_t *task_runnable(void);

#endif

