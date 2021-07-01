#include "tty.h"

#include "device.h"
#include "intr.h"
#include "ktypes.h"
#include "memory.h"
#include "ports.h"
#include "printk.h"
#include "task.h"
#include "ttycook.h"

#include <errno.h>
#include <string.h>

/*
 * 8250/16550 register set
 */
#define UART_XMIT	0
#define UART_RECV	0
#define UART_DIVLOW	0
#define UART_IER	1
#define UART_DIVHI	1
#define UART_FIFO   2
#define UART_IIR	2
#define UART_LCR	3
#define UART_MCR	4
#define UART_LSR	5
#define UART_MSR	6
#define UART_SCR	7

#define IER_RECV	0x01		/* interrupt when RX data ready */
#define IER_XMIT	0x02		/* interrupt when TX enmpty */


#define FIFO_EN_FIFO	0x01
#define FIFO_XRESET		0x02
#define FIFO_TRESET		0x04
#define FIFO_DMA		0x08
#define FIFO_RTRIG_LO	0x40
#define FIFO_RTRIG_HI	0x80

#define IIR_8250_MASK 	0x07	
#define IIR_16550_MASK	0x0f

#define IIR_NONE	0x01
#define IIR_MSR		0x00
#define IIR_TXRDY	0x02
#define IIR_RXRDY	0x04
#define IIR_RXTMO	0x0c
#define IIR_LSR		0x06

/* how many times to retry to clear the IIR before deciding this
 * isn't a working port.
 */
#define IIR_RETRY	10

#define LCR_DLAB	0x80

#define LCR_BITS_5	0x00
#define LCR_BITS_6	0x01
#define LCR_BITS_7	0x02
#define LCR_BITS_8	0x03

#define LCR_STOP_1	0x00
#define LCR_STOP_2  0x04		/* 1.5 if 5 bit words */

#define LCR_NOPAR   0x00
#define LCR_EPAR	0x18
#define LCR_OPAR	0x08

#define MCR_DTR		0x01
#define MCR_RTS		0x02
#define MCR_OP2		0x08

#define LSR_TX_RDY  0x20
#define LSR_RX_RDY  0x01

#define FIFO_ENABLE 0x01

#define FIFO_SIZE	16		/* for both transmit and receive */

#define IIR_TYPE_MASK 0xC0
#define IIR_16550	0x80
#define IIR_16550A  0xC0

#define DIVISOR	115200

#define DEF_BAUD 19200
#define DEF_LCR (LCR_NOPAR | LCR_BITS_8 | LCR_STOP_1)

/* Possible locations for UARTs
 */
typedef struct tty_spec_t tty_spec_t;
struct tty_spec_t {
	uint16_t port;
	int irq;
};

/* Standard places for UART's to live.
 */
static tty_spec_t specs[] = {
	{ 0x3f8, 4 },
	{ 0x2f8, 3 },
	{ 0x3e8, 4 },
	{ 0x2e8, 3 },
};

#define N_SPECS (sizeof(specs) / sizeof(specs[0]))

/* A circular queue of characters. If head == tail, then 
 * empty. Must be a power of 2!
 */
#define QUEUE_SIZE 32

typedef struct tty_queue_t tty_queue_t;
struct tty_queue_t {
	int head;
	int tail;
	uint8_t data[QUEUE_SIZE];
};

#define Q_NEXT(ptr) (((ptr) + 1) & (QUEUE_SIZE-1))
#define Q_FULL(q) (Q_NEXT((q)->tail) == (q)->head)
#define Q_EMPTY(q) ((q)->tail == (q)->head)

/* One UART
 */
typedef struct tty_unit_t tty_unit_t;
struct tty_unit_t {
	/* common */
	uint32_t baud;
	uint16_t base_port;
	uint8_t lcr;
	uint8_t iir_mask;
	uint16_t irq;
	critsect_t refcs;
	int zombcount;

	/* flags */
	unsigned is_16550a : 1;
	unsigned xmit : 1; 
	unsigned open : 1;

	/* receive */
	taskq_t readers;
	ttycook_t cooker;
	critsect_t csrecv;

	/* transmit */
	tty_iob_t *txhead;
	tty_iob_t *txtail;
	tty_iob_t *freeiob;
	critsect_t csxmit;

	/* stats */
	uint16_t interrupts;
	uint16_t txintr;
	uint16_t rxrdy;
	long rxbytes;
	long txbytes;
};

static tty_unit_t *units[N_SPECS];
static int n_units;
static uint16_t added_handler;

static void tty_init(void);

#define PROBE_NO_TTY 	0
#define PROBE_8250 		1
#define PROBE_16550A 	2
static int tty_probe(tty_spec_t *spec);

