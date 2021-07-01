#ifndef DEVNOS_H_
#define DEVNOS_H_

/* NB these MUST match the order of the device tables
 * in device.c!!!
 */
#define CDEV_CON 0			/* PC's screen and keyboard */
#define CDEV_TTY 1			/* serial ports */

#define BDEV_FDD 0			/* floppy drive(s) */
#define BDEV_HDD 1			/* hard disk(s) */

#define CDEV(x) (CDEV_##x)
#define BDEV(x) ((BDEV_##x) | 0x80)
#define DEVNO(x) ((x) & 0x7f)
#define ISBDEV(x) ((x) & 0x80)
#define ISCDEV(x) (((x) & 0x80) == 0)

#endif
