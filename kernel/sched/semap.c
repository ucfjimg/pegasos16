#include "taskp.h"

#include "panic.h"

#define SEMA_SIG 0x4153

/* Initialize a new semaphore with a count
 */
void sema_init(semaph_t *s, int cnt)
{
	s->sig = SEMA_SIG;
	s->count = cnt;
	s->last_owner = NULL;
    taskq_init(&s->blocked);
}

/* Decrement a semaphore. If the value goes below
 * zero, this task will block.
 */
void sema_down(semaph_t *s)
{
	task_t *cur;

	if (s->sig != SEMA_SIG) {
		panic("semaphore down when not initialized");
	}
	
	DISABLE_INTR();

	cur = s->blocked.head;
	if (cur) {
		do {
			if (cur == curtask) {
				panic("semaphore: curtask is already blocked!");
			}
			cur = cur->next;
		} while (cur != s->blocked.head);
	}


	s->count--;
	if (s->count < 0) {
		cur = curtask;
		
		/* unlink current task and mark it blocked */
		task_dequeue(cur);
		cur->state = TS_BLOCKED;

        taskq_push(&s->blocked, cur);
		task_yield_to(task_runnable());
	}

	s->last_owner = curtask;

	ENABLE_INTR();
}

/* Increment a semaphore. If needed, wake up a task.
 */
void sema_up(semaph_t *s)
{
	task_t *next;

	if (s->sig != SEMA_SIG) {
		panic("semaphore up when not initialized");
	}

	DISABLE_INTR();

	s->count++;
	if (s->count <= 0) {
		next = taskq_pop(&s->blocked);

		if (next == NULL) {
			ENABLE_INTR();
			panic("semaphore: count not consistent with queue state");
		}

		/* dequeued task is ready, put it back in the task list
		 */
		next->state = TS_READY;
        taskq_push(&tqueue[next->pri], next);
	}

	s->last_owner = NULL;

	ENABLE_INTR();
}
