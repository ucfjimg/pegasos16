#include "fatfs.h"

#include "device.h"
#include "printk.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>

#ifdef __TURBOC__
# define O_ACCMODE 0x07
#elif defined(_MSC_VER) && (_MSC_VER <= 800)
# define O_ACCMODE 0x03
#endif

static void fillstat(fatvol_t *fv, fileinfo_t *info, struct stat *st);


#if 0
static int unused_fd(fatvol_t *fv);
static openfile_t *openfile_by_fd(fatvol_t *fv, int fd);
static void create_dot_dirs(uint8_t __far *blk, clusidx_t curr, clusidx_t parent, uint16_t modtime, uint16_t moddate);
#endif

#define CHECK_RET(expr) { int rc = (expr); if (rc) return rc; } 

/* Mount a block device as a FAT file system.
 *
 * Returns 0 on success
 * -EBUSY if a filesystem is already mounted
 * -EIO if the image could not be read or is not proper filesystem
 * -ENOMEM on out of memory
 */ 
int fat_mount(dev_t dev, unit_t unit, fatvol_t *fv)
{
    blkptr_t b;

	memset(fv, 0, sizeof(fatvol_t));

	fv->dev = dev;
	fv->unit = unit;

	UNREFERENCED(b);
    CHECK_RET(bc_get(fv->dev, fv->unit, 0, &b, 0));
    _fmemcpy(&fv->bpb, b->data + 3, sizeof(fv->bpb));
    CHECK_RET(bc_put(b));
   
	fv->fat0sec = fv->bpb.hidden_sectors + fv->bpb.reserved_sectors;
    fv->rootdirsec = fv->fat0sec + fv->bpb.fat_count * fv->bpb.sectors_per_fat;
    fv->datasec = fv->rootdirsec + (fv->bpb.root_entries * sizeof(dirent_t) + BLOCK_SIZE - 1) / BLOCK_SIZE;
    fv->n_clus = 2 + (fv->bpb.total_sectors - fv->datasec) / fv->bpb.sector_per_cluster;
    
	return 0;
}

/* Unmount the mounted file system
 */ 
int fat_umount(fatvol_t *fv)
{
    memset(fv, 0, sizeof(*fv));
    return 0;
}

/* Find an inode
 */
ino_t fat_ino_get(fatvol_t *fv, char *path, int flags)
{
    fileinfo_t info;
	int rc;

    rc = dir_find_path(fv, path, &info);

    /* nobody but the FS gets to write to a directory object */
    if (rc == 0 && (info.dir.attr & FA_DIR) != 0 && (flags & O_ACCESS) != O_RDONLY) {
        return (ino_t)-EACCES;
    }

    if (rc != 0) {
        if (rc == -ENOENT && ((flags & O_CREAT) != 0)) {
            rc = fobj_create(fv, path, 0, &info, NULL);
            if (rc != 0) {
				return (ino_t)rc;
            }
        } else {
			return (ino_t)rc;
        }
    }

	return info.ino;	
}	

/* Return information about the given file
 *
 * Return 0 on success
 * -ENOENT if any path element was not found in the directory or if the path is malformed
 * -EIO on I/O issues reading media
 * -ENOMEM on out of memory
 */
int fat_stat(fatvol_t *fv, ino_t ino, struct stat *st)
{
    fileinfo_t info;
	blkno_t sec = FAT_INO_SEC(ino);
	int offs = FAT_INO_OFFS(ino);
	blkptr_t blk;
	

    CHECK_RET(bc_get(fv->dev, fv->unit, sec, &blk, 0));

	info.ino = ino;
	info.dir = *(dirent_t __far *)(blk->data + offs);
	
    fillstat(fv, &info, st);
    
    return 0;
}

static size_t read_root_dir(fatvol_t *fv, uint8_t __far *buf, off_t off, size_t size)
{
    blkno_t sec0 = fv->rootdirsec;
    blkno_t secn = fv->datasec;
    const int blksize = fv->bpb.bytes_per_sector;
    size_t got = 0;
	blkptr_t blk;

    sec0 += off / blksize;
    
    while (size && sec0 < secn) {
		size_t dataoffs;
		size_t datalen;

        CHECK_RET(bc_get(fv->dev, fv->unit, sec0, &blk, 0));

        dataoffs = (size_t)(off % blksize);
        datalen = (size_t)(blksize - dataoffs);
        if (datalen > size) {
            datalen = size;
        }

        _fmemcpy(buf, blk->data + dataoffs, datalen);
        CHECK_RET(bc_put(blk));

        buf += datalen;
        size -= datalen;
        got += datalen;
        sec0++;
    }

    return got;
}

