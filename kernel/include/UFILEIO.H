#ifndef UFILEIO_H
#define UFILEIO_H

#include <sys/syscall.h>
#include <sys/types.h>

extern long SYSCALL open(char __far *path, int flags);
extern long SYSCALL close(int fd);
extern long SYSCALL read(int fd, char __far *path, size_t size);
extern long SYSCALL write(int fd, char __far *path, size_t size);
extern long SYSCALL dup(int fd);

#endif