static int tty_open(int unit);
static int tty_close(int unit);
static int tty_intr(unsigned irq);
static void tty_recv_intr(tty_unit_t *pu);
static void tty_xmit_intr(tty_unit_t *pu);
static int tty_setrate(int unit, uint32_t baud, uint8_t lcrbits);
static int tty_write(int unit, void __far *buf, int size);
static int tty_read(int unit, void __far *buf, int size);
static void tty_freeiob(void);

chardev_t ttydev = {
	&tty_init,
	&tty_open,
	&tty_close,
	&tty_read,
 	&tty_write,
};

void tty_init(void)
{
	int i, probe;
	tty_unit_t *pu;

	for (i = 0; i < N_SPECS; i++) {
		probe = tty_probe(&specs[i]);
		if (probe == PROBE_NO_TTY) {
			continue;
		}

		kprintf("tty%d: found %s UART\n", 
			n_units, 
			probe == PROBE_16550A ? "16550A" : "8250");

		pu = near_malloc(sizeof(tty_unit_t)); 
		if (pu == NULL) {
			kprintf("tty%d could not be created - out of kernel memory\n", n_units);
			continue;
		}

		memset(pu, 0, sizeof(*pu));
		pu->base_port = specs[i].port;
		pu->irq = specs[i].irq;
		pu->is_16550a = probe == PROBE_16550A;
		pu->iir_mask = (uint8_t)(pu->is_16550a ? IIR_16550_MASK : IIR_8250_MASK);
		CS_INIT(&pu->csxmit);
		CS_INIT(&pu->csrecv);
		CS_INIT(&pu->refcs);
		taskq_init(&pu->readers);
		pu->cooker.write = &tty_write;
		pu->cooker.unit = n_units;

		units[n_units++] = pu;

		tty_setrate(n_units-1, DEF_BAUD, DEF_LCR);
	}
}

/* Look for a UART at the given hardware spec. Return if the device
 * doesn't exist (PROBE_NO_TTY), is an old chip (PROBE_8250), or is
 * a decent chip (PROBE_16550A).
 */
int tty_probe(tty_spec_t *spec)
{
	uint16_t port = spec->port;
	int i;
	int is_16550a = 0;
	uint16_t mask;

	/* detect via flipping DLAB
	*/
	outb(port + UART_LCR, LCR_DLAB);
	outb(port + UART_DIVHI, 0xff);
	
	if (inb(port + UART_DIVHI) != 0xff) {
		return PROBE_NO_TTY;
	}

	outb(port + UART_LCR, 0x00);
	if (inb(port + UART_IER) & 0xF0 != 0) {
		/* these bits always read low */
		return PROBE_NO_TTY;
	}

	/* check for the existence of a 16550A. The 16550 has bugs in the 
	 * FIFO so we'll treat it as an 8250. The -A revision is ok.
	 */
	is_16550a = 1;

	outb(port + UART_SCR, 0x55);
	is_16550a = inb(port + UART_SCR) == 0x55;

	if (is_16550a) {
		outb(port + UART_SCR, 0xaa);
		is_16550a = inb(port + UART_SCR) == 0xaa;
	}

	if (is_16550a) {
		outb(port + UART_FIFO, FIFO_ENABLE);
		is_16550a = (inb(port + UART_IIR) & IIR_TYPE_MASK) == IIR_16550A;
	}

	/* check that we can clear all interrupts. if we can't,
	 * the device is busted.
	 */
	mask = is_16550a ? IIR_16550_MASK : IIR_8250_MASK;
	for (i = 0; i < IIR_RETRY; i++) {
		if ((inb(port + UART_IIR) & mask) == IIR_NONE) {
			break;
		}
	}

	if (i == IIR_RETRY) {
		return PROBE_NO_TTY;
	}

	return is_16550a ? PROBE_16550A : PROBE_8250;
}