size_t fat_read(fatvol_t *fv, ino_t ino, uint8_t __far *buf, off_t off, size_t size)
{
    const int blksize = fv->bpb.bytes_per_sector;
    const int bytes_per_cluster = blksize * fv->bpb.sector_per_cluster;

    int rc;
    int rc2;
	dirent_t __far *dir;
    size_t got = 0;
    blkptr_t dirblk;
	blkptr_t blk;
    clusidx_t clcnt, ci;
    blkno_t sec;
    blkno_t secleft;

    /* check for overflow on offset
     */
    if (off + size < off) {
		return (size_t)-EINVAL;
    }

    if (ino == 0) {
        /* a read from the root directory 
         */
        return read_root_dir(fv, buf, off, size);
    } 
    
    rc = bc_get(fv->dev, fv->unit, FAT_INO_SEC(ino), &dirblk, 0);
    if (rc < 0) {
        return rc;
    }
    dir = (dirent_t __far *)(dirblk->data + FAT_INO_OFFS(ino));

    /* clamp read to how much data is in file
     */
    if ((uint32_t)(off + size) > dir->size) {
        size = (size_t)(dir->size - off);
    }

    clcnt = off / bytes_per_cluster;
    ci = dir->cluster0;

    off -= clcnt * bytes_per_cluster;
    sec = off / blksize; 
    secleft = fv->bpb.sector_per_cluster - sec;

    while (clcnt--) {
        rc = clus_get_next(fv, ci, &ci);
        if (rc < 0) {
            goto fail;
        }
        /* TODO FAT12
         */
        if (ci >= 0xff0) {
            rc = -EIO;
            goto fail;
        }
    }

    sec += fv->datasec + (ci - 2) * fv->bpb.sector_per_cluster;

    while (size) {
		size_t dataoffs;
		size_t datalen;

#if 0
        if (dataoffs == 0) {
            size_t left = size;
            int readsecs = secleft;
            left -= readsecs * fv->bpb.bytes_per_sector;

            while (left >= bytes_per_cluster) {
                rc = clus_get_next(fv, ci, &cn);
                if (rc < 0) {
                    goto fail;
                }


            }

        }
#endif

        rc = bc_get(fv->dev, fv->unit, sec, &blk, 0);
        if (rc < 0) {
            goto fail;
        }

        dataoffs = (size_t)(off % fv->bpb.bytes_per_sector);
        datalen = (size_t)(fv->bpb.bytes_per_sector - dataoffs);
        if (datalen > size) {
            datalen = size;
        }

        _fmemcpy(buf, blk->data + dataoffs, datalen);
        rc = bc_put(blk);
        if (rc < 0) {
            goto fail;
        }

        if (--secleft) {
            sec++;
        } else {
            rc = clus_get_next(fv, ci, &ci);
            if (rc < 0) {
                goto fail;
            }

            sec = fv->datasec + (ci - 2) * fv->bpb.sector_per_cluster;
            secleft = fv->bpb.sector_per_cluster;
        }

        buf += datalen;
        size -= datalen;
        got += datalen;
		off += datalen;
    }

fail:
    rc2 = bc_put(dirblk);
    if (rc) {
        return rc;
    } else if (rc2) {
        return rc2;
    }
    
    return got;
}



/* Given a file info struct, populate a stat struct
 */
void fillstat(fatvol_t *fv, fileinfo_t *info, struct stat *st)
{
    memset(st, 0, sizeof(*st));

    st->st_ino = info->ino;
	st->st_mtime = 0;
    st->st_nlink = 1;
    st->st_size = info->dir.size;
	st->st_blksize = fv->bpb.bytes_per_sector;

    st->st_mode = S_IFREG;
    if (info->dir.attr & FA_DIR) {
        st->st_mode = S_IFDIR;
    } else if (info->dir.attr & FA_DEV) {
        uint8_t mjr = info->dir.major;
        if (ISBDEV(mjr)) {
            st->st_mode = S_IFBLK;
        } else {
            st->st_mode = S_IFCHR;
        }

        st->st_rdev = S_RDEV(mjr, info->dir.minor);
    }
}











