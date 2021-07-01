#include "device.h"
#include "panic.h"
#include "printk.h"

extern chardev_t condev;
extern chardev_t ttydev;
extern blkdev_t fdddev;

static chardev_t *cdevtab[] =
{
	&condev,	/* CDEV_CON */
	&ttydev,	/* CDEV_TTY */
	NULL
};
static int nchardev;

static blkdev_t *bdevtab[] =
{
	&fdddev,	/* BDEV_FDD */
	NULL
};
static int nblkdev; 

void dev_init()
{
	int i;

	for (i = 0; bdevtab[i]; i++) {
		bdevtab[i]->init();
	}
	nblkdev = i;

	for (i = 0; cdevtab[i]; i++) {
		cdevtab[i]->init();
	}
	nchardev = i;

	kprintf("dev_init: %d block, %d char\n", nblkdev, nchardev);
}

blkdev_t *dev_getbdev(int idx)
{
	if (idx < 0 || idx >= nblkdev) {
		return NULL;
	}
	return bdevtab[idx];
}

chardev_t *dev_getcdev(int idx)
{
	if (idx < 0 || idx >= nchardev) {
		return NULL;
	}
	return cdevtab[idx];
}
