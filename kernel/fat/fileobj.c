#include "fatfs.h"

#include <errno.h>
#include <string.h>
#include <time.h>

#define CHECK_RET(expr) { int rc = (expr); if (rc) { return rc; } }
		
/* create a new file system object at fn with attributes attr. on successful
 * return, info describes the new file entry, and if parent is not NULL, it 
 * will contain the cluster of the parent directory.
 *
 * Returns 0 on success
 * -EINVAL if the fn is longer than F_MAX_PATH or if any element is invalid
 * -ENOENT if any element of the parent directory does not exist
 * -EIO on any disk error
 * -ENOMEM on out of memory
 */
int fobj_create(fatvol_t *fv, const char *fn, uint8_t attr, fileinfo_t *info, clusidx_t *parent)
{
    int clus = 0;

    char path[F_MAX_PATH+1];
    char ele[11];    /* TODO should have  #define for this */
    char *dir;
    char *name;
    int rc;
    blkptr_t blk;
    dirent_t __far *ent;

    if (strlen(fn) > F_MAX_PATH) {
		return -EINVAL;
    }
    strcpy(path, fn);

    path_split(path, &dir, &name);

    rc = path_parse_fn(ele, name, strlen(name));
    if (rc != 0) {
		return rc;
    }

    /* find the parent directory
     */
    clus = 0;
    if (dir[0]) {
        CHECK_RET(dir_find_path(fv, dir, info));

        if ((info->dir.attr & FA_DIR) == 0) {
            return -ENOENT;
        }

        clus = info->dir.cluster0;
    }

    if (parent) {
        *parent = clus;
    }

    /* check to see if the object already exists
     */
    rc = dir_find_ent(fv, name, strlen(name), clus, info);
    if (rc != -ENOENT) {
        if (rc == 0) {
            return -EEXIST;
        }
		return rc;
    }

    /* find a free directory entry in that parent
     */
    CHECK_RET(dir_find_ent(fv, NULL, 0, clus, info));

    /* Create the directory entry
     */
    CHECK_RET(bc_get(fv->dev, fv->unit, FAT_INO_SEC(info->ino), &blk, 0));

	ent = (dirent_t __far *)(blk->data + FAT_INO_OFFS(info->ino));

    _fmemcpy(ent->name, ele, sizeof(ent->name));
    ent->attr = attr;
    _fmemset(ent->reserved, 0, sizeof(ent->reserved));

#ifdef KERNEL
    dir_set_datetime(ent, 0);
#else
    dir_set_datetime(ent, time(NULL));
#endif

    info->dir = *ent;
    blk->dirty = 1;
	CHECK_RET(bc_put(blk));
	return 0;
}

/* Given a file's starting cluster, and an offset withing the file, return
 * the sector containing the offset.
 *
 * The offset of the data within the sector is simply the offset mod
 * bytes per sector.
 *
 * Returns 0 on success
 * -EIO on I/O error, or filesystem corrupt (past end of cluster chain)
 * -ENOMEM on out of memory
 */ 
int fobj_get_sec_at_offs(
	fatvol_t *fv, clusidx_t clus0, long offs, blkptr_t *blk, int overw
)
{
    const int blksize = fv->bpb.bytes_per_sector;
    const int bytes_per_cluster = blksize * fv->bpb.sector_per_cluster;
    
    clusidx_t clus = offs / bytes_per_cluster;
	clusidx_t ci;
	blkno_t sec;
	
	offs -= clus * bytes_per_cluster;
    sec = offs / blksize;

    ci = clus0;

    while (clus--) {
        CHECK_RET(clus_get_next(fv, ci, &ci));
        if (ci >= 0xff0) { /* TODO FATx */ 
            /* premature EOF
             */
            return -EIO; /* really, a corrupt filesystem */
        }
    }

    sec += fv->datasec + (ci - 2) * fv->bpb.sector_per_cluster;    
    return bc_get(fv->dev, fv->unit, sec, blk, overw ? F_BCGET_OVERW : 0);
}


#if 0
/* truncate the file in 'info' to length 'length'. this can either
 * shorten or lengthen the file.
 *
 * Returns 0 on success
 *
 */
int fobj_trunc(fatvol_t *fv, fileinfo_t *info, uint32_t length)
{
    const int by_per_clus = fv->bpb.bytes_per_sector * fv->bpb.sector_per_cluster;
    
    clusidx_t target = (length + by_per_clus - 1) / by_per_clus;
    clusidx_t n = 0;
    clusidx_t p = 0;
    clusidx_t c = info->dir.cluster0;
    
    blkptr_t dirblk;
    dirent_t __far *dir;

    if (c != 0) {        
        while (target > n && c < 0xff0) {
            p = c;
            CHECK_RET(clus_get_next(fv, p, &c));
            n++;
        }
    }

    /* need to grow file?
     */
    if (n < target) {
        while (n < target) {
            /* TODO if this fails for space, we should go back and clean up
             */
            CHECK_RET(clus_alloc(fv, &c));
            if (p == 0) {
                info->dir.cluster0 = (uint16_t)c;
            } else {
                CHECK_RET(clus_set_next(fv, p, c));
            }
            p = c;
            n++;
        }
        CHECK_RET(clus_set_next(fv, p, 0xfff)); /* TODO FAT12 */
    } else if (c != 0) {
        /* need to shrink file
         */
        for(;;) { 
            if (p == 0) {
                info->dir.cluster0 = 0;
            } else {
                CHECK_RET(clus_set_next(fv, p, 0));
            }
            if (c >= 0xff0) {  /* TODO FAT12 */
                break;
            }
            p = c;
            CHECK_RET(clus_get_next(fv, p, &c));
        }
    }

    info->dir.size = length;

    CHECK_RET(bc_get(fv->dev, fv->unit, info->sector, &dirblk, 0));
    dir = (dirent_t __far *)(dirblk->data + info->secoffs);
    dir->size = length;
    dir->cluster0 = info->dir.cluster0;
    dirblk->dirty = 1;
    
	CHECK_RET(bc_put(dirblk));

    return 0;
}

#endif

