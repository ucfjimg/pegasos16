#include "bpb.h"
#include "cmos.h"
#include "console/console.h"
#include "device.h"
#include "dma.h"
#include "int13.h"
#include "intr.h"
#include "memory.h"
#include "panic.h"
#include "printk.h"

#include "ports.h"
#include "task.h"

#include <errno.h>
#include <string.h>

#undef FDD_DEBUG
#ifdef FDD_DEBUG
# define dprintf(x) kprintf x
#else
# define dprintf(x) ;
#endif

/* global parameters 
 */
#define FDD_MAX_UNITS 2
#define FDD_SECTOR_SIZE 512
#define FDD_RETRY 3

/* hardware interface 
 */
#define FDD_DRQ			2
#define FDD_IRQ			6

#define FDD_DOR_PORT	0x3f2
#define FDD_DSR_PORT	0x3f4		/* datarate select (wr only) */
#define FDD_STAT_PORT	0x3f4		/* status (rd only) */
#define FDD_DATA_PORT	0x3f5
#define FDD_CCR_PORT	0x3f7		/* bottom two bits are data rate, like DSR */

#define FDD_DOR_MOT(dr) ( 0x10 << ((dr) & 0x03) )
#define FDD_DOR_DMA		0x08
#define FDD_DOR_RESET	0x04		/* this is active low! */
#define FDD_DOR_DR(dr)	( (dr) & 0x03 )

#define FDD_DSR_500K	0x00		/* 500k BPS (1.44M/1.2M)
#define FDD_DSR_300K	0x01		/* 300k BPS */
#define FDD_DSR_250K	0x02		/* 250k BPS */
#define FDD_DSR_1M		0x03		/* 1M BPS (2.88M) */
#define FDD_DSR_RESET	0x80

#define FDD_STAT_RQM	0x80		/* request for master */
#define FDD_STAT_DIO	0x40		/* direction, 1 == to CPU, 0 == to FDC */
#define FDD_STAT_NDMA	0x20		/* FDC is in non-DMA mode */
#define FDD_STAT_BUSY	0x10		/* controller is busy w/ non-seek op */
#define FDD_STAT_SEEK(dr) \
	( 0x01 << ((dr) & 0x03) )		/* drive dr is seeking */

/* dump regs - assumes buf is a 10-byte buffer */
#define FDD_DUMP_REGS_BYTES		10
#define FDD_DR_DRIVE_PCN(buf, dr) ((buf)[(dr) & 0x03])
#define FDD_DR_SRT(buf) (((buf)[4] >> 4) & 0x0f)
#define FDD_DR_HUT(buf) ((buf)[4] & 0x0f)
#define FDD_DR_HLT(buf) (((buf)[5] >> 1) & 0x7f)
#define FDD_DR_ND(buf)  ((buf)[5] & 0x01)
#define FDD_DR_SC(buf)  ((buf)[6])
/* 7 is undefined */
#define FDD_DR_EIS(buf) ((buf)[8] & 0x40)
#define FDD_DR_EFIFO(buf) ((buf)[8] & 0x20)
#define FDD_DR_DPOLL(buf) ((buf)[8] & 0x10) /* DISABLE poll */
#define FDD_DR_FIFO_THR(buf) ((buf)[8] & 0x0f)
#define FDD_DR_PRETRK(buf) ((buf)[9])

/* offsets w/in buffer of config bytes */
#define FDD_DR_CONFIG1	8
#define FDD_DR_CONFIG2	9

/* seek bits */
#define FDD_SEEK_RELATIVE		0x80

#define FDD_SEEK_0	0x0f
#define FDD_SEEK_1(hd, dr) ((((hd) & 0x01) << 2) | ((dr) & 0x03))
#define FDD_SEEK_2(cyl)    ((cyl) & 0xff)

/* version */
#define FDD_VERSION_EXT			0x90

/* configure bits */
#define FDD_CONF_IMP_SEEK		0x40
#define FDD_CONF_FIFO			0x20
#define FDD_CONF_DPOLL			0x10	/* DISABLE poll */
#define FDD_CONF_FIFO_THR		0x0f

/* lock bits */
#define FDD_LOCK_LOCK			0x80
#define FDD_LOCK_RESULT_LOCKED	0x10

/* status register bits */
#define FDD_ST0_IC(st) 		(((st) >> 6) & 0x03)
#define FDD_ST0_SEEK		(1 << 5)
#define FDD_ST0_EQCHECK		(1 << 4)
#define FDD_ST0_DRIVE(st)	((st) & 0x03)

#define FDD_ST0_IC_OK		0
#define FDD_ST0_IC_ABORT	1
#define FDD_ST0_IC_BADCMD	2
#define FDD_ST0_ID_BADPOLL	3

