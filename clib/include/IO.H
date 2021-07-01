#ifndef IO_H_
#define IO_H_

#include <sys/types.h>

extern int open(const char far *fn, int flags);
extern int close(int fd);
extern size_t read(int fd, char far *buffer, size_t size);
extern size_t write(int fd, char far *buffer, size_t size);
extern off_t lseek(int fd, off_t offset, int whence);
extern int dup(int fd);


#endif