#if 0
/* Create a file and open it for write; if it already exists, open it and
 * truncate it to 0 length.
 *
 * Returns 0 success.
 */
int f_creat(fatvol_t *fv, char *fn)
{
    return f_open(fv, fn, O_CREAT|O_WRONLY|O_TRUNC);
}

/* O_ACCMODE
 * O_RDONLY
 * O_WRONLY
 * O_RDWR
 * O_CREAT - create if not exist
 * O_TRUNC - trunc if exists
 */
int f_open(fatvol_t *fv, char *fn, int flags)
{
    fileinfo_t info;
	int acc;
	int fd;
	int rc;
    openfile_t *of;

    acc = flags & O_ACCMODE;
    if (acc != O_RDONLY && acc != O_WRONLY && acc != O_RDWR) {
		return -EINVAL;
    }

    fd = unused_fd(fv);
    if (fd < 0) {
		return fd;
    }

    rc = dir_find_path(fv, fn, &info);
    if (rc != 0) {
        if (rc == -ENOENT && ((flags & O_CREAT) != 0)) {
            rc = fobj_create(fv, fn, 0, &info, NULL);
            if (rc != 0) {
				return rc;
            }
        } else {
			return rc;
        }
    }

    if (acc != O_RDONLY && ((flags & O_TRUNC) != 0)) {
        rc = fobj_trunc(fv, &info, 0);
        if (rc != 0) {
			return rc;
        }
    }

    of = &fv->openfiles[fd];
    of->open = 1;
    of->flags = flags;
    of->info = info;
    of->offset = 0;

    return fd;
}

/* Close the file given by fd
 *
 * Returns 0 on success
 * -EINVAL if the file descriptor is invalid
 */
int f_close(fatvol_t *fv, int fd)
{
    openfile_t *of = openfile_by_fd(fv, fd);
    if (!of) {
		return -EINVAL;
    }

    of->open = 0;

    return 0;
}

/* Return information about the given open file
 *
 * Return 0 on success
 * -EINVAL if the given descriptor was invalid
 */
int f_fstat(fatvol_t *fv, int fd, struct stat *st)
{
    openfile_t *of = openfile_by_fd(fv, fd);
    if (!of) {
        return -EINVAL;
    }

    fillstat(fv, &of->info, st);
    
    return 0;
}

/* Read size bytes from fd into buffer. 
 * 
 * Returns the number of bytes actually read, which may be less than size
 * if end of file is reached. A return of 0 indicates end of file.
 *
 * -EINVAL if the file descriptor is invalid or if size is large enough to cause arithmatic overflow.
 * -EIO on I/O error
 * -ENOMEM on out of memory
 */
ssize_t f_read(fatvol_t *fv, int fd, char __far *buffer, size_t size)
{
}

/* Write size bytes from the buffer to fd. 
 * 
 * Returns the number of bytes actually written, which may be less than size
 * if end of file is reached. A return of 0 indicates end of file.
 *
 * -EINVAL if the file descriptor is invalid or if size is large enough to cause arithmatic overflow.
 * -EIO on I/O error
 * -ENOMEM on out of memory
 * -ENOSPC if the disk is full
 */
ssize_t f_write(fatvol_t *fv, int fd, char __far *buffer, size_t size)
{
    size_t wrote = 0;
    openfile_t *of = openfile_by_fd(fv, fd);
	int overw;

    if (!of) {
        return -EINVAL;
    }

    /* check for overflow on offset
     */
    if (of->offset + size < of->offset) {
        return -EINVAL;
    }

    /* if this write will grow the file, pick up backing clusters first
     */
    if (of->offset + size > of->info.dir.size) {
        CHECK_RET(fobj_trunc(fv, &of->info, of->offset + size));
    }


    while (size) {
        blkptr_t blk;
		size_t dataoffs;
		size_t datalen;


        dataoffs = (size_t)(of->offset % fv->bpb.bytes_per_sector);
        datalen = fv->bpb.bytes_per_sector - dataoffs;
        
		if (datalen > size) {
            datalen = size;
        }

		overw = (dataoffs == 0) && (datalen == fv->bpb.bytes_per_sector);

        CHECK_RET(fobj_get_sec_at_offs(fv, &of->info, of->offset, &blk, overw));

        _fmemcpy(blk->data + dataoffs, buffer, datalen);
        blk->dirty = 1;
        CHECK_RET(bc_put(blk));
 
        buffer += datalen;
        size -= datalen;
        wrote += datalen;
        of->offset += datalen;
    }

    return wrote;
}