int tty_open(int unit)
{
	tty_unit_t *pu;
	int i;
	uint16_t mask;
	uint16_t port;

	if (unit < 0 || unit >= n_units) {
		return -ENODEV;
	}

	pu = units[unit];

	CS_ENTER(&pu->refcs);
	if (pu->open || pu->readers.head) {
		CS_LEAVE(&pu->refcs);
		return -EBUSY;
	}
	pu->open = 1;
	CS_LEAVE(&pu->refcs);

	port = pu->base_port;

	/* we just opened the device when it was closed, set it up
	 */
	if (pu->is_16550a) {
		/* using the 16550, make sure the FIFO is on. clear it and 
		 * set the interrupt threshold.
		 */
		outb(port + UART_FIFO, 
			FIFO_ENABLE | 					/* use FIFO */
			FIFO_TRESET | FIFO_XRESET | 	/* clear them out */
			FIFO_RTRIG_HI | FIFO_RTRIG_LO); /* use most of receiver */
	}

	/* clear any pending interrupts
	 */
	for (i = 0; i < IIR_RETRY; i++) {
		if ((inb(port + UART_IIR) & pu->iir_mask) == IIR_NONE) {
			break;
		}
	}

	if (i == IIR_RETRY) {
		/* we couldn't clear interrupts. fail.
		 */
		pu->open = 0;
		return -EIO;
	}

	ttyc_init(&pu->cooker);

	/* On the PC we need to set OP2 (output 2) to enable the
	 * UART's interrupt line on the bus. Also set the DTR and
	 * RTS to let whatever's hooked up to the port know we're
	 * ready to chat.
	 */
	outb(port + UART_MCR, MCR_DTR | MCR_RTS | MCR_OP2);

	/* Hook up the interrupt handler and enable interrupts
	 */
	mask = 1 << pu->irq;
	if ((added_handler & mask) == 0) {
		intr_add_handler(pu->irq, &tty_intr);
		added_handler |= mask;
	}

	outb(port + UART_IER, IER_RECV | IER_XMIT);

	return 0;
}

int tty_close(int unit)
{
	tty_unit_t *pu;
	task_t *rdr;
	uint16_t port;

	if (unit < 0 || unit >= n_units) {
		return -ENODEV;
	}

	pu = units[unit];

	CS_ENTER(&pu->refcs);
	if (!pu->open) {
		CS_LEAVE(&pu->refcs);
		return -EBADF;
	}
	CS_LEAVE(&pu->refcs);
	
	port = pu->base_port;

	/* turn off interrupts. we will not take away the handler
	 * as interrupts are shared between ports and it doesn't
	 * really hurt anything to leave the handler in place. 
	 * also pull down RTS and DTR to let any peer know we're
	 * closed.
	 */
	outb(port + UART_IER, 0);
	outb(port + UART_MCR, 0);

	/* if there are pending iob's, free them. this is normally
	 * an unwanted state as it means we closed before all the
	 * data was sent.
	 */
	if (pu->txhead) {
		pu->txtail->next = pu->freeiob;
		pu->freeiob = pu->txhead;
		pu->txhead = NULL;
		pu->txtail = NULL;
	}		
	tty_freeiob();

	/* at this point, we will never get another interrupt,
	 * so unblock all the readers. zombcount is used to 
	 * hold the unit structure until all the readers have
	 * been scheduled and exited, else we could reuse the unit
	 * while a previous reader was still out there.
	 */
	pu->zombcount = 0;
	rdr = pu->readers.head;
	if (rdr) {
		do {
			pu->zombcount++;
			rdr = rdr->next;
		} while (rdr != pu->readers.head);

		while (pu->readers.head) {
			task_unblock(pu->readers.head);
		}
	}

	pu->open = 0;

	return 0;
}

/* TTY interrupt handler
 */
int tty_intr(unsigned irq)
{
	uint8_t __far *spinner = MKFP(0xb800, 140);
	int i;
	tty_unit_t *pu;
	uint8_t iir;

	(*spinner)++;

	for (i = 0; i < n_units; i++) {
		pu = units[i];
		if (pu->irq == irq) {
			pu->interrupts++;
			for(;;) {
				iir = (uint8_t)(inb(pu->base_port + UART_IIR) & pu->iir_mask);
				if (iir == IIR_NONE) {
					break;
				}

				switch (iir) {
				case IIR_RXRDY:
				case IIR_RXTMO:
					CS_ENTER(&pu->csrecv);
					tty_recv_intr(pu);
					CS_LEAVE(&pu->csrecv);
					break;
					
				case IIR_TXRDY:
					pu->txintr++;
					CS_ENTER(&pu->csxmit);
					tty_xmit_intr(pu);
					CS_LEAVE(&pu->csxmit);
					break;
				
				default:
					*spinner = iir;
					break;
				}
			}
		}
	}

	return 0;
}

void tty_recv_intr(tty_unit_t *pu)
{
	int got = 0;
	uint16_t base = pu->base_port;

	pu->rxrdy++;

	while (inb(base + UART_LSR) & LSR_RX_RDY) {
		pu->rxbytes++;

		ttyc_putc(&pu->cooker, inb(base + UART_RECV));
		got++;
	}

	if (got) {
		/* if there are readers waiting, wake them up
		 */
		while (pu->readers.head) {
			task_unblock(pu->readers.head);
		}
	}
}

