/*
 * BIOS device driver for a floppy disk
 */								
#include "blkdev.h"
#include "int13.h"

#include <errno.h>
#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>

static int fdd_init(void);
static int fdd_get_units(void);
static int fdd_read(unit_t unit, blkno_t blk, uint8_t __far *buffer);
static int fdd_write(unit_t unit, blkno_t blk, uint8_t __far *buffer);

static int nunits = 0;

typedef struct fdd_unit_t fdd_unit_t;
struct fdd_unit_t {
	bool_t ok : 1;
	drvgeom_t geom;
};

static fdd_unit_t *units;

struct blkdev_t fdd_dev = {
	&fdd_init,
	&fdd_get_units,
	&fdd_read,
	&fdd_write
};

int fdd_init(void)
{
	int i;
	drvgeom_t geom;

	nunits = 0;

	if (bios_reset_drv(0) == -1) {
		printf("failed to reset FDD\n");
		return 0;

	}

	if (bios_drvgeom(0x00, &geom) == -1) {
		printf("FDD: no devices\n");
		return 0;
	}

	nunits = geom.drives;
	printf("FDD: there are %d FDD(s)\n", nunits);
	
	units = malloc(nunits * sizeof(fdd_unit_t));
	if (units == NULL) {
		return -ENOMEM;
	}

	units[0].geom = geom;
	units[0].ok = TRUE;
	
	for (i = 1; i < nunits; i++) {
		if (bios_drvgeom(i, &units[i].geom) == -1) {
			printf("FDD: unit %d failed init\n", i);
			units[i].ok = FALSE;
		} else {
			units[i].ok = TRUE;
		}
	}

	return 0; 	
}

int fdd_get_units(void)
{
	return nunits;
}

int fdd_read(unit_t unit, blkno_t blk, uint8_t __far * buffer)
{
	fdd_unit_t *pu;
	unsigned c, h, s;
	int r;
	blkno_t blksave = blk;

		
	if (unit >= nunits) {
		return -ENODEV;
	}

	pu = &units[unit];
	if (!pu->ok) {
		return -ENODEV;
	}

	/* TODO share this with HDD
	 */
	s = (unsigned)(1 + (blk % pu->geom.secs));
	blk /= pu->geom.secs;

	h = (unsigned)(blk % pu->geom.heads);
	blk /= pu->geom.heads;

	c = (unsigned)blk;

	r = bios_read_blk(unit, c, h, s, buffer);
	if (r < 0) {
		printf("FDD: read failed CHS=%u:%u:%u result=%d\n", c, h, s, -r);
		if (r == -I13_ERR_MEDIA_CHANGE) {
			printf("     media changed\n");
			return -EUCLEAN;
		}

		if (r == -I13_ERR_TIMEOUT) {
			printf("     no media\n");
			return -ENXIO;
		}
		
		return -EIO;
	}

	return 0;
}

int fdd_write(unit_t unit, blkno_t blk, uint8_t __far *buffer)
{
	fdd_unit_t *pu;
	unsigned c, h, s;
	int r;
	blkno_t blksave = blk;
		
	if (unit >= nunits) {
		return -ENODEV;
	}

	pu = &units[unit];
	if (!pu->ok) {
		return -ENODEV;
	}

	/* TODO share this with HDD
	 */
	s = (unsigned)(1 + (blk % pu->geom.secs));
	blk /= pu->geom.secs;

	h = (unsigned)(blk % pu->geom.heads);
	blk /= pu->geom.heads;

	c = (unsigned)blk;

	r = bios_write_blk(unit, c, h, s, buffer);
	if (r < 0) {
		printf("FDD: write failed CHS=%u:%u:%u result=%d\n", c, h, s, -r);
		return -EIO;
	}

	return 0;
}
