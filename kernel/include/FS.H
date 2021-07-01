#ifndef FS_H_
#define FS_H_

#include "ktypes.h"
#include "blkcache.h"

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

/*
 * we're not supporting anything but root mount right now 
 */
extern int fs_mount(int dev, int unit);
extern int fs_umount(void);

#define INO_GET_CREAT 0x0001

extern ino_t ino_get(char *path, int flags);
extern int ino_put(ino_t ino);
extern int ino_stat(ino_t ino, struct stat *st);
extern size_t ino_read(ino_t ino, void __far *buf, off_t off, size_t size);

#define INO_ERR(ino) \
	((((long)(ino)) >= -EMAX) && (((long)(ino)) < 0) ? \
		((int)ino) : \
		0) 





#endif