#define FDD_ST2_WRONGCYL	0x10

/* commands */
#define FDD_CMD_READ_TRACK		0x02
#define FDD_CMD_SPECIFY			0x03
#define FDD_CMD_SENSE_DRV_STAT	0x04
#define FDD_CMD_WRITE_DATA		0x05
#define FDD_CMD_READ_DATA		0x06
#define FDD_CMD_RECALIBRATE		0x07
#define FDD_CMD_SENSE_INTR_STAT	0x08
#define FDD_CMD_WRITE_DEL_DATA	0x09
#define FDD_CMD_READ_ID			0x0a
#define FDD_CMD_READ_DEL_DATA	0x0c
#define FDD_CMD_FORMAT_TRACK	0x0d
#define FDD_CMD_DUMP_REGS		0x0e
#define FDD_CMD_SEEK			0x0f
#define FDD_CMD_VERSION			0x10
#define FDD_CMD_PERP_MODE		0x12
#define FDD_CMD_CONFIGURE		0x13
#define FDD_CMD_LOCK			0x14
#define FDD_CMD_VERIFY			0x16

#define FDD_OP_MT				0x80	/* multitrack read/write/verify */
#define FDD_OP_MFM				0x40	/* double density read/write/verify */

/* recal bits */
#define FDD_RECAL_0(dr) ((dr) & 0x03)

/* read-back results from controller 
 */
typedef struct fdd_recal_res_t fdd_recal_res_t;
struct fdd_recal_res_t {
	uint8_t st0;
	uint8_t cyl;
};

typedef struct fdd_io_result_t fdd_io_result_t;
struct fdd_io_result_t {
	uint8_t st0;
	uint8_t st1;
	uint8_t st2;
	uint8_t cyl;
	uint8_t head;
	uint8_t sec;
	uint8_t n;
};

/* driver state
 * NB all of this assumes only one controller. 
 */

#define FDD_CS_TIMEOUT	0x0001	/* controller unresponsive */
#define FDD_CS_STATE	0x0002	/* controller not idle when it should be */

#define FDD_CS_RESET	0x0100	/* waiting for reset interrupt */
#define FDD_CS_CONFIG	0x0200	/* config and lock have been issued */
#define FDD_CS_SEEKING	0x0400	/* one or more drives are seeking */

typedef enum {
	fdd_idle,
	fdd_seeking,
	fdd_seekdone,
	fdd_io,
} fdd_state_t;	

typedef struct fddunit_t fddunit_t;
struct fddunit_t {
	int drive;							/* drive index */
	int type;	   					 	/* CMOS drive type */

	int motor_tmo;						/* motor ticks left */

	fdd_state_t state;					/* what's the drive doing? */

	taskq_t waiter;						/* someone waiting on interrupt */

	unsigned timeout : 1;				/* hit a timeout error */

	int nexpresult;						/* expected # result bytes */
	int nresult;						/* # of result bytes */
	uint8_t result[10];					/* result bytes */

	int cyl;							/* current cylinder */
};

typedef struct fdd_biosparm_t fdd_biosparm_t;
struct fdd_biosparm_t {
	uint8_t spec1;						/* specify byte 1 */
	uint8_t spec2;						/* specify byte 2 */
	uint8_t motoff_tmo;					/* motor off timeout (ticks) */
	uint8_t by_per_sec;					/* 0 = 128, 1 = 256, etc. */
	uint8_t eot;						/* sectors per track */
	uint8_t gap;						/* I/O gap byte */
	uint8_t dtl;						/* I/O DTL byte */
	uint8_t gap3;						/* gap for format */
	uint8_t fmt_fil;					/* fill byte for format */
	uint8_t head_settle;				/* in millisec */
	uint8_t mot_start;					/* 1/8 seconds */
};

static char *type_names[] = {
	NULL,
	"360K 5.25\"",
	"1.2M 5.25\"",
	"720K 3.5\"",
	"1.44M 3.5\"",
};

static uint8_t __far *work_buf;
static int nunits;
static fddunit_t units[FDD_MAX_UNITS];
static int ctrl_state;			/* controller state flags */
static fddunit_t *iounit;		/* if I/O is going, to which unit */

static fdd_biosparm_t biosparm;	/* parameters from BIOS */
static task_t *motor_task;		/* motor spindown task */
static critsect_t motor_cs;		/* protects motor data */
static uint8_t motor_bits;		/* bit mask of which motors are on */

static fddunit_t *seldrive;		/* currently selected drive */
static taskq_t waiter;			/* wait queue for non-unit irq's */

static critsect_t fdd_cs;		/* for now we are NOT allowing concurrent access across drives */ 

