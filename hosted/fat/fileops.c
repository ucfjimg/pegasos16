#include "fat.h"

#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>

#ifdef __TURBOC__
# define O_ACCMODE 0x07
#elif defined(_MSC_VER) && (_MSC_VER <= 800)
# define O_ACCMODE 0x03
#endif

static void fillstat(fatvol_t *fv, fileinfo_t *info, struct stat *st);
static int unused_fd(fatvol_t *fv);
static openfile_t *openfile_by_fd(fatvol_t *fv, int fd);
static void create_dot_dirs(uint8_t __far *blk, clusidx_t curr, clusidx_t parent, uint16_t modtime, uint16_t moddate);



#define CHECK_RET(expr) { int rc = (expr); if (rc) return rc; } 

/* Mount a block device as a FAT file system.
 *
 * Returns 0 on success
 * -EBUSY if a filesystem is already mounted
 * -EIO if the image could not be read or is not proper filesystem
 * -ENOMEM on out of memory
 */ 
int f_mount(dev_t dev, unit_t unit, fatvol_t *fv)
{
    blkptr_t b;

    memset(fv, 0, sizeof(fatvol_t));

	fv->dev = dev;
	fv->unit = unit;

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
int f_umount(fatvol_t *fv)
{
    memset(fv, 0, sizeof(*fv));
    return 0;
}

/* Return information about the given file
 *
 * Return 0 on success
 * -ENOENT if any path element was not found in the directory or if the path is malformed
 * -EIO on I/O issues reading media
 * -ENOMEM on out of memory
 */
int f_stat(fatvol_t *fv, char *fn, struct stat *st)
{
    fileinfo_t info;

    CHECK_RET(dir_find_path(fv, fn, &info));
    fillstat(fv, &info, st);
    
    return 0;
}

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
    size_t got = 0;
	openfile_t *of = openfile_by_fd(fv, fd);

    if (!of) {
        return -EINVAL;
    }

    /* check for overflow on offset
     */
    if (of->offset + size < of->offset) {
		return -EINVAL;
    }

    /* clamp read to how much data is in file
     */
    if (of->offset + size > of->info.dir.size) {
        size = (size_t)(of->info.dir.size - of->offset);
    }

 

    while (size) {
        blkptr_t blk;
		size_t dataoffs;
		size_t datalen;
        
		CHECK_RET(fobj_get_sec_at_offs(fv, &of->info, of->offset, &blk, 0));

        dataoffs = (size_t)(of->offset % fv->bpb.bytes_per_sector);
        datalen = (size_t)(fv->bpb.bytes_per_sector - dataoffs);
        if (datalen > size) {
            datalen = size;
        }
        _fmemcpy(buffer, blk->data + dataoffs, datalen);
        CHECK_RET(bc_put(blk));

        buffer += datalen;
        size -= datalen;
        got += datalen;
        of->offset += datalen;
    }

    return got;
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
        fprintf(stderr, "overflow\n");
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

/* create a new special file at fn
 */
int f_mknod(fatvol_t *fv, char *fn, uint8_t major, uint8_t minor)
{
    blkptr_t objblk = NULL;
	
	dirent_t __far *ent;
    fileinfo_t info;

    CHECK_RET(fobj_create(fv, fn, FA_DEV, &info, NULL));

    /* the sector containing the new object
     */
    CHECK_RET(bc_get(fv->dev, fv->unit, info.sector, &objblk, 0));
    ent = (dirent_t __far *)(objblk->data + info.secoffs);

    ent->major = major;
    ent->minor = minor;

    objblk->dirty = 1;

    return bc_put(objblk);
}

/* Given a file info struct, populate a stat struct
 */
void fillstat(fatvol_t *fv, fileinfo_t *info, struct stat *st)
{
#ifndef NO_STAT_EXTRAS
    const int by_per_clus = fv->bpb.bytes_per_sector * fv->bpb.sector_per_cluster;
    clusidx_t clusters = (info->dir.size + by_per_clus - 1) / by_per_clus;
#endif

#ifndef KERNEL
    struct tm tm;
#endif

	time_t tt = 0;

	fv = fv;

    memset(st, 0, sizeof(*st));

    st->st_ino = (ino_t)info->dir.cluster0;
    st->st_nlink = 1;
    st->st_size = info->dir.size;
    
#ifndef NO_STAT_EXTRAS
	st->st_blksize = fv->bpb.bytes_per_sector;
    st->st_blocks = clusters * fv->bpb.sector_per_cluster;
#endif

#ifndef KERNEL
    memset(&tm, 0, sizeof(tm));

    tm.tm_hour = info->dir.modtime >> 11;
    tm.tm_min = (info->dir.modtime >> 5) & 0x3f;
    tm.tm_sec = (info->dir.modtime & 0x1f) << 1;
    tm.tm_year = (info->dir.moddate >> 9) + 1980 - 1900;
    tm.tm_mon = ((info->dir.moddate >> 5) & 0x0f) - 1;
    tm.tm_mday = info->dir.moddate & 0x1f;

	tt = mktime(&tm);
#endif

#ifdef NO_STAT_EXTRAS
	st->st_mtime = tt;
#else
    st->st_mtim.tv_sec = tt;
#endif
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

