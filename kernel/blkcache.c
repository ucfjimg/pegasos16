/*
 * block cache for utilities hosted on DOS
 */
#include "blkcache.h"
#include "device.h"
#include "memory.h"
#include "panic.h"
#include "printk.h"
#include "task.h"

#include <errno.h>
#include <string.h>

#define MAX_BLOCK 32

/* TODO other block sizes? Really only for CDROM */
#define BLKSIZE 512

#define INVALID_BLOCK ((blkno_t)-1)

static uint8_t __far *cache;
static blkptr_t blocks;
       blkptr_t blocklist;
static critsect_t block_cs;


static bcstats_t stats;

void bc_dump(void);

extern uint16_t get_ds(void);

/* Initialize the cache
 */
int bc_init(void)
{
	uint8_t __far *p;
	blkptr_t b;
	int i;

	cache = kmalloc(MAX_BLOCK * BLKSIZE);
	blocks = kmalloc(MAX_BLOCK * sizeof(block_t));

	if (cache == NULL || blocks == NULL) {
		panic("failed to initialize block cache");
	}

	/* blocks are a circular list pre-initialized with a
	 * data buffer
	 */
	blocklist = NULL;
	for (i = 0; i < MAX_BLOCK; i++) {
		p = cache + i * BLKSIZE;
		b = blocks + i;

		b->data = p;
		b->blkno = INVALID_BLOCK;
		b->inuse = 0;
		b->dirty = 0;
		b->dev = -1;
		b->unit = -1;
		b->blkno = 0;

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

	CS_INIT(&block_cs);

	return 0;	
}

void bc_dump(void)
{
	int i;
	blkptr_t pb;
	blkptr_t pc;

	i = 0;
	pb = blocklist;
	do {
		kprintf("%lp <%lp >%lp inuse %u dirty %u block %ld\n",
			pb,
			pb->next,
			pb->prev,
			pb->inuse,
			pb->dirty ? 1 : 0,
			(long)pb->blkno);
		pb = pb->next;
		i++;
		if (i > MAX_BLOCK) {
			break;
		}
	} while (pb != blocklist);

	pb = blocklist;
	do {
		pc = blocklist;
		do {
			if (pb != pc && pb->blkno == pc->blkno && pb->blkno != INVALID_BLOCK) {
				kprintf("!!! duplicate blocks in cache !!!\n");
			}
			pc = pc->next;
		} while (pc != blocklist);
	} while (pb != blocklist);
}

/*
 * Get a block from the device
 */
int bc_get(dev_t dev, unit_t unit, blkno_t blkno, blkptr_t *block, int flags)
{
	blkptr_t b;
	int rc;
	blkdev_t *pdev = dev_getbdev(dev);

	UNREFERENCED(flags);

	*block = NULL;

	CS_ENTER(&block_cs);

	stats.gets++;

	/*
	 * first see if the block is already there
	 */	  	
	b = blocklist;
	do {
		if (b->dev == dev && b->unit == unit && b->blkno == blkno) {
			*block = b;
			b->inuse++;
			CS_LEAVE(&block_cs);
			return 0;
		}
		
		if (*block == NULL && b->inuse == 0) {
			*block = b;
		}
		b = b->next;
	} while (b != blocklist);

	if (*block == NULL) {
		/* TODO grow, or block the process, or something */
		panic("bc_get: all blocks are in use!");
	}

	b = *block;

	blocklist = b->next;
	
	b->dev = dev;
	b->unit = unit;
	b->blkno = blkno;

	stats.reads++;
	rc = pdev->read(unit, b->data, blkno, BLKSIZE);
	if (rc != BLKSIZE) {
		kprintf("bc_get: pdev->read %d (blkno %ld)\n", rc, blkno);
		CS_LEAVE(&block_cs);
		return rc;
	}

	b->inuse++;
	blocklist = b->next;

	CS_LEAVE(&block_cs);
	return 0;
}

/* Put a block back to the device
 */ 
int bc_put(blkptr_t block)
{
	blkptr_t b;
	blkdev_t *pdev;

	CS_ENTER(&block_cs);
	stats.puts++;
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
	if (b != block || b->inuse == 0) {
		panic("bc: invalid block in bc_put");
	}

	b->inuse--;

	/* if block has been changed, write it back to media
	 */
	if (b->inuse == 0 && b->dirty) {
		int rc;

		stats.writes++;
		pdev = dev_getbdev(b->dev);
		rc = pdev->write(b->unit, b->data, b->blkno, BLKSIZE);
		if (rc != 0) {
			/* this is bad. it's up to the filesystem to deal with what 
			 * happens next.
			 */
			CS_LEAVE(&block_cs);
			return rc;
		}

		b->dirty = FALSE;
	}
	
	CS_LEAVE(&block_cs);
	return 0;
}

void bc_get_stats(bcstats_t *pstats)
{
	*pstats = stats;
}

/* Get a block if it's already in cache
 */
blkptr_t bc_cache_get(dev_t dev, unit_t unit, blkno_t blkno)
{
	blkptr_t blk;

	CS_ENTER(&block_cs);

	blk = blocklist;
	do {
		if (blk->dev == dev && blk->unit == unit && blk->blkno == blkno) {
			blk->inuse++;
			return blk;
		}
		blk = blk->next;
	} while (blk != blocklist);

	CS_LEAVE(&block_cs);

	return NULL;
}

/* Read from device
 */
int bc_device_read(dev_t dev, unit_t unit, blkno_t start, int blks, uint8_t __far *data)
{
	blkdev_t *pdev = dev_getbdev(dev);
	int bytes;

	while (blks) {
		bytes = pdev->read(unit, data, start, blks * BLKSIZE);
		if (bytes < 0) {
			return bytes;
		}

		if (bytes == 0) {
			return -EIO;
		}

		data += bytes;
		bytes /= BLKSIZE;
		blks -= bytes;
		start += bytes;
	}

	return 0;
}

/* Read a contiguous range of sectors
 */
int bc_read(dev_t dev, unit_t unit, blkno_t blkno, int blks, uint8_t __far *data)
{
	int rc = 0;
	int i;
	blkptr_t blk;
	blkno_t start;

	start = blkno;

	for (i = 0; i < blks; i++) {
		blk = bc_cache_get(dev, unit, blkno);
		if (blk) {
			if (blkno > start) {
				rc = bc_read(dev, unit, start, (int)(blkno-start), data);
				if (rc != 0) {
					bc_put(blk);
					return rc;
				}
				data += (blkno - start) * BLKSIZE;
				start = blkno;
			}
			_fmemcpy(data, blk->data, BLKSIZE);
			data += BLKSIZE;
			blkno++;
			rc = bc_put(blk);
			if (rc < 0) {
				return rc;
			}
			start = blkno;
			continue;
		}
	}

	if (blkno > start) {
		rc = bc_read(dev, unit, start, (int)(blkno-start), data);
	}

	return rc;
}
