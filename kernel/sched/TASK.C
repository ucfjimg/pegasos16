#include "taskp.h"

#include "intr.h"
#include "ktypes.h"
#include "memory.h"
#include "panic.h"
#include "printk.h"
#include "x86.h"

#include <stdarg.h>
#include <string.h>

/* the task table
 */
task_t tasks[MAX_TASK];

/* current task and free task list pointer
 */
task_t *curtask;
taskq_t tqueue[TASK_NPRI];
task_t *tsk_freel;

#define ENQUEUE(task) taskq_push(&tqueue[(task)->pri], task)

/* system tick counter, 18.2 ticks per second (55 ms)
 */ 
uint32_t tickcnt;
static taskq_t sleeping;

static task_t *task_alloc(void);
static void task_free(task_t *);
static void __far *task_stack_alloc(task_t *tsk, uint16_t bytes);
static void task_print(void);
static void idle_task(void);

/* asm: set up the timer interrupt
 */
extern void task_hook_timer(void);

/**
 * asm: schedule one-time async function callback 
 */
extern void __far task_async_stub(void);

/* Initialize the task system
 */
void task_init(void)
{
	task_t *p;
	task_t *end;
    int i;

	memset(tasks, 0, sizeof(tasks));

	/* populate the free list
     */
	for (p = tasks, end = &tasks[MAX_TASK-1]; p < end; p++) {
		p->next = p+1;
		p->prev = NULL;
	}
	tsk_freel = &tasks[0];

    /* clear the task queues
     */
    for (i = 0; i < TASK_NPRI; i++) {
        taskq_init(&tqueue[i]);
    }

	/* allocate a task for the caller 
     */
	curtask = task_alloc();
	curtask->pri = TASK_PRI_NORM;
    ENQUEUE(curtask);

	/* allocate the system idle task 
     */
	task_start((task_entry)&idle_task, 128, TASK_PRI_IDLE);

	taskq_init(&sleeping);

	/* safe to start task switching now 
     */
	tickcnt = 0;
	task_hook_timer();
}

/* Return the system tick count
 */
uint32_t ticks(void)
{
	return tickcnt;
}

/* Return the currently running task
 */
task_t *get_curtask()
{
	return curtask;
}

/* reserve a task slot and a stack. This is meant for places where a task must
 * be immediately available and we cannot fail to allocate it, such as 
 * interrupt handlers
 */
task_t *task_reserve(uint16_t ssize, taskpri_t pri)
{
	task_t *tsk;

	tsk = task_alloc();
	if (tsk == NULL) {
		return NULL;
	}

	memset(tsk, 0, sizeof(*tsk));
	if (task_stack_alloc(tsk, ssize) == NULL) {
		task_free(tsk);
		return NULL;
	}

	tsk->pri = pri;
	tsk->state = TS_BLOCKED;

	return tsk;
}

/* Allocate (but do not link) a kernel task. Process
 * tasks have a different way of allocating their stacks.
 * TODO right now we are assuming that a kernel task
 * will never exit, but we should be able to return
 * from them and have the task list get cleaned up.
 */
task_t *task_start(task_entry entry, uint16_t ssize, taskpri_t pri)
{
	uint16_t ds = get_ds();
	taskreg_t __far *reg;
	task_t *tsk;
	
	if (ssize < 2 * sizeof(taskreg_t)) {
		ssize = 2 * sizeof(taskreg_t);
	}

	if (pri > TASK_PRI_IDLE) {
		pri = TASK_PRI_IDLE;
	}

	tsk = task_alloc();
	reg = (taskreg_t __far *)task_stack_alloc(tsk, ssize);
	
	if (reg == NULL || tsk == NULL) {
		panic("out of tasks or stack space");
	}

	reg--;
	tsk->stack = reg;
	tsk->pri = pri;
	tsk->process = NULL;
	tsk->state = TS_READY;

	_fmemset(reg, 0, sizeof(*reg));

	reg->fl = FL_INTR_ENABLED; 
	reg->cs = KSEG_OF(entry);
	reg->ip = KOFFS_OF(entry);
	reg->ds = ds;
	reg->es = ds;

	DISABLE_INTR();
    ENQUEUE(tsk);
	ENABLE_INTR();

	return tsk;
}

