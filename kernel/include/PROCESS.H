#ifndef PROCESS_H_
#define PROCESS_H_

#include "ktypes.h"
#include "task.h"

#include <sys/syscall.h>
#include <sys/types.h>

#ifndef _TASK_T_DEFINED
# define _TASK_T_DEFINED
typedef struct task_t task_t;
#endif

#ifndef _PROCESS_T_DEFINED
# define _PROCESS_T_DEFINED
typedef struct process_t process_t;
#endif

#define PROC_FILES 8

typedef struct procfile_t procfile_t;
struct procfile_t {
    int kfd;
};

struct process_t {
	int pid;
    process_t *parent;
	task_t *task;
    taskq_t waitq;

    unsigned zombie : 1;
    unsigned rc : 8;

    uint8_t __far *image;
    procfile_t fds[PROC_FILES];
};

extern void proc_init(void);
extern process_t *proc_alloc(void);
extern void proc_free(process_t *proc);

/* the running process */
extern process_t *get_curproc();

extern long SYSCALL _exit(int rc);
extern long SYSCALL _wait(int __far *status);

#endif

