#ifndef BPB_H_
#define BPB_H_

#include "ktypes.h"

#ifdef __GNUC__
# define PACKED __attribute__((packed))
#else
# define PACKED
#endif

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

#if defined(_MSC_VER) && !defined(PACK1)
# pragma pack(2)
#endif

#endif

