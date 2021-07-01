#include "ufileio.h"

#include "exec.h"
#include "fileio.h"
#include "ktypes.h"
#include "memory.h"
#include "printk.h"
#include "process.h"
#include "task.h"

#include <errno.h>
#include <string.h>

#pragma warning( disable : 4702 )


long SYSCALL open(char __far *path, int flags)
{
    process_t *pr = get_curproc();
    int i;
    int len;
    int kfd;
    char *npath;

    if (pr == NULL) {
        return -EINVAL;
    }

    /* since processes don't have threads, we can only get here once
     * at a time per process and don't need a critical section
     */
    for (i = 0; i < PROC_FILES; i++) {
        if (pr->fds[i].kfd == -1) {
            break;
        }
    }

    if (i == PROC_FILES) {
        kprintf("open(): no fd\n");
        return -ENOMEM;
    }

    len = _fstrlen(path);

    npath = near_malloc(len + 1);
    if (npath == NULL) {
        kprintf("open(): no mem\n");
        return -ENOMEM;
    }
    _fstrcpy(npath, path);
    npath[len] = '\0';

    kfd = kopen(npath, flags);
    if (kfd < 0) {
        near_free(npath);
        return kfd;
    }

    pr->fds[i].kfd = kfd;
    
    near_free(npath);
    return i;
}

long SYSCALL close(int fd)
{
    process_t *pr = get_curproc();

    if (pr == NULL) {
        return (size_t)-EINVAL;
    }

    if (fd < 0 || fd >= PROC_FILES || pr->fds[fd].kfd == -1) {
        return (size_t)-EBADF;
    }

    kclose(pr->fds[fd].kfd);
    pr->fds[fd].kfd = -1;

    return 0;
}

long SYSCALL write(int fd, char __far *buf, size_t size)
{
    process_t *pr = get_curproc();
    size_t wrote;

    if (pr == NULL) {
        kprintf("write: no process\n");
        return -EINVAL;
    }

    if (fd < 0 || fd >= PROC_FILES || pr->fds[fd].kfd == -1) {
        kprintf("write: bad fd\n");
        return -EBADF;
    }

    wrote = kwrite(
        pr->fds[fd].kfd,
        buf,
        size
    );

    return wrote;
}

long SYSCALL read(int fd, char __far *buf, size_t size)
{
    static int warned = 0;
    process_t *pr = get_curproc();
    size_t got;

    if (pr == NULL) {
        kprintf("read: no process\n");
        return -EINVAL;
    }

    if (fd < 0 || fd >= PROC_FILES || pr->fds[fd].kfd == -1) {
        if (warned == 0) {
        kprintf("read: fd %d EBADF task %p\n", fd, get_curtask());
            warned = 1;
        }
        return -EBADF;
    }

    got = kread(
        pr->fds[fd].kfd,
        buf,
        size
    );

    return got;
}

long SYSCALL dup(int fd)
{
    process_t *pr = get_curproc();
    int i;
    int nfd;

    if (pr == NULL) {
        return (size_t)-EINVAL;
    }

    if (fd < 0 || fd >= PROC_FILES || pr->fds[fd].kfd == -1) {
        return (size_t)-EBADF;
    }

    /* since processes don't have threads, we can only get here once
     * at a time per process and don't need a critical section
     */
    for (i = 0; i < PROC_FILES; i++) {
        if (pr->fds[i].kfd == -1) {
            break;
        }
    }

    if (i == PROC_FILES) {
        return -ENOMEM;
    }

    nfd = kdup(pr->fds[fd].kfd);
    if (nfd < 0) {
        return nfd;
    }

    pr->fds[i].kfd = nfd;
    return i;
}


