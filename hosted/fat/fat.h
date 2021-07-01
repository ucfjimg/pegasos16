#ifndef FAT_H_
#define FAT_H_

#if defined(__TURBOC__) || (defined(_MSC_VER) && (_MSC_VER <= 800))
# define NO_UNISTD
# define NO_STAT_EXTRAS
#endif

#include "ktypes.h"

typedef int ssize_t;

#include "blkcache.h"

#include <errno.h>
#include <stdio.h>
#include <time.h>

#include <sys/types.h>
#include <sys/stat.h>

#ifdef __GNUC__
# define PACKED __attribute__((packed))
#else
# define PACKED
#endif

#define DELETE_FLAG ((uint8_t)0xe5)
#define F_MAX_PATH 128
#define MAX_OPENFILES 8

typedef uint32_t clusidx_t;

#define BLOCK_SIZE 512
enum media_desc_t {
    md_288mb = 0xf0,  /* 2.88MB 3.5"  2-sided 36-sector 80 tracks */
    md_144mb = 0xf0,  /* 1.44MB 3.5"  2-sided 18-sector 80 tracks */
    md_720k  = 0xf9,  /*  720KB 3.5"  2-sided  9-sector 80 tracks */
    md_12mb  = 0xf9,  /*  1.2MB 5.25" 2-sided 15-sector 80 tracks */
    md_360k  = 0xfd,  /*  360KB 5.25" 2-sided  9-sector 40 tracks */
    md_320k  = 0xff,  /*  320KB 5.25" 2-sided  8-sector 40 tracks */
    md_180k  = 0xfc,  /*  180KB 5.25" 1-sided  9-sector 40 tracks */
    md_160k  = 0xfe,  /*  160KB 5.25" 1-sided  8-sector 40 tracks */
    md_hdd   = 0xf8,  /* any size hard drive */
};

typedef struct bpb_t bpb_t;

#if defined(_MSC_VER) && !defined(PACK1)
# pragma pack(1)
# pragma warning( disable: 4121 )
#endif
struct bpb_t {
    char fsid[8];                       /* file system identification */
    uint16_t bytes_per_sector;          /* bytes per sector on media */
    uint8_t sector_per_cluster;         /* sectors in a FAT cluster */
    uint16_t reserved_sectors;          /* reserved logical sectors */
    uint8_t fat_count;                  /* number of FAT's */
    uint16_t root_entries;              /* # entries in the root directory */
    uint16_t total_sectors;             /* total sectors on media */
    uint8_t media_descriptor;           /* descriptor for floppy media types */
    uint16_t sectors_per_fat;           /* sectors in each FAT image */
    uint16_t sectors_per_track;         /* sectors per physical track */
    uint16_t heads;                     /* heads on physical media */
    uint32_t hidden_sectors;            /* hidden sectors */
    uint32_t large_sectors;             /* if total > 64k sectors, goes here */
    uint8_t drive_no;                   /* drive number if HDD */
    uint8_t flags;                      /* flags */
    uint8_t ebootsig;                   /* extended boot signature */
    uint32_t volser;                    /* volume serial number */
    char label[11];                     /* volume label	*/
    char fstype[8];                     /* file system type */
} PACKED;

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
    /* sector and offset w/in sector of dir entry
     */
	blkno_t sector;
    unsigned secoffs;

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
    openfile_t openfiles[MAX_OPENFILES];
};

extern int clus_get_next(fatvol_t *fv, clusidx_t cluster, clusidx_t *next);
extern int clus_set_next(fatvol_t *fv, clusidx_t cluster, clusidx_t next);
extern int clus_alloc(fatvol_t *fv, clusidx_t *clus);

extern int path_parse_fn(char *dst, const char *src, int len);
extern substr_t path_next_dir_ele(const char *fn, int start);
extern void path_split(char *fn, char **dir, char **name);

extern int dir_find_ent(fatvol_t *fv, char *path, int len, clusidx_t cluster, fileinfo_t *info);
extern int dir_find_path(fatvol_t *fv, char *fn, fileinfo_t *info);
extern void dir_set_datetime(dirent_t __far *ent, time_t t);

extern int fobj_trunc(fatvol_t *fv, fileinfo_t *info, uint32_t length);
extern int fobj_create(fatvol_t *fv, const char *fn, uint8_t attr, fileinfo_t *info, clusidx_t *parent);
extern int fobj_get_sec_at_offs(
	fatvol_t *fv, fileinfo_t *info, long offs, blkptr_t *blk, int overw
);

extern int f_mount(dev_t dev, unit_t unit, fatvol_t *fv);
extern int f_umount(fatvol_t *fv);
extern int f_stat(fatvol_t *fv, char *fn, struct stat *st);
extern int f_creat(fatvol_t *fv, char *fn);
extern int f_open(fatvol_t *fv, char *fn, int flags);
extern int f_close(fatvol_t *fv, int fd);
extern int f_fstat(fatvol_t *fv, int fd, struct stat *st);
extern ssize_t f_read(fatvol_t *fv, int fd, char __far *buffer, size_t size);
extern ssize_t f_write(fatvol_t *fv, int fd, char __far *buffer, size_t size);
extern off_t f_lseek(fatvol_t *fv, int fd, off_t offset, int whence);
extern int f_mkdir(fatvol_t *fv, char *fn);
extern int f_mknod(fatvol_t *fv, char *fn, uint8_t major, uint8_t minor);

#endif
