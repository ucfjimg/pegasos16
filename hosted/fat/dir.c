#include "fat.h"

#include <string.h>

#define CHECK_RET(expr) { int rc = (expr); if (rc) return rc; }

/* Search for ele of length len in the directory which starts at cluster on disk. If
 * path is NULL, return an unused entry if there is one.
 *
 * On success the directory entry and its location are returned in info.
 *
 * Return 0 on success
 * -ENOENT if any path element was not found in the directory or if the path is malformed
 * -EIO on I/O issues reading media
 * -ENOMEM on out of memory
 */
int dir_find_ent(fatvol_t *fv, char *path, int len, clusidx_t cluster, fileinfo_t *info)
{
    const int blksize = fv->bpb.bytes_per_sector;
    const int sec_per_clus = fv->bpb.sector_per_cluster; 
    const int clussize = blksize * sec_per_clus;

    int offs = 0;

    blkptr_t blk;
    int rc = 0;
    blkno_t sec;
    int entleft;
    char ele[12];

    ele[11] = 0;

    if (path != NULL) {
        rc = path_parse_fn(ele, path, len);
        if (rc != 0) {
            return -ENOENT;
        }
    } else {
        ele[0] = '\0';
    }
    
    if (cluster == 0) {
        sec = fv->rootdirsec;
        entleft = fv->bpb.root_entries; 
    } else {
        sec = fv->datasec + (cluster - 2) * sec_per_clus;
        entleft = clussize / sizeof(dirent_t);
    }

    CHECK_RET(bc_get(fv->dev, fv->unit, sec, &blk, 0));

    for(;;) {
        dirent_t __far *dirent = (dirent_t __far *)(blk->data + offs);

        if (path != NULL && dirent->name[0] == 0) {
            /* end of directory marker, and we're not looking for an empty slot
             */
			break;
        }

        /* 0xe5 in the first byte of the name indicates an erased entry
         */
        if (
            (path == NULL && (dirent->name[0] == 0 || (dirent->name[0] & 0xff) == DELETE_FLAG)) ||
            (path != NULL && _fmemcmp(dirent->name, ele, sizeof(dirent->name)) == 0)
            ) {
            info->sector = blk->blkno;
            info->secoffs = offs;
            info->dir = *dirent;

            CHECK_RET(bc_put(blk));
            return 0;
        }

        entleft--;
        if (entleft == 0) {
            if (cluster == 0) {
                /* end of root directory
                 */
                CHECK_RET(bc_put(blk));
                break;
            } else {
                rc = clus_get_next(fv, cluster, &cluster);
                if (rc != 0) {
                    bc_put(blk);
					return rc;
                }

                if (cluster >= 0xff0) {  /* TODO other FAT versions */
                    /* end of directory
                     */
					break;
                }

                CHECK_RET(bc_put(blk));

                sec = fv->datasec + (cluster - 2) * sec_per_clus;
                entleft = clussize / sizeof(dirent_t);
                offs = 0;
                CHECK_RET(bc_get(fv->dev, fv->unit, sec, &blk, 0));
                continue;
            }
        }

        offs += sizeof(dirent_t);
        if (offs == blksize) {
            offs = 0;
            sec++;
            CHECK_RET(bc_put(blk));
            CHECK_RET(bc_get(fv->dev, fv->unit, sec, &blk, 0));
        }
    }

    return -ENOENT;
}

/* find a rooted path in the filesystem by considering each path element
 * in turn.
 *
 * on success, returns the last element's directory entry in info 
 *
 * Return 0 on success
 * -ENOENT if any path element was not found in the directory or if the path is malformed
 * -EIO on I/O issues reading media
 * -ENOMEM on out of memory
 */
int dir_find_path(fatvol_t *fv, char *fn, fileinfo_t *info)
{
    int rc;
    substr_t ss = path_next_dir_ele(fn, 0);
    int cluster = 0;

    while (ss.len) {
        rc = dir_find_ent(fv, &fn[ss.start], ss.len, cluster, info);
        if (rc != 0) {
            return rc;
        }

        cluster = info->dir.cluster0;
        ss = path_next_dir_ele(fn, ss.start + ss.len);

        if (ss.len && (info->dir.attr & FA_DIR) == 0) {
            /* there is more to the path, but the path so far didn't
             * end in a directory.
             */
			return -ENOENT;
        }
    }

    return 0;
}

/* set the time and date in a directory entry
 *
 */
void dir_set_datetime(dirent_t __far *ent, time_t t)
{
#ifdef KERNEL
	UNREFERENCED(t);
	ent->modtime = 0;
	ent->moddate = 0;
#else
    struct tm *tm = localtime(&t);

    ent->modtime =
        (tm->tm_hour << 11) |
        (tm->tm_min << 5) |
        (tm->tm_sec >> 1);

    ent->moddate =
        (((tm->tm_year - 80) << 9) & 0xfe00) |
        ((tm->tm_mon + 1) << 5) |
        tm->tm_mday;
#endif
}

