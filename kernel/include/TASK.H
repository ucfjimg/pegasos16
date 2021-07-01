#ifndef TASK_H_
#define TASK_H_

#include "ktypes.h"
#include <sys/syscall.h>

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
	TS_FREE,
	TS_RUN,
	TS_READY,
	TS_BLOCKED,
	TS_FREEING,
};

#define TASK_QUANTA_MS 55
#define TASK_MS_TO_TICKS(ms) (((ms) + TASK_QUANTA_MS - 1) / TASK_QUANTA_MS) 

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
	uint32_t due;
};

struct taskq_t {
    task_t *head;
};

typedef struct semaph_t semaph_t;

struct semaph_t {
	int sig;
	int count;
	task_t *last_owner;
    taskq_t blocked;
};

typedef struct event_t event_t;
struct event_t {
	event_t *next;
	task_t *blocked;
};

extern void task_init(void);

extern void __far task_switch(void);
extern uint16_t get_ds(void);
extern uint16_t get_cs(void);
extern uint16_t get_ss(void);

typedef void (__far *task_entry)(void);

extern task_t *task_reserve(uint16_t ssize, taskpri_t pri);
extern task_t *task_start(task_entry entry, uint16_t ssize, taskpri_t pri);
extern task_t *task_from_proc(process_t *proc, uint16_t cs, uint16_t ip, uint16_t ss, uint16_t sp, uint16_t ax);
extern void task_exit(void);
extern void task_lock(void);
extern void task_unlock(void);
extern void task_sleep(uint32_t ticks);
extern task_t *get_curtask();

extern long SYSCALL sleep(uint32_t ms);

typedef void (*tasync_t)();
extern void task_sch_async(task_t *tsk, tasync_t tn, int argw, ...);

extern void task_block(taskq_t *blkq);
extern void task_unblock(task_t *task);

extern uint32_t ticks(void);

extern void taskq_init(taskq_t *queue);
extern void taskq_push(taskq_t *queue, task_t *task);
extern task_t *taskq_pop(taskq_t *queue);
extern void task_dequeue(task_t *task);

extern void sema_init(semaph_t *s, int cnt);
extern void sema_down(semaph_t *s);
extern void sema_up(semaph_t *s);

/* a critical section is just a semaphore with a count of 1 */
typedef semaph_t critsect_t;
#define CS_INIT(cs) sema_init(cs, 1)
#define CS_ENTER(cs) sema_down(cs)
#define CS_LEAVE(cs) sema_up(cs)

extern void ev_init(event_t *event);
extern int  ev_wait(int nev, event_t *evs[]);
extern void ev_signal(event_t *event);
extern int  ev_signaled(event_t *event);

#endif