static void fdd_init(void);
static int  fdd_intr(unsigned irq);
static void fdd_intr_reset(void);
static void fdd_intr_seek(void);

static uint8_t fdd_wait_for_rqm(void);
static int fdd_send_command(uint8_t *data, int len);
static int fdd_read_bytes(uint8_t *data, int len);
static int fdd_flush(void);
static int fdd_version(void);
static int fdd_configure(void);
static int fdd_ctrl_reset(void);
static int fdd_specify(void);
static int fdd_seldrive(fddunit_t *pu);
static int fdd_recalibrate(fddunit_t *pu);
static int fdd_startio(fddunit_t *pu);

static int fdd_seek(fddunit_t *pu, unsigned head, unsigned cyl);
static void fdd_set_seeking(void);

static int fdd_load_media(int unit);
       int fdd_read(int unit, void __far *buf, blkno_t blk, int size);
static int fdd_write(int unit, void __far *buf, blkno_t blk, int size);
static int fdd_op(int unit, uint8_t __far *buf, blkno_t blk, int size, int doread);
static int fdd_dma_cross(uint8_t __far *ptr);

static void fdd_motor_task(void);

blkdev_t fdddev = {
	&fdd_init,
	&fdd_read,
	&fdd_write
};

#pragma warning( disable : 4101 ) /* unreferenced local variable */
#pragma warning( disable : 4505 ) /* unreferenced local function */

void fdd_init(void)
{
	uint8_t __far *p;
	uint8_t drives = cmos_read(CMOS_FDD_REG);
	int i, j;
	int sti;
	uint32_t linear;
	task_t *mt;

	nunits = 0;
	ctrl_state = 0;
	taskq_init(&waiter);
	CS_INIT(&motor_cs);
	CS_INIT(&fdd_cs);

	/* first see if CMOS even reports any drives
	 */
	for (i = 0; i < FDD_MAX_UNITS; i++) {
		fddunit_t *pu = &units[nunits];
		int type = (i == 0) ? CMOS_FDD_UNIT_0(drives) : CMOS_FDD_UNIT_1(drives);
		if (type == CMOS_FDD_NONE || type > CMOS_FDD_MAX) {
			continue;
		}
		kprintf("fdd%d: %s\n", i, type_names[type]);
		nunits++;

		pu->drive = i;
		pu->type = type;
		pu->motor_tmo = 0;
		taskq_init(&pu->waiter);
		pu->timeout = 0;
	}

	/* flush anything pending from the controller. if this fails
	 * there is no drive or it's broken.
	 */
	if (fdd_flush() == -1) {
		kprintf("fdd: controller is not responding\n");
		return;
	}

	if (fdd_version() != FDD_VERSION_EXT) {
		kprintf("fdd: controller not supported; pre-82077\n");
		return;
	}

	kprintf("fdd: found 82077 controller\n");

	/* save bios parameters, which are pointed to by the int 1E
	 * vector
	 */
	_fmemcpy(
		&biosparm,
		*(void __far * __far *)MKFP(0, 0x78),
		sizeof(biosparm)
	);

	/* we have a controller so install the interrupt handler and
	 * reset it.
	 */
	intr_add_handler(FDD_IRQ, fdd_intr);
	if (fdd_ctrl_reset() != 0) {
		kprintf("fdd: failed to reset controller.\n");
		return;
	}

	mt = task_start((task_entry)&fdd_motor_task, 128, TASK_PRI_BOOST);
	dprintf(("motor_task %p\n", mt));

	work_buf = mem_dma_alloc(FDD_SECTOR_SIZE);
	if (work_buf == NULL) {
		panic("fdd: couldn't allocate dma work buffer\n");
	}

	/* recalibrate all the drives 
	 */
	for (i = 0; i < nunits; i++) {
		if (fdd_recalibrate(&units[i]) != 0) {
			dprintf(("failed to recal drive %d\n", units[i].drive));
		}
	}
}

/* the interrupt handler
 */
int fdd_intr(unsigned irq)
{
	UNREFERENCED(irq);

	con_status(30, "INTR ");
	dprintf(("fdd_intr: "));

	if (ctrl_state & FDD_CS_RESET) {
		dprintf(("reset\n"));
		con_status(30, "RESET");
		fdd_intr_reset();
		con_status(30, "     ");
		return 0;
	}

	if (ctrl_state & FDD_CS_SEEKING) {
		dprintf(("seek\n"));
		con_status(30, "SEEK ");
		fdd_intr_seek();
		con_status(30, "     ");
		return 0;
	}

	if (iounit != NULL) {
		con_status(30, "I/O  ");
		dprintf(("io\n"));
		if (fdd_read_bytes(iounit->result, iounit->nexpresult) != 0) {
			dprintf(("fdd io: failed to read result\n"));
			iounit->nresult = 0;
		} else {
			iounit->nresult = iounit->nexpresult;
		}
		
		if (iounit->waiter.head == NULL) {
			dprintf(("fdd io: iounit is not waiting for unblock\n"));
		} else {
			task_unblock(iounit->waiter.head);
		}
		iounit = NULL;
		con_status(30, "     ");
		return 0;
	}

	con_status(30, "INTR?");
	return 0;
}

