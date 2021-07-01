/*
 * Interface for block device drivers
 */
#ifndef BLKDEV_H_
#define BLKDEV_H_

#include "ktypes.h"

typedef struct blkdev_t blkdev_t;

struct blkdev_t {
	int (*init)(void);
	int (*get_units)(void);
	int (*read)(unit_t unit, blkno_t blkno, uint8_t far *buffer);
	int (*write)(unit_t unit, blkno_t blkno, uint8_t far *buffer);
};


#endif

