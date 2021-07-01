#include "fileio.h"

#include "device.h"
#include "fs.h"
#include "ktypes.h"
#include "printk.h"
#include "task.h"
#include "ttycook.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

typedef struct openfile_t openfile_t;

typedef int (*fnopen_t)(int fd);
typedef int (*fnclose_t)(int fd);
typedef size_t (*fnread_t)(int fd, void __far *buf, off_t off, size_t size);
typedef size_t (*fnwrite_t)(int fd, void __far *buf, off_t off, size_t size);

static size_t file_read(int fd, void __far *buf, off_t off, size_t size);
static size_t file_write(int fd, void __far *buf, off_t off, size_t size);

static int cdev_open(int fd);
static int cdev_close(int fd);
static size_t cdev_read(int fd, void __far *buf, off_t off, size_t size);
static size_t cdev_write(int fd, void __far *buf, off_t off, size_t size);

static size_t bdev_read(int fd, void __far *buf, off_t off, size_t size);
static size_t bdev_write(int fd, void __far *buf, off_t off, size_t size);

typedef struct iovtable_t iovtable_t;
struct iovtable_t {
	fnopen_t open;
	fnclose_t close;
	fnread_t read;
	fnwrite_t write;
};

static iovtable_t fileio = {
	NULL,
	NULL,
	&file_read,
	&file_write,
};

static iovtable_t cdevio = {
	&cdev_open,
	&cdev_close,
	&cdev_read,
	&cdev_write,
};

static iovtable_t bdevio = {
	NULL,
	NULL,
	&bdev_read,
	&bdev_write,
};

struct openfile_t {
	ino_t ino;
	int refcnt;
	off_t offset;
	unsigned blksize;
	unsigned long size;
	uint8_t maj;
	uint8_t min;
	iovtable_t *ops;
};

#define MAX_OPEN_FILE 10

static openfile_t oft[MAX_OPEN_FILE];
static semaph_t critsect;

void fio_init(void)
{
	sema_init(&critsect, 1);
}

int kopen(char *path, int flags)
{
	ino_t ino;
	int err;
	int i;

	struct stat st;

    /* really we need to be sharing so there's only one fd per device
     */
	sema_down(&critsect);
	for (i = 0; i < MAX_OPEN_FILE; i++) {
		if (oft[i].refcnt == 0) {
			break;
		}
	}

	if (i == MAX_OPEN_FILE) {
		sema_up(&critsect);
		return -ENFILE;
	}

	oft[i].refcnt++;
	sema_up(&critsect);

	ino = ino_get(path, flags);
	err = INO_ERR(ino);
	if (err) {
		oft[i].refcnt--;
		return err;
	}

	err = ino_stat(ino, &st);
	if (err) {
		oft[i].refcnt--;
		return err;
	}

	if (S_ISREG(st.st_mode) || S_ISDIR(st.st_mode)) {
		oft[i].ops = &fileio;
	} else if (S_ISBLK(st.st_mode)) {
		oft[i].ops = &bdevio;
	} else if (S_ISCHR(st.st_mode)) {
		oft[i].ops = &cdevio;
	} else {
		kprintf("unknown file type - mode %04x\n", st.st_mode);
		sema_down(&critsect);
		if (--oft[i].refcnt == 0) {
			ino_put(ino);
		}
		sema_up(&critsect);
		return -EINVAL;
	}

	oft[i].ino = ino;
	oft[i].blksize = st.st_blksize;
	oft[i].offset = 0;
	oft[i].size = st.st_size;
	oft[i].maj = (uint8_t)S_MAJOR(st.st_rdev);
	oft[i].min = (uint8_t)S_MINOR(st.st_rdev);

	/* open the driver if the driver supports it */
	if (oft[i].ops->open) {
		err = oft[i].ops->open(i);
		if (err < 0) {
			sema_down(&critsect);
			if (--oft[i].refcnt == 0) {
				ino_put(ino);
			}
			sema_up(&critsect);
			return err;
		}
	}

	return i;
}

int kclose(int fd)
{
	int rc = 0;
	int clrc;

	if (fd < 0 || fd >= MAX_OPEN_FILE || oft[fd].refcnt == 0) {
		kprintf("kclose: invalid fd %d\n", fd);
		return -EINVAL;
	}

	sema_down(&critsect);
	oft[fd].refcnt--;

	if (oft[fd].refcnt == 0) {
		rc = ino_put(oft[fd].ino);
		if (oft[fd].ops->close) {
			clrc = oft[fd].ops->close(oft[fd].min);
			if (rc == 0) {
				rc = clrc;
			}
		}
	}
	sema_up(&critsect);

	return rc;
}

