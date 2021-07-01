#include "process.h"

#include "fileio.h"
#include "intr.h"
#include "memory.h"
#include "printk.h"
#include "task.h"

#include <errno.h>
#include <sys/wait.h>

#define MAX_PROCESS 32

static process_t proc[MAX_PROCESS];
static int nextpid = 1;

static semaph_t critsect;

void proc_init(void)
{
    sema_init(&critsect, 1);
}

process_t *proc_alloc(void)
{
	int i;
    int j;

    sema_down(&critsect);
	for (i = 0; i < MAX_PROCESS; i++) {
		if (proc[i].pid == 0) {
			break;
		}
	}

	if (i == MAX_PROCESS) {
        sema_up(&critsect);
		kprintf("proc_alloc(): out of procs\n");
		return NULL;
	}

	proc[i].pid = nextpid++;
    sema_up(&critsect);

    for (j = 0; j < PROC_FILES; j++) {
        proc[i].fds[j].kfd = -1;
    }

	taskq_init(&proc->waitq);

	proc->image = NULL;
	proc->zombie = 0;

	return &proc[i];
}


void proc_free(process_t *proc)
{
	int i;

	/* close any hanging file handles 
	 */
	for (i = 0; i < PROC_FILES; i++) {
		if (proc->fds[i].kfd != -1) {
			kclose(proc->fds[i].kfd);
			proc->fds[i].kfd = -1;
		}
	}

	if (proc->image) {
		kfree(proc->image);
		proc->image = NULL;
	}

	proc->task = NULL;
	proc->zombie = 0;

    sema_down(&critsect);
	proc->pid = 0;
    sema_up(&critsect);
}

process_t *get_curproc()
{
    return get_curtask()->process;
}

long SYSCALL _exit(int rc)
{
	task_t *task;
	process_t *proc;
	int sti;

	/* we are in the context of the process that
	 * wants to exit
	 */
	if ((proc = get_curtask()->process) == NULL) {
		kprintf("exit() called from a kernel task context!\n");
		return -EINVAL;
	}

	task = proc->task;
	proc->task = NULL;

	if (proc->parent) {
		sti = intr_cli();
		proc->zombie = 1;
		proc->rc = rc & 0xff;

		if (proc->parent->waitq.head) {
			task_unblock(proc->parent->waitq.head);
		}
		intr_restore(sti);
	} else {
		proc_free(proc);
	}

	task->process = NULL;
	task->state = TS_FREEING;

	/* note that we don't actually call task_exit here. we do that in the
	 * syscall return path, due to having to free the kernel stack we're on
	 */
	return 0;
}

long _wait(int __far *status)
{
	process_t *curr;
	int i;
	int sti;
	int pid;

	if ((curr = get_curtask()->process) == NULL) {
		kprintf("wait() called from a kernel task context!\n");
		return -EINVAL;
	}

	for (;;) {
		sti = intr_cli();
		for (i = 0; i < MAX_PROCESS; i++) {
			if (proc[i].pid && proc[i].zombie && proc[i].parent == curr) {
				break;
			}
		}
		if (i < MAX_PROCESS) {
			intr_restore(sti);
			break;
		}

		task_block(&proc->waitq);
	}

	*status = _WIFEXITED | proc[i].rc;
	pid = proc[i].pid;

	proc_free(&proc[i]);

	return pid;
}
