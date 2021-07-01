#include "blkcache.h"
#include "console/console.h"
#include "dbview.h"
#include "device.h"
#include "exec.h"
#include "fileio.h"
#include "fs.h"
#include "intr.h"
#include "dma.h"
#include "ktypes.h"
#include "memory.h"
#include "panic.h"
#include "ports.h"
#include "printk.h"
#include "process.h"
#include "task.h"
#include "tty.h"

#include "fcntl.h"
#include "sys/stat.h"

extern const char *kver;

extern void kbreak(void);

extern void sc_init(void);
extern void bluescreen(void);

static void thread(void);

extern int strcmp(char*, char*);

extern void splash(void);
extern void tty_dump(void);


/* unreachable code (due to the infinite loop) */
#pragma warning( disable : 4702 )

void async(void)
{
	kprintf("async\n");
}


int main(int minal, int pages, int extra, int hdrpara, int ds, int drive)
{
	int i;
	unsigned exepara;
	char __far *scr = MKFP(0xb800, 156);
	uint8_t __far *blk;
	blkdev_t *fdd;
	int dev, unit;

/*
	task_t *tsk;
*/

	UNREFERENCED(drive);

	con_clrscr();
    splash();
	con_gotoxy(0, con_maxy());


	kprintf("Welcome to PegasOS 0.0.%s\n", kver);

	if (extra) {
		pages--;
	}
	exepara = pages << (PAGESHIFT - PARASHIFT);
	extra = (extra + PARASIZE - 1) >> PARASHIFT;
	exepara += extra;
	exepara -= hdrpara;

	db_init();
   	mem_init(ds, ds + exepara + minal);
	task_init();
	dma_init();
    proc_init();
   	dev_init();
	bc_init();
	fio_init();
	sc_init();


	blk = kmalloc(512);
	fdd = dev_getbdev(0);

	/* TODO hardcoded BIOS goop */
	if (drive & 0x80) {
		dev = BDEV_HDD;
	} else {
		dev = BDEV_FDD;
	}
	unit = drive & 0x7f;
	kprintf("mounting (%d,%d) as root\n", dev, unit);

	i = fs_mount(dev, unit);
	if (i < 0) {
		kprintf("mount failed %d\n", i);
		panic("could not mount root filesystem");
	}

	i = (int)exec("SH.EXE");
	if (i < 0) {
		kprintf("exec failed %d\n", i);
	} 

	i = kopen("/dev/con", O_RDWR);
	if (i < 0) {
		kprintf("failed to open con\n");
	} 

#if 0
	kprintf("sleeping for about 5 seconds\n");
	task_sleep(5000 / 55);
	kprintf("awake again\n");
#endif
{
	static char buffer[80];
	int ibuf = 0;
	static char bs[] = "\b \b";
	static char prompt[] = "\r\n# ";

	char __far *scr = MKFP(0xb800, 156);

	kwrite(i, "\r\n# ", 4);
	for(;;)
	{
		char ch;
		if (kread(i, &ch, 1) == 1) {
			if (ch == '\b') {
				if (ibuf > 0) {
					kwrite(i, bs, sizeof(bs) - 1);
					ibuf--;
				}
			} else if (ch == '\n') {
				buffer[ibuf] = 0;
				if (strcmp(buffer, "tty") == 0) {
					kwrite(i, "\r\n", 2);
					tty_dump();
				} else if (strcmp(buffer, "irq") == 0) {
					uint8_t is[2], ir[2], im[2];
					intr_cli();
					outb(0x20, 0x0a); /* read IR */
					ir[0] = inb(0x20);
					outb(0x20, 0x0b); /* read IS */
					is[0] = inb(0x20);
					outb(0xa0, 0x0a); /* read IR */
					ir[1] = inb(0xa0);
					outb(0xa0, 0x0b); /* read IS */
					is[1] = inb(0xa0);
					im[0] = inb(0x21);
					im[1] = inb(0xa1);
					intr_sti();

					kwrite(i, "\r\n", 2);
					kprintf("ir %02x%02x  is %02x%02x  mask %02x%02x\n",
						ir[1], ir[0],
						is[1], is[0],
						im[1], im[0]
					);
				}
				ibuf = 0;
				kwrite(i, prompt, sizeof(prompt) - 1);
			} else if (ch >= 0x20 && ch <= 0x7e) {
				if (ibuf < sizeof(buffer) - 1) {
					buffer[ibuf++] = ch;
					kwrite(i, &ch, 1);
				}
			}
		}

		(*scr)++;
	}
}

	return 0;
}