/* handle a controller reset interrupt
 */
void fdd_intr_reset(void)
{
	int i;
	uint8_t cmd = FDD_CMD_SENSE_INTR_STAT;
	uint8_t sense[2];

	/* per the docs, sending the configure command has to be done 
	 * fairly quickly after a reset or polling will have started and
	 * we need do the sense interrupt calls per drive. so do
	 * all that here right after the interrupt has fired.
	 */
	if ((ctrl_state & FDD_CS_CONFIG) == 0) {
		for (i = 0; i < 4; i++) {
			if (fdd_send_command(&cmd, sizeof(cmd)) != 0) {
				ctrl_state |= FDD_CS_TIMEOUT;
				break;
			}

			if (fdd_read_bytes(sense, sizeof(sense)) != 0) {
				ctrl_state |= FDD_CS_TIMEOUT;
				break;
			}
		}

		if (fdd_configure() != 0) {
			ctrl_state |= FDD_CS_TIMEOUT;
		}
	}

	ctrl_state &= ~FDD_CS_RESET;
	task_unblock(waiter.head);
}

/* handle an interrupt when one or more drives are seeking
 * (which includes recalibrate)
 */
void fdd_intr_seek(void)
{
	int i;
	uint8_t cmd = FDD_CMD_SENSE_INTR_STAT;
	uint8_t sense[2];

	/* otherwise, do a sense interrupt and see which unit is
	 * expecting something.
	 * 
	 * TODO synchronization!
	 */
	dprintf(("fdd_intr: sense interrupt\n"));
	if (fdd_send_command(&cmd, 1) != 0) {
		dprintf(("fdd_intr: failed to send sense interrupt\n"));
		return;
	}

	if (fdd_read_bytes(sense, sizeof(sense)) != 0) {
		dprintf(("fdd_intr: failed to read sense data\n"));
		return;
	}

	dprintf(("sense %02x %02x\n", sense[0], sense[1]));

	for (i = 0; i < nunits; i++) {
		fddunit_t *pu = &units[i];
		if ((sense[0] & 0x03) == pu->drive) {
			/* save ST0 and what cylinder we stopped on 
			 */
			pu->result[0] = sense[0];
			pu->result[1] = sense[1];
			pu->nresult = 2;

#ifdef FDD_DEBUG
			if (pu->state != fdd_seeking) {
				dprintf(("drive %d seek intr but isn't seeking\n", pu->drive));
			}
#endif
			/* TODO sync
			 */
			pu->state = fdd_seekdone;
			fdd_set_seeking();

			if (pu->waiter.head == NULL) {
				dprintf(("drive %d selected but not waiting.\n", pu->drive));
				return;
			}

			dprintf(("unblock drive %d\n", i));
			task_unblock(pu->waiter.head);
			return;
		}
	}

	dprintf(("there is no drive for the interrupt!\n"));
}

/*****************************************************************************
 * Sending or receiving command and result data to/from the controller is
 * a little complex. It's slow and stateful. To send bytes, the controller
 * has be ready to transfer bytes, and it wants the driver to wait until it
 * sets an "I'm ready to talk" bit for each byte. It also expects an exact
 * number of bytes to be sent or read in any particular context or it'll
 * lock up. If you try to send a command while the controller still has
 * result data, it'll lock up.
 * 
 * RQM (ReQuest for Master) is the bit that says the controller is ready
 * to talk. DIO (Direction of I/O) is the bit that says if the controller
 * wants to send back results or is ready for command bytes.
 */

/* wait for RQM indicating that the controller is ready to send
 * or receive a byte on its data port. returns the MSR byte
 * on success, or 0 on timeout.
 */
uint8_t fdd_wait_for_rqm(void)
{
	uint8_t __far *scr = MKFP(0xb800, 120);
	uint8_t msr;
	uint32_t t = ticks() + 2;

	/* this is loop polled because for the most part we don't
	 * expect to wait for more than a handful of iterations
	 *
	 * TODO this isn't going to work when called with
	 * interrupts off!!!!
	 */
	scr[1] = 0x1f;
	do {
		(*scr)++;
		msr = inb(FDD_STAT_PORT);
		scr[2] = (uint8_t)(0x30 | (msr >> 4));
		scr[4] = (uint8_t)(0x30 | (msr & 0x0f));
		if (msr & FDD_STAT_RQM) {
			break;
		}
	} while (ticks() <= t);
	scr[1] = 0x2f;

	if (ticks() >= t) {
		ctrl_state |= FDD_CS_TIMEOUT;
	}
	return msr;
}

