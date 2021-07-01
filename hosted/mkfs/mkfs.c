#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <kernel/sys/devnos.h>

#include "blkcache.h"
#include "fat\fat.h"
#include "blkcache\blkdev.h"

#ifndef BLKSIZE
# define BLKSIZE 512
#endif

extern uint8_t bootsect[];
extern char *kernloc;

static char *get_kernel_path(char *arg0);
static int write_bootsect(unit_t unit, bpb_t *bpb);
static int write_fat(unit_t unit, const bpb_t *bpb);
static int write_rootdir(unit_t unit, const bpb_t *bpb);
static int write_kernel(unit_t unit, FILE *fp, fatvol_t *fv);
static int make_devs(fatvol_t *fv);

static void usage(void);

int main(int argc, char **argv)
{
	int rc;
	bpb_t bpb;
	char *kpath;
	FILE *fp;
	fatvol_t fv;
	int i;
	char *loopfn = NULL;
	unit_t unit = 0;

	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			switch (argv[i][1]) {
			case 'f':
				if (i >= argc - 1) {
					usage();
				}
				i++;
				loopfn = argv[i];
				break;

			default:
				usage();
			}
		}
	}

	if (loopfn) {
		int n = loop_open(loopfn);
		if (n != 0) {
			fprintf(stderr, "failed to open loop file %s\n", loopfn);
			return 1;
		}

		unit = LOOP_UNIT + n;
	}

	kpath = get_kernel_path(argv[0]);
	fp = fopen(kpath, "rb");
	if (fp == NULL) {
		fprintf(stderr, "cannot open default kernel image %s\n", kpath);
		return 1;
	}

	if (bc_init() != 0) {
		fprintf(stderr, "Failed to init devices and/or cache\n");
		return 1;
	}

	rc = write_bootsect(unit, &bpb);
	if (rc < 0) {
		if (rc == -ENXIO) {
			fprintf(stderr, "\nNo media in drive\n");
			return 1;
		}
		fprintf(stderr, "write boot: %s\n", strerror(-rc));
		return 1;
	}

	rc = write_fat(unit, &bpb);
	if (rc < 0) {
		fprintf(stderr, "write FAT: %s\n", strerror(-rc));
		return 1;
	}

	rc = write_rootdir(unit, &bpb);
	if (rc < 0) {
		fprintf(stderr, "write rootdir: %s\n", strerror(-rc));
		return 1;
	}

	rc = f_mount(0, unit, &fv);
	if (rc < 0) {
		fprintf(stderr, "mount new volume: %s\n", strerror(-rc));
		return rc;
	}

	rc = write_kernel(unit, fp, &fv);
	if (rc < 0) {
		fprintf(stderr, "write kernel: %s\n", strerror(-rc));
		return 1;
	}

	rc = make_devs(&fv);
	if (rc < 0) {
		fprintf(stderr, "write kernel: %s\n", strerror(-rc));
		return 1;
	}

	rc = f_umount(&fv);
	if (rc < 0) {
		fprintf(stderr, "unmount new volume: %s\n", strerror(-rc));
		return rc;
	}

	rc = bc_sync();
	if (rc < 0) {
		fprintf(stderr, "sync: %s\n", strerror(-rc));
		return 1;
	}

	return 0;
}

void usage(void)
{
	printf("mkfs [-f file]\n");
	exit(0);
}

char *get_kernel_path(char *arg0)
{
	size_t len;
	size_t klen;
	char *p;
	char *path;

	p = arg0 + strlen(arg0) - 1;

	while (p >= arg0 && *p != '\\') {
		--p;
	}

	p++;

	klen = strlen(kernloc);
	len = (p - arg0) + 1 + klen + 1;
	
	path = malloc(len);
	memcpy(path, arg0, p - arg0);
	p = path + (p - arg0);
	*p++ = '\\';
	strcpy(p, kernloc);

	return path;
}

int write_bootsect(unit_t unit, bpb_t *bpb)
{
	block_t *blk;
	int rc;
	int i;

	printf("boot sector\n");

	for (i = 0; i < 2; i++) {
		rc = bc_get(0, unit, 0, &blk, 0);
	 	if (rc == -EUCLEAN) {
			continue;
		}
		if (rc != 0) {
			return rc;
		}
	}

	_fmemcpy(bpb, blk->data + 3, sizeof(bpb_t));
	_fmemcpy(blk->data, bootsect, BLKSIZE);

	/* TODO fix BPB - defaults to 1.44M floppy
	 */
	blk->dirty = 1;
	rc = bc_put(blk);
	
	return rc;
}

