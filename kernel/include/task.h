#ifndef TASK_H_
#define TASK_H_

#include "ktypes.h"

#ifndef _TASK_T_DEFINED
# define _TASK_T_DEFINED
typedef struct task_t task_t;
#endif

#ifndef _TASKQ_T_DEFINED
# define _TASKQ_T_DEFINED
typedef struct taskq_t taskq_t;
#endif

#ifndef _PROCESS_T_DEFINED
# define _PROCESS_T_DEFINED
typedef struct process_t process_t;
#endif

enum tstate_t {
	TS_RUN,
	TS_READY,
	TS_BLOCKED,
};


/* Priorities - lower is higher priority
 *
 * 0-15 are hardware interrupts, same priority as PIC
 * 16-18 are for tasks/procs - 17 is normal, 18 is nice, 16 is boosted
 * 20 is just for the idle task
 */
#define TASK_PRI_IRQ0	0
#define TASK_PRI_IRQ15	15
#define TASK_PRI_BOOST  16
#define TASK_PRI_NORM   17
#define TASK_PRI_NICE   18
#define TASK_PRI_IDLE   19
#define TASK_NPRI		20

typedef uint8_t taskpri_t;

struct task_t {
	task_t *next;
	task_t *prev;
    taskq_t *owner;
	process_t *process;
	void __far *stack;
	uint8_t __far *kstkbse;
	uint16_t kstksz;
	uint8_t state;
	taskpri_t pri;
};

struct taskq_t {
    task_t *head;
};

typedef struct semaph_t semaph_t;

struct semaph_t {
	int count;
    taskq_t blocked;
};

extern void task_init(void);

extern void __far task_switch(void);
extern uint16_t get_ds(void);

typedef void (__far *task_entry)(void);

extern task_t *task_reserve(uint16_t ssize, taskpri_t pri);
extern task_t *task_start(task_entry entry, uint16_t ssize, taskpri_t pri);
extern task_t *task_from_proc(process_t *proc, uint16_t cs, uint16_t ip, uint16_t ss, uint16_t sp);
extern void task_exit(void);
extern void task_lock(void);
extern void task_unlock(void);
extern task_t *get_curtask();

typedef void (*tasync_t)();
extern void task_sch_async(task_t *tsk, tasync_t tn, int argw, ...);

extern uint32_t ticks(void);

extern void taskq_init(taskq_t *queue);
extern void taskq_push(taskq_t *queue, task_t *task);
extern task_t *taskq_pop(taskq_t *queue);
extern void task_dequeue(task_t *task);

extern void sema_init(semaph_t *s, int cnt);
extern void sema_down(semaph_t *s);
extern void sema_up(semaph_t *s);




#endif