/* send a multi-byte command to the controller 
 * returns 0 on success or an error code
 */
int fdd_send_command(uint8_t *data, int len)
{	
	dprintf(("send:"));
	while (len--) {
		uint8_t msr = fdd_wait_for_rqm();
		if (msr == 0) {
			dprintf(("fdd: controller timed out sending command\n"));
			return -1;
		}
		if (msr & FDD_STAT_DIO) {
			ctrl_state |= FDD_CS_STATE;
			dprintf(("fdd: state error, tried to send command but controller has result data\n"));
			return -1;
		}

		dprintf((" %02x", *data));
		outb(FDD_DATA_PORT, *data++);
	}
	dprintf(("\n"));

	return 0;
}

/* Attempt to read a byte from the FDC. Return -1 on timeout or
 * no data, else return the byte. 
 */
int fdd_read_bytes(uint8_t *buf, int len)
{
	dprintf(("read:"));
	while (len--) {
		uint8_t msr = fdd_wait_for_rqm();
		if (msr == 0x00) {
			/* timeout */
			return -1;	
		}

		if ((msr & FDD_STAT_DIO) == 0) {
			/* FDC has nothing ready to read */
			return -1;
		}

		*buf++ = inb(FDD_DATA_PORT);
		dprintf((" %02x", buf[-1]));
	}
	dprintf(("\n"));

	return 0;
}

/* flush any pending bytes ready to be pulled from the FDC to the 
 * port. this is part of reset.
 */
int fdd_flush(void)
{
	int i;
	uint8_t msr;

	for (i = 0; i < 32; i++) {
		msr = fdd_wait_for_rqm();

		if (msr == 0) {
			/* the RQM bit never went high, the controller is not
			 * responding.
			 */
			return -1;
		}

		if ((msr & FDD_STAT_DIO) == 0) {
			/* controller is ready for CPU to talk to it */
			return 0;
		}

		inb(FDD_DATA_PORT);
	}

	/* the controller never stopped giving us bytes */
	ctrl_state |= FDD_CS_STATE;
	return -1;
}

/* Send a version command, return -1 on error or the version
 */
int fdd_version(void)
{
	uint8_t cmd = FDD_CMD_VERSION;
	uint8_t version;

	if (fdd_send_command(&cmd, 1) != 0) {
		return -1;
	}

	if (fdd_read_bytes(&version, 1) != 0) {
		return -1;
	}

	return version;
}

/* Set the controller's configuration and lock it so resetting 
 * doesn't touch it. set the drive up to use the FIFO, to not
 * poll, and to do implied seeks on I/O operations.
 */

/* non-constant aggregation initializer */
#pragma warning( disable : 4204 )
int fdd_configure(void)
{
	const uint8_t conf1 =
		FDD_CONF_FIFO | 
		FDD_CONF_DPOLL |
		(8 & FDD_CONF_FIFO_THR);
	const uint8_t conf2 = 0; /* precomp */

	uint8_t cmd[] =
	{
		FDD_CMD_CONFIGURE,
		0x00,			/* first parameter must be zero */
		conf1,
		conf2,
		/* configure has no results phase
		 */
	};
	uint8_t lock_cmd = FDD_CMD_LOCK | FDD_LOCK_LOCK;
	uint8_t lock_result = 0;

	/* send configure 
	 */
	if (fdd_send_command(cmd, sizeof(cmd)) != 0) {
		return -1;
	}

	/* lock the configuration so reset won't change it later 
	 */
	if (
		fdd_send_command(&lock_cmd, 1) != 0 ||
		fdd_read_bytes(&lock_result, 1) != 0 ||
		lock_result != FDD_LOCK_RESULT_LOCKED
	) {
		return -1;
	}

	ctrl_state |= FDD_CS_CONFIG;

	return 0;
}

/* Reset the controller and wait for the resulting interrupt
 */
int fdd_ctrl_reset(void)
{
	/* reset the controller, which will generate an interrupt when
	 * done. also set the data rate (1M for any device we support)
	 */
	intr_cli();
	/* first turn the motor off, make sure reset is off and 
	 * interrupts/DMA are enabled (NB reset is active low)
	 * if we're booting off floppy, it's quite likely that
	 * BIOS has not turned the motor off, both because init
	 * happens quickly and because we've already stolen the 
	 * timer tick which would turn it off.
	 */

	outb(FDD_DOR_PORT, FDD_DOR_RESET | FDD_DOR_DMA);

	ctrl_state |= FDD_CS_RESET;
	outb(FDD_CCR_PORT, FDD_DSR_500K);
	outb(FDD_DSR_PORT, FDD_DSR_500K | FDD_DSR_RESET);

	/* TODO with timeout */
	task_block(&waiter);

	if (ctrl_state & FDD_CS_TIMEOUT) {
		kprintf("ctrl_reset: timeout\n");
		return -1;
	}

	return fdd_specify();
}

