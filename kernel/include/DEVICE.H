#ifndef DEVICE_H_
#define DEVICE_H_

#include "ktypes.h"
#include <sys/devnos.h>

typedef struct chardev_t chardev_t;
struct chardev_t {
	void (*init)(void);
	int (*open)(int unit);
	int (*close)(int unit);
	int (*read)(int unit, void __far *buf, int size);	
	int (*write)(int unit, void __far *buf, int size);	
};

typedef struct blkdev_t blkdev_t;
struct blkdev_t {
	void (*init)(void);
	int (*read)(int unit, void __far *buf, blkno_t blk, int size);	
	int (*write)(int unit, void __far *buf, blkno_t blk, int size);	
};

extern void dev_init(void);
extern blkdev_t *dev_getbdev(int idx);
extern chardev_t *dev_getcdev(int idx);


#endif

