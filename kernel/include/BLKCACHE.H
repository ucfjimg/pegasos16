#ifndef BLKCACHE_H_
#define BLKCACHE_H_

#include "ktypes.h"
#include <sys/types.h>

typedef struct block_t block_t;

#ifdef KERNEL
typedef block_t __far * blkptr_t;
#else
typedef block_t * blkptr_t;
#endif			
 
struct block_t {
    blkptr_t next;
    blkptr_t prev;
    int inuse;
    int dirty : 1;
    int dev;
	int unit;
	blkno_t blkno;
    uint8_t __far *data;
};

typedef struct bcstats_t bcstats_t;
struct bcstats_t {
    long gets;
    long puts;
    long reads;
    long writes;
};

#define F_BCGET_OVERW 1    /* caller will totally overwrite this block */

extern int bc_init(void);

extern int bc_get(dev_t dev, unit_t unit, blkno_t blkno, blkptr_t *block, int flags); 
extern int bc_put(blkptr_t block); 

extern int bc_sync(void);
extern void bc_get_stats(bcstats_t *stats);

extern int bc_read(dev_t dev, unit_t unit, blkno_t blkno, int blks, uint8_t __far *data);


#endif