/* Sent a specify command, which sets drive parameters. Really we only
 * need to send this once, since we (as BIOS) don't vary these parameters
 * based on drive type, but we send it every time to be safe.
 * 
 * Originally Intel had this grand scheme where these values would change
 * over the life of the drive to account for it getting older and less
 * reliable (so the drive, early in its life, would be faster) but
 * that was never adopted by any OS.
 */
int fdd_specify(void)
{
	uint8_t specify[] =
	{
		FDD_CMD_SPECIFY,
		0,
		0
	};

	/* send a specify command for drive parameters. these are hard
	 * coded safe values which should work for any drive.
	 */
	specify[1] = biosparm.spec1;
	specify[2] = biosparm.spec2;

	if (fdd_send_command(specify, sizeof(specify)) != 0) {
		con_status(40, "SPEC");
		dprintf(("ctrl_reset: couldn't send specify\n"));
		return -1;
	}

	return 0;
}

/* Select a drive, and turn the motor on if it's off. If the motor
 * is already on, reset its motor off timeout to the max value so
 * it stays on.
 */
int fdd_seldrive(fddunit_t *pu)
{
	uint8_t motor = (uint8_t)FDD_DOR_MOT(pu->drive);
	uint8_t sel = (uint8_t)(FDD_DOR_DMA | FDD_DOR_RESET | FDD_DOR_DR(pu->drive));
	int need_sel = 0;

	dprintf(("seldrive %d\n", pu->drive));

	CS_ENTER(&motor_cs);

	/* if the drive is already selected and the motor is running,
	 * there's nothing to do
	 */	
	if (pu != seldrive || (motor_bits & motor) == 0) {
		motor_bits |= motor;
		sel |= motor_bits;
		outb(FDD_DOR_PORT, sel);
		need_sel = 1;
	}
	seldrive = pu;

	/* if the motor was off, wait for it to spin up. The mot_start
	 * parameter is in 1/8's of a second.
	 */
	if (need_sel) {
		int ms = (1000 / 8) * biosparm.mot_start;
		int ticks = TASK_MS_TO_TICKS(ms);
		task_sleep(ticks);
	}

	/* always reset the spindown timeout since we don't want the motor
	 * turned off mid-operation.
	 */
	pu->motor_tmo = biosparm.motoff_tmo;
	CS_LEAVE(&motor_cs);

	/* send specify since drive changed 
	 */
	if (fdd_specify() != 0) {
		return -1;
	}

	return 0;
}


/* Recalibrate the given drive. This brings the heads all the way back to
 * track 0 so they're in a known state.
 */
int fdd_recalibrate(fddunit_t *pu)
{
	fdd_recal_res_t *pres;
	const int recal_retry = 3;
	int rc = -1;
	int sti;
	int i, seeks;

	uint8_t recal[] =
	{
		FDD_CMD_RECALIBRATE,
		(uint8_t)FDD_RECAL_0(pu->drive)
	};

	if (pu->state != fdd_idle) {
		dprintf(("fdd%d: recalibrate called but unit busy (%d)\n", pu->drive, pu->state));
		return -1;
	}

	for (i = 0; i < recal_retry; i++) {
		if (fdd_seldrive(pu) != 0) {
			dprintf(("recal: fdd_seldrive failed\n"));
			continue;
		}

		sti = intr_cli();
		pu->state = fdd_seeking;
		pu->nexpresult = sizeof(*pres);
		ctrl_state |= FDD_CS_SEEKING;

		if (fdd_send_command(recal, sizeof(recal)) != 0) {
			dprintf(("recal: failed to send recal command\n"));
			intr_restore(sti);
			continue;
		}

		/* there's no result phase, we just wait for the interrupt
		 * TODO timeout!
		 */
		task_block(&pu->waiter);

		if (pu->nresult != sizeof(fdd_recal_res_t)) {
			dprintf(("recal: didn't get two result bytes\n"));
			continue;
		}

		pres = (fdd_recal_res_t*)pu->result;

		if ((pres->st0 & FDD_ST0_SEEK) == 0) {
			dprintf(("recal: interrupt was not for a seek\n"));
			continue;
		}

		if (FDD_ST0_IC(pres->st0) != FDD_ST0_IC_OK) {
			dprintf(("recal: failed with sense %02x\n", pres->st0));
			continue;
		}

		if (pres->cyl == 0) {
			rc = 0;
			break;
		}
		dprintf(("recal: cylinder at end %d; retry\n", pres->cyl));
	}

	dprintf(("recal done %d\n", rc));
	return rc;
}