/* Allocate and start a task that's tied to a process.
 */
task_t *task_from_proc(process_t *proc, uint16_t cs, uint16_t ip, uint16_t ss, uint16_t sp, uint16_t ax)
{
	taskreg_t __far *reg;
	task_t *tsk;
	
	reg = (taskreg_t __far *)MKFP(ss, sp);
	tsk = task_alloc();
	
	/* unlike a kernel task, it's not fatal to fail to
     * launch a process
	 */
	if (tsk == NULL) {
		return NULL;
	}

	reg--;
	tsk->stack = reg;
	tsk->pri = TASK_PRI_NORM;
	tsk->kstkbse = NULL;
	tsk->kstksz = 0;
	tsk->process = proc;
	tsk->state = TS_READY;

	_fmemset(reg, 0, sizeof(*reg));

	reg->fl = X86_INTR_ENABLE;
	reg->cs = cs;
	reg->ip = ip;
	reg->ax = ax;

	DISABLE_INTR();
    ENQUEUE(tsk);
	ENABLE_INTR();

	return tsk;
}

/* Put a task to sleep for the given number of timer ticks (each is
 * about 55 ms, or 18.2 ticks per second)
 */
void task_sleep(uint32_t ticks)
{
	if (ticks == 0) {
		ticks = 1;
	}
	DISABLE_INTR();
	curtask->due = tickcnt + ticks;
	curtask->state = TS_BLOCKED;
	task_dequeue(curtask);
	if (sleeping.head == NULL) {
		taskq_push(&sleeping, curtask);
	} else {
		register task_t *p = sleeping.head;
		do {
			if (p->due > curtask->due) {
				break;
			}
		} while (p != sleeping.head);

		curtask->owner = &sleeping;
		curtask->prev = p->prev;
		curtask->next = p;
    	p->prev->next = curtask;
    	p->prev = curtask;

		if (sleeping.head == p && curtask->due < p->due) {
			sleeping.head = curtask;
		}
	}

	task_yield_to(task_runnable());
	ENABLE_INTR();
}

/* syscall version of sleep which is measured in milliseconds
 */
long SYSCALL sleep(uint32_t ms)
{
	uint32_t ticks = TASK_MS_TO_TICKS(ms);
	task_sleep(ticks);
	return 0;
}

/* Wake up any tasks due on the current timer tick. Interrupts
 * are off when this is called and must stay that way.
 */
void task_wake(void)
{
	task_t *p;
	while (sleeping.head && sleeping.head->due == tickcnt) {
		p = sleeping.head;
		task_dequeue(p);
		p->state = TS_READY;
		ENQUEUE(p);
	}
}

/* Given an already allocated task with a priority, use it to schedule an 
 * async callback.
 */
extern void task_sch_async(task_t *tsk, tasync_t tn, int argw, ...)
{
	va_list va;
	uint16_t __far *args;
	taskreg_t __far *reg;
	uint16_t ds;
	int sti;

	void __far *stub = (void __far *)&task_async_stub;

	va_start(va, argw);

	/* args to called tn() go on the stack first */
	if (tsk->kstkbse == NULL) {
		panic("attempt to schedule a kernel async callback to a user process");
	}

	args = (uint16_t __far *)(tsk->kstkbse + tsk->kstksz);
	while (argw--) {
		*--args = va_arg(va, uint16_t);
	}

	/* arg to stub is our async callback */
	*--args = (uint16_t)tn;

	/* now comes the registers to force entry into the stub */
	reg = (taskreg_t __far *)args;
	reg--;

	_fmemset(reg, 0, sizeof(reg));
	
	ds = get_ds();
	reg->fl = X86_INTR_ENABLE;
	reg->cs = KSEG_OF(stub);
	reg->ip = KOFFS_OF(stub);
	reg->ds = ds;
	reg->es = ds;

	tsk->stack = (void __far *)reg;
	tsk->state = TS_READY;

	sti = intr_enabled();
	intr_cli();
    ENQUEUE(tsk);
	if (sti) {
		intr_sti();
	}
}