/* Set the file pointer in the given file, and return the new value
 */
off_t f_lseek(fatvol_t *fv, int fd, off_t offset, int whence)
{
    openfile_t *of = openfile_by_fd(fv, fd);
    if (!of) {
        return -EINVAL;
    }

    switch (whence) {
    case SEEK_SET:
        of->offset = offset;
        break;

    case SEEK_CUR:
    /* TODO check wrap */
        of->offset += offset;
        break;

    case SEEK_END:
    /* TODO check wrap */
        of->offset = of->info.dir.size + offset;
        break;
    }

    return of->offset;
}

/* Creat a new directory at fn
 */
int f_mkdir(fatvol_t *fv, char *fn)
{
    blkptr_t objblk = NULL;
	
	dirent_t __far *ent;
    fileinfo_t info;
	int rc;
	blkno_t sec;
	int i;
    clusidx_t entclus;

    CHECK_RET(fobj_create(fv, fn, FA_DIR, &info, NULL));

    /* the sector containing the new object
     */
    CHECK_RET(bc_get(fv->dev, fv->unit, info.sector, &objblk, 0));

    ent = (dirent_t __far *)(objblk->data + info.secoffs);

    /* add a cluster for the . and .. entries
     */
    rc = clus_alloc(fv, &entclus);
    sec = fv->datasec + (entclus - 2) * fv->bpb.sector_per_cluster;

    for (i = 0; rc == 0 && i < fv->bpb.sector_per_cluster; i++, sec++) {
        blkptr_t dirblk;
        rc = bc_get(fv->dev, fv->unit, sec + i, &dirblk, 0);
        if (rc == 0) {
            _fmemset(dirblk->data, 0, fv->bpb.bytes_per_sector);

            if (i == 0) {
                create_dot_dirs(dirblk->data, entclus, info.dir.cluster0, ent->modtime, ent->moddate);
            }

            dirblk->dirty = 1;
            rc = bc_put(dirblk);
        }
    }

    if (rc == 0) {
        ent->cluster0 = (uint16_t)entclus;
    } else {
        ent->name[0] = DELETE_FLAG;
    }

    objblk->dirty = 1;

    if (rc != 0) {
        bc_put(objblk);
    } else {
        rc = bc_put(objblk);
    }
    return rc;
}



/* Return a free file descriptor or -ENFILE if the file table is full
 */
int unused_fd(fatvol_t *fv)
{
	int i;

    for (i = 0; i < MAX_OPENFILES; i++) {
        if (fv->openfiles[i].open == 0) {
            return i;
        }
    }

    return -ENFILE;
}

/* return the openfile referenced by fd, or NULL if fd isn't
 * valid
 */
openfile_t *openfile_by_fd(fatvol_t *fv, int fd)
{
	openfile_t *of;
    
	if (fd < 0 || fd >= MAX_OPENFILES) {
        return NULL;
    }

    of = &fv->openfiles[fd];
    if (!of->open) {
        return NULL;
    }

    return of;
}

/* Create the . and .. entries in a new directory
 */
void create_dot_dirs(uint8_t __far *blk, clusidx_t curr, clusidx_t parent, uint16_t modtime, uint16_t moddate)
{
    dirent_t __far *dot = (dirent_t __far *)blk;
    _fmemset(dot->name, ' ', sizeof(dot->name));
    
    dot->name[0] = '.';
    dot->attr = FA_DIR;
    dot->modtime = modtime;
    dot->moddate = moddate;
    dot->cluster0 = (uint16_t)curr;
    dot->size = 0;

    dot[1] = dot[0];
    dot++;

    dot->name[1] = '.';
    dot->cluster0 = (uint16_t)parent;
}


















#endif
