#include "fatfs.h"

#include <errno.h>

#define CHECK_RET(expr) { int rc = (expr); if (rc) return rc; } 


/* given a cluster, return the next cluster past `cluster`
 *  in the chain in `next`
 *
 * Returns 0 on success
 * -EIO on failure to read FAT
 * -ENOMEM on out of memory
 */ 
int clus_get_next(fatvol_t *fv, clusidx_t cluster, clusidx_t *next)
{
    const long offs = (cluster * 3) / 2;
    const int blksize = fv->bpb.bytes_per_sector;
    
    blkno_t sec0 = fv->fat0sec + offs / blksize;
    blkno_t sec1 = fv->fat0sec + (offs + 1) / blksize;

    unsigned low, high;
    blkptr_t blk;

    CHECK_RET(bc_get(fv->dev, fv->unit, sec0, &blk, 0));

    low = blk->data[offs % blksize];

    if (sec1 != sec0) {
        CHECK_RET(bc_put(blk));
        CHECK_RET(bc_get(fv->dev, fv->unit, sec1, &blk, 0));
    }

    high = blk->data[(offs + 1) % blksize];
    CHECK_RET(bc_put(blk));

    if (cluster & 1) {
        *next = ((low >> 4) & 0x0F) | (high << 4);
    } else {
        *next = ((high & 0x0F) << 8) | low;
    }

    return 0;
}

#if 0
/* set the next pointer ot the cluster at `curr` to `next` in
 * all copies of the FAT.
 */
int clus_set_next(fatvol_t *fv, clusidx_t curr, clusidx_t next)
{
    const int blksize = fv->bpb.bytes_per_sector;
    const long offs = (curr * 3) / 2;

    blkno_t sec0 = fv->fat0sec + offs / blksize;
    blkno_t sec1 = fv->fat0sec + (offs + 1) / blksize;

    blkptr_t blk;
    uint8_t b;
    int i;

    /* TODO things which read the FAT should check both FAT's if something fails
     */
    for (i = 0; i < fv->bpb.fat_count; i++) {
        /* TODO we might want to continue here. There are two FAT's for redundancy.
         */
		CHECK_RET(bc_get(fv->dev, fv->unit, sec0, &blk, 0));

        if (curr & 1) {
            b = blk->data[offs];
            b &= 0x0F;
            b |= ((next << 4) & 0xF0);
            blk->data[offs] = b;
        } else {
            blk->data[offs] = (uint8_t)(next & 0xFF);
        }

        blk->dirty = 1;

        if (sec1 != sec0) {
            CHECK_RET(bc_put(blk));
            CHECK_RET(bc_get(fv->dev, fv->unit, sec1, &blk, 0));
        }

        if (curr & 1) {
            blk->data[offs + 1] = (uint8_t)((next >> 4) & 0xFF);
        } else {
            b = blk->data[offs + 1];
            b &= 0xF0;
            b |= ((next >> 8) & 0x0F);
            blk->data[offs + 1] = b;
        }

        blk->dirty = 1;
        CHECK_RET(bc_put(blk));

        sec0 += fv->bpb.sectors_per_fat;
        sec1 += fv->bpb.sectors_per_fat;
    }
    
    return 0;
}

/* allocate a cluster and mark it as the end of a chain 
 *
 * Returns 0 on success (cluster index in clus)
 * -ENOSPC if disk is full
 * -EIO on error reading or writing
 */
int clus_alloc(fatvol_t *fv, clusidx_t *clus)
{
    const int blksize = fv->bpb.bytes_per_sector;

    blkptr_t blk = NULL;
    clusidx_t c, next;

    unsigned low, high;

    for (c = 2; c < fv->n_clus; c++) {
        const long offs = (c * 3) / 2;
        const blkno_t sec0 = fv->fat0sec + offs / blksize;
        const blkno_t sec1 = fv->fat0sec + (offs + 1) / blksize;

        if (blk == NULL || blk->blkno != sec0) {
            if (blk) {
                CHECK_RET(bc_put(blk));
            }
            CHECK_RET(bc_get(fv->dev, fv->unit, sec0, &blk, 0));
        }

        low = blk->data[offs % blksize];

        if (sec1 != sec0) {
            CHECK_RET(bc_put(blk));
            CHECK_RET(bc_get(fv->dev, fv->unit, sec1, &blk, 0));
        }

        high = blk->data[(offs + 1) % blksize];

        if (c & 1) {
            next = ((low >> 4) & 0x0F) | (high << 4);
        } else {
            next = ((high & 0x0F) << 8) | low;
        }

        if (next == 0) {
            *clus = c;
            CHECK_RET(bc_put(blk));
            CHECK_RET(clus_set_next(fv, c, 0xfff));     /* TODO FAT12-ism */

            return 0;
        }
    }

    return -ENOSPC;
}









#endif