int write_fat(unit_t unit, const bpb_t *bpb)
{
	int rc;
	blkno_t fat0 = bpb->hidden_sectors + bpb->reserved_sectors;
	block_t *blk;
	unsigned ui;
	unsigned uj;

	for (ui = 0; ui < bpb->fat_count; ui++) {
		for (uj = 0; uj < bpb->sectors_per_fat; uj++) {
			printf("fat block %u %u %lu\n", ui, uj, fat0);
			rc = bc_get(0, unit, fat0++, &blk, F_BCGET_OVERW);
			if (rc < 0) {
				return rc;
			}
			_fmemset(blk->data, 0, bpb->bytes_per_sector);
			if (uj == 0) {
				blk->data[0] = bpb->media_descriptor;
				blk->data[1] = 0xff;
				blk->data[2] = 0xff;
			}

			blk->dirty = 1;
			rc = bc_put(blk);
			if (rc < 0) {
				return rc;
			}
		}
	}

	return 0;
}

int write_rootdir(unit_t unit, const bpb_t *bpb)
{
	blkno_t dir =
		bpb->hidden_sectors +
		bpb->reserved_sectors +
		bpb->fat_count * bpb->sectors_per_fat;
	unsigned secs =
		(bpb->root_entries * sizeof(dirent_t) +
		 bpb->bytes_per_sector - 1) / bpb->bytes_per_sector;

	unsigned ui;
	int rc;
	block_t *blk;

	printf("sizeof dirent %d\n", sizeof(dirent_t));
	printf("root dir starts at %lu for %u\n", dir, secs);

	for (ui = 0; ui < secs; ui++) {
		rc = bc_get(0, unit, dir + ui, &blk, F_BCGET_OVERW);
		if (rc < 0) {
			return rc;
		}
		_fmemset(blk->data, 0, bpb->bytes_per_sector);
		blk->dirty = 1;
		rc = bc_put(blk);
		if (rc < 0) {
			return rc;
		}
	}

	return 0;
}

int write_kernel(unit_t unit, FILE *fp, fatvol_t *fv)
{
	int rc;
	int fd;
	block_t *blk;
	bpb_t far *bpb;
	unsigned long by_per_cluster;
	unsigned long ksize;
	unsigned long wrote;
	static char buffer[512];
	
	rc = bc_get(0, unit, 0, &blk, 0);
	if (rc < 0) {
		return rc;
	}
	bpb = (bpb_t far *)blk->data;

	by_per_cluster = bpb->bytes_per_sector * bpb->sector_per_cluster;
	printf("by per cluster %ld\n", by_per_cluster);
	
	fseek(fp, 0L, SEEK_END);
	ksize = ftell(fp);
	fseek(fp, 0L, SEEK_SET);

	printf("kernel is %ld bytes\n", ksize);

#if 0
	ksize = (ksize + by_per_cluster - 1) / by_per_cluster;
	printf("kernel is %ld clusters\n", ksize);
#endif

	/* NB we are taking advantage of the fact that we have
	 * just cleared the disk here. The kernel has to be
	 * contiguous on disk for the boot sector to work.
	 * If we want to update the kernel without erasing the
	 * whole disk, we need more code.
     */
	wrote = 0;
	fd = f_open(fv, "KERNEL", O_CREAT|O_WRONLY);
	if (fd < 0) {
		printf("open call failed\n");
		return fd;
	}

	while (wrote < ksize) {
		size_t towrite = sizeof(buffer);
		int bufwrote;

		if (ksize - wrote < towrite) {
			towrite = (size_t)(ksize - wrote);
		}

		if (fread(buffer, 1, towrite, fp) != towrite) {
			f_close(fv, fd);
			return -EIO;
		}

		bufwrote = (int)f_write(fv, fd, buffer, towrite);
		if ((size_t)bufwrote < towrite) {
			printf("write call failed\n");
			f_close(fv, fd);
			return rc >= 0 ? -EIO : rc;
		}

		wrote += towrite;
	}

	return f_close(fv, fd);
}

/* Make the /dev directory
 */

typedef struct device_t device_t;
struct device_t {
	char *name;
	uint8_t major;
	int units;
};

static device_t devices[] = 
{
	{ "fdd", BDEV(FDD), 2 },
	{ "hdd", BDEV(HDD), 2 },
	{ "con", CDEV(CON), 1 },
	{ "tty", CDEV(TTY), 4 },
	{ NULL, 0, 0 }, 
};

int make_devs(fatvol_t *fv)
{
	int rc;
	int i;
	int j;

	char name[50];

	rc = f_mkdir(fv, "\\dev");

	for (i = 0; devices[i].name; i++) {
		for (j = 0; j < devices[i].units; j++) {
            if (devices[i].units == 1) {
			    sprintf(name, "\\dev\\%s", devices[i].name, j);
            } else {
			    sprintf(name, "\\dev\\%s%d", devices[i].name, j);
            }
			printf("mknod %s\n", name);
			rc = f_mknod(fv, name, devices[i].major, (uint8_t)j);
			if (rc < 0) {
				fprintf(stderr, "mknod %s failed.\n", name);
				return rc;
			}
		}
	}

	return 0;
}
