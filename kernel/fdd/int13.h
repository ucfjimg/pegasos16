/*
 * int13 wrapper routines
 */
#ifndef INT13_H_
#define INT13_H_

#include "ktypes.h"

extern int bios_reset_drv(int drive);

typedef struct drvgeom_t drvgeom_t;
struct drvgeom_t {
	uint16_t heads;
	uint16_t cyls;
	uint8_t secs;
	uint8_t drives;
};

extern int bios_drvgeom(int drive, drvgeom_t __far *geom);

#define BMC_NOCHANGE 0
#define BMC_CHANGE   1
#define BMC_NOMEDIA  2

extern int bios_media_change(int drive);

#define I13_ERR_SUCCESS		    0x00
#define I13_ERR_INVALID_CMD     0x01
#define I13_ERR_ADDR_MARK		0x02
#define I13_ERR_WRITE_PROTECT   0x03
#define I13_ERR_NO_SECTOR       0x04
#define I13_ERR_RESET_FAILED    0x05
#define I13_ERR_MEDIA_CHANGE 	0x06
#define I13_ERR_ACTIVITY_FAILED 0x07
#define I13_ERR_DMA_OVERRUN     0x08
#define I13_ERR_DMA_64K			0x09
#define I13_ERR_BAD_SECTOR		0x0A
#define I13_ERR_BAD_CYL         0x0B
#define I13_ERR_NO_MEDIA_TYPE   0x0C
#define I13_ERR_BAD_SEC_COUNT   0x0D
#define I13_ERR_CD_ADDRMARK     0x0E
#define I13_ERR_DMA_OUT_OF_RNG  0x0F
#define I13_ERR_CRC				0x10
#define I13_ERR_ECC_CORRECTED   0x11
#define I13_ERR_CONTROLLER      0x20
#define I13_ERR_SEEK            0x40
#define I13_ERR_TIMEOUT 		0x80

/*
 * it's the caller's responsibility to
 *  (1) ensure that the passed buffer is DMA safe
 *      i.e. doesn't span a physical 64k boundary
 *
 *  (2) handle the multiple recommended retries for
 *      floppy disk access
 *
 *  (3) ensure that the CHS address is within 
 *      the bounds of the device's geometry
 *
 * Unlike other calls, this call returns 0 for success and an 
 * actual error code rather than just -1 on failure
 */
extern int bios_read_blk(int drive, int cyl, int head, int sec, uint8_t __far *buffer);
extern int bios_write_blk(int drive, int cyl, int head, int sec, uint8_t  __far *buffer);

#endif
