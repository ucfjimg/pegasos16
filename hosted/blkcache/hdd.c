/*
 * BIOS device driver for a hard disk
 */
#include "blkdev.h"
#include "int13.h"

#include <dos.h>
#include <stdio.h>

#define BIOS_SEG 0x0040
#define BIOS_NHDD 0x0075

static int hdd_init(void);
static int hdd_get_units(void);
static int hdd_read(unit_t unit, blkno_t blk, uint8_t far *buffer);
static int hdd_write(unit_t unit, blkno_t blk, uint8_t far *buffer);

struct blkdev_t hdd_dev = {
	&hdd_init,
	&hdd_get_units,
	&hdd_read,
	&hdd_write
};

int hdd_init(void)
{
	drvgeom_t geom;

	uint8_t far *pndrive = MK_FP(BIOS_SEG, BIOS_NHDD);
	printf("HDD: there are %d HDD(s)\n", *pndrive);

	if (bios_drvgeom(0x80, &geom) == -1) {
		printf("failed\n");
	}
	return 0;
}

int hdd_get_units(void)
{
	/* TODO
     */
	return 0;
}

int hdd_read(unit_t unit, blkno_t blk, uint8_t far *buffer)
{
	unit = unit;
	blk = blk;
	buffer = buffer;
	
	return 0;
}

int hdd_write(unit_t unit, blkno_t blk, uint8_t far *buffer)
{
	unit = unit;
	blk = blk;
	buffer = buffer;
	
	return 0;
}
