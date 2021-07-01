#ifndef FILEIO_H_
#define FILEIO_H_

#include <sys/stat.h>
#include <sys/types.h>

extern void fio_init(void);
extern int kopen(char *path, int flags);
extern int kclose(int fd);
extern size_t kread(int fd, void __far *buf, size_t size);
extern size_t kwrite(int fd, void __far *buf, size_t size);
extern int kfstat(int fd, struct stat *st);
extern int kdup(int fd);
extern off_t kseek(int fd, long offset, int whence);

#endif