int fdd_seek(fddunit_t *pu, unsigned head, unsigned cyl)
{
	int sti;
	uint8_t seek[3];

	seek[0] = FDD_CMD_SEEK;
	seek[1] = (uint8_t)FDD_SEEK_1(head, pu->drive);
	seek[2] = (uint8_t)FDD_SEEK_2(cyl);


	sti = intr_cli();
	ctrl_state |= FDD_CS_SEEKING;
	pu->state = fdd_seeking;

	if (fdd_send_command(seek, sizeof(seek)) != 0) {
		pu->state = fdd_idle;
		fdd_set_seeking();
		return -1;
	}

	/* TODO timeout 
	 */
	task_block(&pu->waiter);

	return 0;
}

/* Test to see if any unit is seeking, and if so, set the 
 * controller seeking flag, else clear the flag.
 */
void fdd_set_seeking(void)
{
	int sti = intr_cli();
	int i;

	ctrl_state &= ~FDD_CS_SEEKING;
	for (i = 0; i < nunits; i++) {
		if (units[i].state == fdd_seeking) {
			ctrl_state |= FDD_CS_SEEKING;
			break;
		}
	}

	intr_restore(sti);
}


int fdd_read(int unit, void __far *buf, blkno_t blk, int size)
{
	int ret;

	CS_ENTER(&fdd_cs);
	ret = fdd_op(
		unit,
		buf,
		blk,
		size,
		1		/* doread */
	);
	CS_LEAVE(&fdd_cs);

	return ret;
}

int fdd_write(int unit, void __far *buf, blkno_t blk, int size)
{
	int ret;

	CS_ENTER(&fdd_cs);
	ret = fdd_op(
		unit,
		buf,
		blk,
		size,
		0		/* doread */
	);
	CS_LEAVE(&fdd_cs);

	return ret;
}