int kfstat(int fd, struct stat *st)
{
	if (fd < 0 || fd >= MAX_OPEN_FILE || oft[fd].refcnt == 0) {
		return -EINVAL;
	}

	return ino_stat(oft[fd].ino, st);
}

int kdup(int fd)
{
	if (fd < 0 || fd >= MAX_OPEN_FILE || oft[fd].refcnt == 0) {
		return -EINVAL;
	}

	sema_down(&critsect);
	oft[fd].refcnt++;
	sema_up(&critsect);

	return fd;
}

size_t kread(int fd, void __far *buf, size_t size)
{
	size_t read;
	int err;

	if (fd < 0 || fd >= MAX_OPEN_FILE || oft[fd].refcnt == 0) {
		return (size_t)-EINVAL;
	}

	read = oft[fd].ops->read(fd, buf, oft[fd].offset, size);
	err = (int)read;
	if (err >= 0 || err <= -EMAX) {
		oft[fd].offset += read;
	}

	return read;
}

size_t kwrite(int fd, void __far *buf, size_t size)
{
	size_t wrote;
	int err;

	if (fd < 0 || fd >= MAX_OPEN_FILE || oft[fd].refcnt == 0) {
		return (size_t)-EINVAL;
	}

	wrote = oft[fd].ops->write(fd, buf, oft[fd].offset, size);
	err = (int)wrote;
	if (err >= 0 || err <= -EMAX) {
		oft[fd].offset += wrote;
	}

	return wrote;
}

off_t kseek(int fd, long offset, int whence)
{
	off_t base;
	long newoffs = 0;
	struct stat st;
	int err;

	if (fd < 0 || fd >= MAX_OPEN_FILE || oft[fd].refcnt == 0) {
		return (size_t)-EINVAL;
	}

	switch (whence) {
	case SEEK_SET:
		base = 0;
		break;

	case SEEK_CUR:
		base = oft[fd].offset;
		break;

	case SEEK_END:
		err = kfstat(fd, &st);
		if (err < 0) {
			return err;
		}
		base = st.st_size;
		break;

	default:
		return -EINVAL;
	}

	newoffs = base + offset;

	if (
		(offset < 0 && newoffs > offset) ||
		(offset > 0 && newoffs < offset)
	) {
		return -EOVERFLOW;
	}

	oft[fd].offset = newoffs;

	return newoffs;
}


/*****************************************************************************
 * I/O stubs for file system operations
 */
size_t file_read(int fd, void __far *buf, off_t off, size_t size)
{
	return ino_read(oft[fd].ino, buf, off, size);
}

size_t file_write(int fd, void __far *buf, off_t off, size_t size)
{
	UNREFERENCED(fd);
	UNREFERENCED(buf);
	UNREFERENCED(off);
	UNREFERENCED(size);

	return 0;
}

int cdev_open(int fd)
{
	chardev_t *dev = dev_getcdev(DEVNO(oft[fd].maj));
	if (dev == NULL) {
		return (size_t)-ENODEV;
	}

	return dev->open(oft[fd].min);
}

int cdev_close(int fd)
{
	chardev_t *dev = dev_getcdev(DEVNO(oft[fd].maj));
	if (dev == NULL) {
		return (size_t)-ENODEV;
	}

	return dev->close(oft[fd].min);
}

size_t cdev_read(int fd, void __far *buf, off_t off, size_t size)
{
	chardev_t *dev = dev_getcdev(DEVNO(oft[fd].maj));

	/* character devices don't seek */
	UNREFERENCED(off);

	if (dev == NULL) {
		return (size_t)-ENODEV;
	}

	return dev->read(oft[fd].min, buf, size);
}

size_t cdev_write(int fd, void __far *buf, off_t off, size_t size)
{
	int wrote;
	chardev_t *dev = dev_getcdev(DEVNO(oft[fd].maj));

	/* character devices don't seek */
	UNREFERENCED(off);

	if (dev == NULL) {
		return (size_t)-ENODEV;
	}

	wrote = dev->write(oft[fd].min, buf, size); 
	return wrote;
}

size_t bdev_read(int fd, void __far *buf, off_t off, size_t size)
{
	UNREFERENCED(fd);
	UNREFERENCED(buf);
	UNREFERENCED(off);
	UNREFERENCED(size);

	return 0;
}

size_t bdev_write(int fd, void __far *buf, off_t off, size_t size)
{
	UNREFERENCED(fd);
	UNREFERENCED(buf);
	UNREFERENCED(off);
	UNREFERENCED(size);

	return 0;
}