void tty_xmit_intr(tty_unit_t *pu)
{
	uint16_t port = pu->base_port;
	tty_iob_t *iob;
	int i;
	
	iob = pu->txhead;

	/* if the FIFO isn't empty or there's no iob,
	 * there's nothing to do 
	 */
	if (iob == NULL) {
		return;
	}

	if ((inb(port + UART_LSR) & LSR_TX_RDY) == 0) {
		return;
	}

	/* fill the FIFO as much as we can 
	 */
	for (i = 0; i < FIFO_SIZE; i++) {
		while (iob && iob->ptr == iob->size) {
			tty_iob_t *old = iob;
			iob = iob->next;

			old->next = pu->freeiob;
			pu->freeiob = old;

			pu->txhead = iob;
			if (iob == NULL) {
				pu->txtail = NULL;
			}
		}

		if (iob == NULL) {
			break;
		}

	 	outb(port + UART_XMIT, iob->data[iob->ptr++]);
	}
}

/* Write to the device (right now, synchronously) 
 */
int tty_write(int unit, uint8_t __far *buf, int size)
{
	tty_unit_t *pu;
	tty_iob_t *iob;

	tty_freeiob();

	if (unit < 0 || unit >= n_units) {
		return -ENODEV;
	}

	if (size == 0) {
		return 0;
	}

	pu = units[unit];

	iob = ttyc_cook_output(buf, size);
	if (iob == NULL) {
		return -ENOMEM;
	}

	iob->ptr = 0;
	iob->next = NULL;

	CS_ENTER(&pu->csxmit);
	if (pu->txhead) {
		pu->txtail->next = iob;
		pu->txtail = iob;
	} else {
		pu->txhead = iob;
		pu->txtail = iob;
		tty_xmit_intr(pu);
	}
	CS_LEAVE(&pu->csxmit);

	return size;
}

/* Read from the device (right now, synchronously) 
 */
int tty_read(int unit, uint8_t __far *buf, int size)
{
	int got;

	tty_unit_t *pu;

	tty_freeiob();

	if (unit < 0 || unit >= n_units) {
		return -ENODEV;
	}

	pu = units[unit];

	if (size == 0) {
		return 0;
	}

	for(;;) {
		got = 0;
		
		CS_ENTER(&pu->csrecv);
		got = ttyc_read(&pu->cooker, buf, size);
		CS_LEAVE(&pu->csrecv);

		if (got) {
			return got;
		}

		task_block(&pu->readers);
		
		/* check to see if the device was closed out from under us
		 */
		CS_ENTER(&pu->refcs);
		if (pu->zombcount) {
			/* if zombcount is non-zero, the device was closed
				* with readers still waiting. when zombcount reaches
				* zero, everything is cleaned up.
				*/
			--pu->zombcount;
			CS_LEAVE(&pu->refcs);
			return -EIO;
		}
		CS_LEAVE(&pu->refcs);
	}
}

/* set the serial protocol parameters
 */
int tty_setrate(int unit, uint32_t baud, uint8_t lcrbits)
{
	uint16_t base;
	uint16_t rate;

	if (unit < 0 || unit >= n_units) {
		return -ENODEV;
	}

	base = units[unit]->base_port;
	rate = (uint16_t)(DIVISOR / baud);

	units[unit]->baud = baud;
	units[unit]->lcr = lcrbits;

	outb(base + UART_LCR, LCR_DLAB);
	outb(base + UART_DIVLOW, (uint8_t)(rate & 0xff));
	outb(base + UART_DIVHI, (uint8_t)(rate >> 8));
	outb(base + UART_LCR, lcrbits);
}

/* opportunistic free'ing of iob's
 */
void tty_freeiob(void)
{
	int i;

	for (i = 0; i < n_units; i++) {
		tty_unit_t *pu = units[i];

		while (pu->freeiob) {
			tty_iob_t *iob = pu->freeiob;
			pu->freeiob = iob->next;
			kfree(iob->data);
			near_free(iob);
		}
	}
}

void tty_dump()
{
	int i, j;
	int r;
	task_t *pt;

	for (i = 0; i < n_units; i++) {
		tty_unit_t *pu = units[i];

		r = 0;
		if (pu->readers.head) {
			pt = pu->readers.head;
			do {
				r++;
				pt = pt->next;
			} while (pt != pu->readers.head);
		}

		kprintf("tty%d %lu %04x irq%d %s\n",
			i,
			pu->baud,
			pu->base_port,
			pu->irq,
			pu->is_16550a ? "16550A" : "8250");
		
		kprintf("  rx:  rxrdy %d rxbytes %lu readers %d\n",
			pu->rxrdy,
			pu->rxbytes,
			r);

		kprintf("  tx:  txintr %u txbytes %lu\n",
			pu->txintr,
			pu->txbytes);

		kprintf("  irq: iirmask %02x interrupts %d\n",
			pu->iir_mask,
			pu->interrupts);

		kprintf("  prt: ");
		for (j = 0; j < 8; j++) {
			kprintf("%02x ", inb(pu->base_port + j) & 0xff);
		}
		kprintf("\n");
	}
}