int fdd_op(int unit, uint8_t __far *buf, blkno_t blk, int size, int doread)
{
	int rc = -1;
	int use_wb = 0;

	int sti;
	uint8_t dma_mode;
	uint32_t buflin;
	uint32_t nextpage;
	int seek_retry;
	int op_retry;
	unsigned sec_per_trk;
	uint16_t cyl, head, sec;
	fddunit_t *pu;
	fdd_io_result_t *pr;
	uint8_t opbuf[9];
	int need_recal = 0;
	int secs, maxsecs;

	if (size < 0 || (size % FDD_SECTOR_SIZE) != 0) {
		return -EINVAL;
	}

	if (size == 0) {
		return 0;
	}

	if (unit >= nunits) {
		return -ENODEV;
	}

	pu = &units[unit];

	switch (pu->type) {
	case CMOS_FDD_360 : sec_per_trk =  9; break;
	case CMOS_FDD_1_2 : sec_per_trk = 15; break;
	case CMOS_FDD_720 : sec_per_trk =  9; break;
	case CMOS_FDD_1_44: sec_per_trk = 18; break;
	default:
		return -EIO;
	}

	if (doread) {
		dma_mode = DMA_MODE_WRITE | DMA_MODE_DEMAND;
	} else {
		dma_mode = DMA_MODE_READ | DMA_MODE_DEMAND;
	}

	sec = (uint16_t)(1 + (blk % sec_per_trk));
	blk /= sec_per_trk;

	/* NB all drives have two heads. 
	 */
	head = (uint16_t)(blk & 0x01);
	blk >>= 1;

	cyl = (uint16_t)blk;

	/* figure out how much data we can actually transfer
	 */
	secs = size / FDD_SECTOR_SIZE;
	maxsecs = sec_per_trk - sec + 1;
	if (secs > maxsecs) {
		/* we can only read one track */
		secs = maxsecs;
	}

	/* if the buffer crosses a physical page, then we need to only transfer
	 * as many sectors as will fit before the sector that would be in the 
	 * part that crosses. If that's zero, then we use a work buffer that we 
	 * know doesn't cross, transfer just one sector, and copy the data
	 */
	buflin = KLINEAR(buf);
	nextpage = 1 + (buflin | 0xffff);
	maxsecs = (int)((nextpage - buflin) / FDD_SECTOR_SIZE);

	if (maxsecs == 0) {
		use_wb = 1;
		buflin = KLINEAR(work_buf);
		size = FDD_SECTOR_SIZE;
	} else if (secs > maxsecs) {
		secs = maxsecs;
	}

	size = secs * FDD_SECTOR_SIZE;

	/* build the 9-byte read or write data command 
	 */
	opbuf[0] = 
		(uint8_t)(
				FDD_OP_MFM | 
				(doread ? FDD_CMD_READ_DATA : FDD_CMD_WRITE_DATA));
	opbuf[1] = (uint8_t)((head << 2) | (pu->drive & 3));
	opbuf[2] = (uint8_t)cyl; 
	opbuf[3] = (uint8_t)head;
	opbuf[4] = (uint8_t)sec;
	opbuf[5] = biosparm.by_per_sec; /* 512b sectors */
	opbuf[6] = biosparm.eot;   		/* EOT */
	opbuf[7] = biosparm.gap; 		/* Gap length */
	opbuf[8] = biosparm.dtl; 		/* DTL - no short sectors */

	if (!doread && use_wb) {
		_fmemcpy(work_buf, buf, FDD_SECTOR_SIZE);
	}

#define ST(x) con_status(20, x)

	for (seek_retry = 0; seek_retry < FDD_RETRY && rc != 0; seek_retry++) {
		/* select drive and make sure motor is running
		 */
		if (fdd_seldrive(pu) != 0) {
			continue;
		}

		pu->state = fdd_idle;
		
		/* get to the right cylinder
		 */
		if (need_recal) {
			if (fdd_recalibrate(pu) != 0) {
				continue;
			}
		}
		need_recal = 0;

		if (fdd_seek(pu, head, cyl) != 0) {
			ST("SEK");
			continue;
		}
		
		/* do the operation
		 */
		for (op_retry = 0; op_retry < FDD_RETRY && rc != 0; op_retry++) {
			if (dma_setup(FDD_DRQ, buflin, size, dma_mode) != 0) {
				continue;
			}

			sti = intr_cli();
			iounit = pu;
			pu->state = fdd_io;
			pu->nexpresult = sizeof(fdd_io_result_t);

			if (fdd_send_command(opbuf, sizeof(opbuf)) != 0) {
				intr_restore(sti);
				continue;
			}

			/* TODO timeout */
			task_block(&pu->waiter);
			
			if (pu->nresult != pu->nexpresult) {
				continue;
			}
			
			pr = (fdd_io_result_t*)pu->result;
			if (FDD_ST0_IC(pr->st0) == FDD_ST0_IC_OK) {
				rc = 0;
				pu->state = fdd_idle;
			} else {
				if (pr->st2 & FDD_ST2_WRONGCYL) {
					/* if we're on the wrong cylinder, stop trying to r/w
					 * and go back and recalibrate. the heads aren't
					 * properly aligned.
					 */
					dprintf(("fdd: need recalibrate\n"));
					need_recal = 1;
					break;
				} else {
					dprintf(("fdd: some other error\n"));
				}
			}
		}
	}

	if (rc < 0) {
		kprintf("rc %d size %d\n", rc, size);
		return rc;
	}

	if (doread && use_wb) {
		_fmemcpy(buf, work_buf, FDD_SECTOR_SIZE);
	}


	return size;
}

/* Check if the given buffer, used as a floppy sector buffer,
 * would cross a DMA hardware buffer.
 */
int fdd_dma_cross(uint8_t __far *ptr)
{
	uint32_t seg = KSEG_OF(ptr);
	uint32_t off = KOFFS_OF(ptr);
	uint32_t linear = (seg << 4) | off;
	uint32_t page_start, page_end;

	page_start = linear & 0xF0000ul;
	linear += FDD_SECTOR_SIZE - 1;
	page_end = linear & 0xF0000ul;

	return page_start != page_end;
}

/* always running task to turn off motors after a period of inactivity
 */
#pragma warning( disable : 4702 )	/* unreachable code */
void fdd_motor_task(void)
{
	static int wakes = 0;
	const int sleep_quanta = 4;

	int i;
	fddunit_t *pu;
	uint8_t motor;
	uint8_t sel;

	for(;;) {
		task_sleep(sleep_quanta);
		for (i = 0; i < nunits; i++) {
			pu = &units[i];
			motor = (uint8_t)FDD_DOR_MOT(pu->drive);
			CS_ENTER(&motor_cs);
			if ((motor_bits & motor) != 0) {
				pu->motor_tmo -= sleep_quanta;
				if (pu->motor_tmo <= 0 && pu->state == fdd_idle) {
					dprintf(("motor off %d\n", pu->drive));
					motor_bits &= ~motor; 
					sel = (uint8_t)(motor_bits | FDD_DOR_DMA | FDD_DOR_RESET | FDD_DOR_DR(pu->drive));
					outb(FDD_DOR_PORT, sel);
				}
			}
			CS_LEAVE(&motor_cs);
		}
	}
}
