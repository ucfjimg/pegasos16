#include "fs.h"
#include "printk.h"

#include "fatfs.h"

fatvol_t fv;

/*
 * mount a root file system on the given block
 * device and unit
 */
int fs_mount(int dev, int unit)
{
	int rc;
	rc = fat_mount(dev, unit, &fv);
	return rc;
}

/*
 * unmount the root file system
 */
int fs_umount(void)
{
	return fat_umount(&fv);
}

/*
 * get information about the inode
 */
int ino_stat(ino_t ino, struct stat *st)
{
	return fat_stat(&fv, ino, st);
}


size_t ino_read(ino_t ino, void __far *buf, off_t off, size_t size)
{
	return fat_read(
		&fv,
		ino,
		buf,
		off,
		size
	);
}

ino_t ino_get(char *path, int flags)
{
	return fat_ino_get(&fv, path, flags);
}

int ino_put(ino_t ino)
{
	UNREFERENCED(ino);
	return 0;
}	