/* mark the current task as blocked, put it on the 
 * given blocked queue, and run whatever's runnable 
 * next.
 */
void task_block(taskq_t *blkq)
{
	intr_cli();
	task_dequeue(curtask);
	curtask->state = TS_BLOCKED;
	taskq_push(blkq, curtask);	
	task_yield_to(task_runnable());
}

/* unblock the given task
 */
void task_unblock(task_t *task)
{
	int sti;

	/* there is code that waits on a timeout or an interrupt.
	 * if the timeout hits before the interrupt, the interrupt will
	 * expect to unblock a task but the task is already unblocked
	 * and the task queue will be empty. It's convenient to handle
	 * this here rather than making all the interrupt handlers have to 
	 * have code for this race condition.
	 */
	if (task == NULL) { 
		return;
	}

	sti = intr_enabled();
	intr_cli();
	
	/* the task could also have already been unblocked elsewhere.
	 */
	if (task->state == TS_BLOCKED) {
		/* removes from whatever blocked queue the task is on */
		task_dequeue(task);

		task->state = TS_READY;
		ENQUEUE(task);
	}

	if (sti) {
		intr_sti();
	}
}

/* Allocate a task from the free task list
 */
task_t *task_alloc(void)
{
	task_t *p = NULL;

	/* TODO a less overkill way of stopping task switch
     */
	DISABLE_INTR();

	if (tsk_freel) {
		p = tsk_freel;
		tsk_freel = tsk_freel->next;
	}

	ENABLE_INTR();

	if (p == NULL) {
		kprintf("task_alloc(): out of tasks\n");
	}

	return p;
}

/* Put a task back on the free list
 */
void task_free(task_t *tsk)
{
	DISABLE_INTR();
	tsk->state = TS_FREE;	
	tsk->next = tsk_freel;
	tsk->prev = NULL;
	tsk_freel = tsk;
	ENABLE_INTR();
}

/* Allocate a stack for a task
 */
void __far *task_stack_alloc(task_t *tsk, uint16_t bytes)
{
	uint8_t __far *stack;

	/* round size up to a paragraph boundary
	 */
	if (bytes > MAX_STACK) {
		bytes = MAX_STACK;
	}
	bytes = (bytes + PARASIZE - 1) & ~(PARASIZE - 1);
	stack = near_malloc(bytes);
	if (stack == NULL) {
		return NULL;
	}

	tsk->kstkbse = stack;
	tsk->kstksz = bytes;

	return stack + bytes;
}

/* Find the next runnable task. Interrupts should be disabled.
 */
task_t *task_runnable(void)
{
	unsigned i;
		
	/* Note that since we have the system idle task in the lowest pri bucket, we
	 * are guaranteed to find something here.
	 */
	for (i = 0; i < TASK_NPRI; i++) {
        /* NB there should never be a task in the priority queues
         * that isn't runnable! If a task is blocked it should be in 
         * a blocked queue on some object.
         */
        if (tqueue[i].head) {
            return tqueue[i].head;
        }
	}
				
	panic("No runnable tasks!");
}

#if 0
void task_print(void)
{
	int i;
	kprintf("tasks (curtask %p freelist %p)\n", curtask, tsk_freel);
	for (i = 0; i < 4; i++) {
		kprintf("%p next %p prev %p stack %lp\n", &tasks[i], tasks[i].next, tasks[i].prev, tasks[i].stack);
	}		  
}
#endif



/* The system idle task
 */
#pragma warning( disable : 4702 ) /* unreachable code */
void idle_task(void)
{
	for(;;) {
		__asm hlt;
		task_yield_to(task_runnable());
	}
}

 

