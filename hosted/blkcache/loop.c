/*
 * File loopback device driver for a floppy disk
 */								
#include "blkdev.h"

#include <errno.h>
#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static int loop_init(void);
static int loop_get_units(void);
static int loop_read(unit_t unit, blkno_t blk, uint8_t __far *buffer);
static int loop_write(unit_t unit, blkno_t blk, uint8_t __far *buffer);

struct blkdev_t loop_dev = {
	&loop_init,
	&loop_get_units,
	&loop_read,
	&loop_write
};

typedef struct loop_unit_t loop_unit_t;
struct loop_unit_t {
    FILE *fp;
    unsigned blocks;
};

#define N_UNITS 4
static loop_unit_t units[N_UNITS];

#define BLK_SIZE 512ul
#define BLOCKS 2880ul

extern uint8_t bootsect[];

static uint8_t workblk[BLK_SIZE];

int loop_init(void)
{
    int i;

    for (i = 0; i < N_UNITS; i++) {
        units[i].fp = NULL;
    }
    return 0;
}

int loop_open(const char *name)
{
    int i;
    loop_unit_t *pu = NULL;
    long size;

    for (i = 0; i < N_UNITS; i++) {
        if (units[i].fp == NULL) {
            pu = &units[i];
            break;
        }
    }

    if (pu == NULL) {
        return -ENOMEM;
    }

    pu->fp = fopen(name, "r+b");
    if (pu->fp) {
        fseek(pu->fp, 0, SEEK_END);
        size = ftell(pu->fp);        
    } else {
        pu->fp = fopen(name, "w+b");
        if (!pu->fp) {
            return -ENOENT;
        }
        
        /* create a 1.44M disk */
        memset(workblk, 0xf6, BLK_SIZE);

        for (i = 0; i < BLOCKS; i++) {
            if (fwrite(i == 0 ? bootsect : workblk, BLK_SIZE, 1, pu->fp) != 1) {
                fclose(pu->fp);
                pu->fp = NULL;
                return -EIO;
            }
        }

        size = BLK_SIZE * BLOCKS;
    }

    fseek(pu->fp, 0, SEEK_SET);
    pu->blocks = (int)(size / BLK_SIZE);

    return pu - units;
}

int loop_get_units(void)
{
    return N_UNITS;
}

int loop_read(unit_t unit, blkno_t blk, uint8_t __far *buffer)
{
    loop_unit_t *pu;

    if (
        unit < 0 || 
        unit >= N_UNITS || 
        units[unit].fp == NULL || 
        blk >= unit[units].blocks
    ) {
        printf("E_INVAL\n");
        return -EINVAL;
    }

    pu = &units[unit];
    fseek(pu->fp, blk * BLK_SIZE, SEEK_SET);

    if (fread(workblk, BLK_SIZE, 1, pu->fp) != 1) {
        return -EIO;
    }

    _fmemcpy(buffer, workblk, BLK_SIZE);

    return 0;
}

int loop_write(unit_t unit, blkno_t blk, uint8_t __far *buffer)
{
    loop_unit_t *pu;

    if (
        unit < 0 || 
        unit >= N_UNITS || 
        units[unit].fp == NULL || 
        blk >= unit[units].blocks
    ) {
        return -EINVAL;
    }

    pu = &units[unit];
    fseek(pu->fp, blk * BLK_SIZE, SEEK_SET);

    _fmemcpy(workblk, buffer, BLK_SIZE);

    if (fwrite(workblk, BLK_SIZE, 1, pu->fp) != 1) {
        return -EIO;
    }

    return 0;
}

