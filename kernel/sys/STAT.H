#ifndef STAT_H_
#define STAT_H_

struct stat {
	unsigned long st_ino;
	unsigned long st_mtime;
	unsigned long st_size;
	unsigned st_nlink;
	unsigned st_blksize;
	unsigned st_mode;
	unsigned st_rdev;
};

#define S_IFMT 		0170000
#define S_IFREG		0100000
#define S_IFBLK		0060000
#define S_IFDIR		0040000
#define S_IFCHR		0020000

#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)
#define S_ISBLK(m)  (((m) & S_IFMT) == S_IFBLK)
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)

#define S_MAJOR(rdev) ((rdev) >> 8)
#define S_MINOR(rdev) ((rdev) & 0xff)
#define S_RDEV(maj,min) (((maj) << 8) | ((min) & 0xff))

#endif

