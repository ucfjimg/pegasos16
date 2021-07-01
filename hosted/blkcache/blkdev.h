/*
 * Interface for block device drivers
 */
#ifndef BLKDEV_H_
#define BLKDEV_H_

#include "blkcache.h"

typedef struct blkdev_t blkdev_t;

struct blkdev_t {
	int (*init)(void);
	int (*get_units)(void);
	int (*read)(unit_t unit, blkno_t blkno, uint8_t __far *buffer);
	int (*write)(unit_t unit, blkno_t blkno, uint8_t __far *buffer);
};

extern int loop_open(const char *name);
#define LOOP_UNIT 0x0100

#endif

