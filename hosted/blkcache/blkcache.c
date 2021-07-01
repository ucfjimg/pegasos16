/*
 * block cache for utilities hosted on DOS
 */
#include "blkcache.h"
#include "blkdev.h"

#include <errno.h>
#include <malloc.h>
#include <stdio.h>
#include <sys/types.h>


#define MAX_BLOCK 8
#define BLKSIZE 512

#define INVALID_BLOCK ((blkno_t)-1)

/* convert a far pointer to a 20-bit linear address
 */
#define FP_TO_LINEAR(fp) \
	(((((uint32_t)(fp)) >> 12) & 0xffff0) + (((uint32_t)(fp))&0xffff))


/* return the dma page of a far pointer pointer. each DMA page is 64k
 * and thus in a 20 bit address space there are only 16 of them
 */
#define FP_TO_DMA_PAGE(fp) \
	((FP_TO_LINEAR(fp) >> 16) & 0x0000f)

/*
 * For this interface, dev should be 0 and unit should be the BIOS drive number
 * (0=A:, 1=B:, 0x80=C:, etc.) 
 *
 * This implentation also does not implement locking as there is only one process.
 */


extern struct blkdev_t fdd_dev;
extern struct blkdev_t hdd_dev;
extern struct blkdev_t loop_dev;

static uint8_t __far *cache;
static block_t *blocks;
static block_t *blocklist;

typedef struct device_t device_t;
struct device_t
{
	blkdev_t *dev;
	int unit;
};

static int flush_block(block_t *blk);
static bool_t get_device(unit_t unit, device_t *dev);

/* Initialize the cache
 */
#pragma optimize( "", off )
int bc_init(void)
{
	uint8_t __far *p;
	block_t *b;
	int i;
	int rc;

	rc = fdd_dev.init();
	if (rc != 0) {
		return rc;
	}

	rc = hdd_dev.init();
	if (rc != 0) {
		return rc;
	}

	cache = _fmalloc((MAX_BLOCK + 1) * BLKSIZE);
	blocks = calloc((MAX_BLOCK + 1), sizeof(block_t));

	if (cache == NULL || blocks == NULL) {
		_ffree(cache);
		free(blocks);
		return -ENOMEM;
	}

	/* blocks are a circular list pre-initialized with a DMA-safe
	 * data buffer
	 */
	for (i = 0; i < MAX_BLOCK + 1; i++) {
		uint32_t page0, page1;

		p = cache + i * BLKSIZE;
		b = blocks + i;
		page0 = FP_TO_DMA_PAGE(p);
		page1 = FP_TO_DMA_PAGE(p + BLKSIZE - 1);
		if (page0 != page1) {
			continue;
		}

		b->data = p;
		b->blkno = INVALID_BLOCK;
		
		if (blocklist == NULL) {
			blocklist = b;
			b->next = b->prev = b;
		} else {
			b->prev = blocklist;
			b->next = blocklist->next;
			blocklist->next->prev = b;
			blocklist->next = b;
		}
	}
	
	return 0;	
}

int bc_sync(void)
{
	block_t *b;

	/* ensure block is valid
	 */
	b = blocklist;
	do {
		flush_block(b);
		b = b->next;
	} while (b != blocklist);

	return 0;
}


#if 0
void bc_dump(void)
{
	block_t *pb;
	block_t *pc;

	pb = blocklist;
	do {
		printf("%p <%p >%p inuse %u dirty %u block %5ld\n",
			pb,
			pb->next,
			pb->prev,
			pb->inuse ? 1 : 0,
			pb->dirty ? 1 : 0,
			(long)pb->blkno);
		pb = pb->next;
	} while (pb != blocklist);
	
	pb = blocklist;
	do {
		pc = blocklist;
		do {
			if (pb != pc && pb->blkno == pc->blkno && pb->blkno != INVALID_BLOCK) {
				printf("!!! duplicate blocks in cache !!!\n");
			}
			pc = pc->next;
		} while (pc != blocklist);
	} while (pb != blocklist);
}
#endif

#pragma optimize( "", on )

/*
 * Get a block from the device
 */
int bc_get(dev_t dev, unit_t unit, blkno_t blkno, blkptr_t *block, int flags)
{
	block_t *b;
	device_t d;
	int rc;

	if (!get_device(unit, &d)) {
		return -ENODEV;
	}

	*block = NULL;

	/*
	 * first see if the block is already there and not locked
	 */
	b = blocklist;
	do {
		if (b->unit == unit && b->blkno == blkno) {
			*block = b;
			b->inuse = TRUE;
			return 0;
		}
		
		if (*block == NULL && !b->inuse) {
			*block = b;
		}
		b = b->next;
	} while (b != blocklist);

	if (*block == NULL) {
		return -ENOMEM;
	}

	b = *block;
	
	flush_block(b);

	blocklist = b->next;

	b->dev = dev;
	b->unit = unit;
	b->blkno = blkno;

	if ((flags & F_BCGET_OVERW) == 0) {
		printf("CACHE: read block %lu blk %p\n",
			b->blkno,
			b);
		rc = d.dev->read(d.unit, blkno, b->data);
		if (rc != 0) {
			b->blkno = INVALID_BLOCK;
			return rc;
		}
	}

	b->inuse = TRUE;
	blocklist = b->next;

	return 0;
}

/* Put a block back to the device
 */ 
int bc_put(blkptr_t block)
{
	block_t *b;

	/* ensure block is valid
	 */
	b = blocklist;
	do {
		if (b == block) {
			break;
		}
		b = b->next;
	} while (b != blocklist);	

	/* ensure block state is what we expect
	 */		   
	if (b != block || !b->inuse) {
		printf("b != block %d b->inuse %d\n", b != block, b->inuse);
		return -EINVAL;
	}

	b->inuse = FALSE;
	
	return 0;
}

/* Given a unit_t as a BIOS drive#, figure out the proper block device and unit
 */
bool_t get_device(unit_t unit, device_t *dev)
{
	if (unit >= LOOP_UNIT) {
		dev->dev = &loop_dev;
		unit -= LOOP_UNIT;
	} else if (unit & 0x80) {
		dev->dev = &hdd_dev;
		unit &= 0x7f; 
	} else {
		dev->dev = &fdd_dev;
	}

	if (unit >= dev->dev->get_units()) {
		return FALSE;
	}

	dev->unit = unit;

	return TRUE;
}

/* Flush a block to disk
 */
int flush_block(block_t *blk)
{
	int rc;
	device_t d;
	
	if (!blk->dirty) {
		return 0;
	}

	blk->dirty = FALSE;

	if (!get_device(blk->unit, &d)) {
		printf("CACHE: flush could not find device for block %lu\n",
			blk->blkno
		);
		return -ENODEV;
	}

	rc = d.dev->write(d.unit, blk->blkno, blk->data);
	if (rc != 0) {
		printf("CACHE: failed to write back block %lu\n",
			blk->blkno
		);
		return rc;
	}

	return 0;
}	
