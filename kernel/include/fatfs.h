#ifndef FATFS_H_
#define FATFS_H_

#if defined(__TURBOC__) || (defined(_MSC_VER) && (_MSC_VER <= 800))
# define NO_UNISTD
# define NO_STAT_EXTRAS
#endif

#include "bpb.h"
#include "fs.h"
#include "ktypes.h"
#include "blkcache.h"
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#define DELETE_FLAG ((uint8_t)0xe5)
#define F_MAX_PATH 128

typedef int ssize_t;
typedef uint32_t clusidx_t;

#define BLOCK_SIZE 512
#define DIRENT_SHIFT 5


/* Our inode is a 32-bit number representing the 
 * sector and index w/in the sector of a directory
 * entry. There are 16 (4 bits) directory entries
 * per 512-byte block. This limits our theoretical
 * volume size to 128G.
 */
#define FAT_INO(sec, doffs) (ino_t)(((sec) << 4) | (((doffs) >> 5) & 0xf))
#define FAT_INO_SEC(ino)    (blkno_t)(((ino) >> 4) & 0x0FFFFFFFul)
#define FAT_INO_OFFS(ino)   (((unsigned)((ino) & 0x0F)) << DIRENT_SHIFT)  




#if defined(_MSC_VER) && !defined(PACK1)
# pragma pack(1)
# pragma warning( disable: 4121 )
#endif

typedef struct dirent_t dirent_t;

struct dirent_t {
    char name[11];                      /* 8.3 name without the '.' */
    uint8_t attr;                       /* attributes */
    uint8_t major;                      /* extension: major and minor of device if */
    uint8_t minor;                      /* FA_DEV set */
    uint8_t reserved[8];                /* should be zero */
    uint16_t modtime;                   /* last modification time */
    uint16_t moddate;                   /* last modification date */
    uint16_t cluster0;                  /* starting cluster */
    uint32_t size;                      /* length in bytes */
} PACKED;								 

#if defined(_MSC_VER) && !defined(PACK1)
# pragma pack(2)
#endif

#define FA_READONLY 0x01
#define FA_HIDDEN   0x02
#define FA_SYSTEM   0x04
#define FA_VOLSER   0x08
#define FA_DIR      0x10
#define FA_ARCHIVE  0x20
#define FA_DEV      0x40

typedef struct substr_t substr_t;

struct substr_t {
    int start;
    int len;
};

typedef struct fileinfo_t fileinfo_t;

struct fileinfo_t {
	ino_t ino;	
    dirent_t dir;
};

typedef struct openfile_t openfile_t;

struct openfile_t {
    int open;
    unsigned flags;
    fileinfo_t info;
    unsigned long offset;
};

typedef struct fatvol_t fatvol_t;

struct fatvol_t {
	dev_t dev;
	unit_t unit;
    bpb_t bpb;
    blkno_t fat0sec;
    blkno_t rootdirsec;
    blkno_t datasec;
    clusidx_t n_clus;
};

extern int fat_mount(dev_t dev, unit_t unit, fatvol_t *fv);
extern int fat_umount(fatvol_t *fv);
extern int fat_stat(fatvol_t *fv, ino_t ino, struct stat *st);
extern size_t fat_read(fatvol_t *fv, ino_t ino, uint8_t __far *buf, off_t off, size_t size);

extern ino_t fat_ino_get(fatvol_t *fv, char *path, int flags);

extern int dir_find_path(fatvol_t *fv, char *fn, fileinfo_t *info);
extern int dir_find_ent(fatvol_t *fv, char *path, int len, clusidx_t cluster, fileinfo_t *info);
extern void dir_set_datetime(dirent_t __far *ent, time_t t);

extern int fobj_create(fatvol_t *fv, const char *fn, uint8_t attr, fileinfo_t *info, clusidx_t *parent);
extern int fobj_get_sec_at_offs(
	fatvol_t *fv, clusidx_t clus0, long offs, blkptr_t *blk, int overw
);

extern substr_t path_next_dir_ele(const char *fn, int start);
extern int path_parse_fn(char *dst, const char *src, int len);
extern void path_split(char *fn, char **dir, char **name);

extern int clus_get_next(fatvol_t *fv, clusidx_t cluster, clusidx_t *next);

#if 0
extern int clus_set_next(fatvol_t *fv, clusidx_t cluster, clusidx_t next);
extern int clus_alloc(fatvol_t *fv, clusidx_t *clus);


extern int dir_find_path(fatvol_t *fv, char *fn, fileinfo_t *info);

extern int fobj_trunc(fatvol_t *fv, fileinfo_t *info, uint32_t length);
extern int fobj_create(fatvol_t *fv, const char *fn, uint8_t attr, fileinfo_t *info, clusidx_t *parent);
extern int fobj_get_sec_at_offs(
	fatvol_t *fv, fileinfo_t *info, long offs, blkptr_t *blk, int overw
);

extern int f_creat(fatvol_t *fv, char *fn);
extern int f_open(fatvol_t *fv, char *fn, int flags);
extern int f_close(fatvol_t *fv, int fd);
extern int f_fstat(fatvol_t *fv, int fd, struct stat *st);
extern ssize_t f_read(fatvol_t *fv, int fd, char __far *buffer, size_t size);
extern ssize_t f_write(fatvol_t *fv, int fd, char __far *buffer, size_t size);
extern off_t f_lseek(fatvol_t *fv, int fd, off_t offset, int whence);
extern int f_mkdir(fatvol_t *fv, char *fn);
#endif
#endif
