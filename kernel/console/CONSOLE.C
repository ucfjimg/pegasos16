#include "console.h"

#include "dbview.h"
#include "device.h"
#include "intr.h"
#include "ports.h"
#include "printk.h"
#include "task.h"

#include <errno.h>
#include <stdarg.h>
#include <string.h>

/* inline assembly */
#pragma warning( disable : 4704 )

static void con_init(void);
static int con_open(int unit);
static int con_close(int unit);
static int con_read(int unit, void __far *buf, int size);

extern int con_write(int unit, void __far *buf, int size);

chardev_t condev = {
	&con_init,
	&con_open,
	&con_close,
	&con_read,
	&con_write
};

#define KBD_IRQ     1
#define KBD_DATA    0x60
#define KBD_CMD     0x64

#define KBD_CMD_DRDY	0x01

#define MAXQ 32

static uint8_t scqueue[MAXQ];
static int qhead;
static int qtail;
static int lost;

static uint8_t prefix = 0;
static uint16_t meta = 0;
static uint8_t keyqueue[MAXQ];
static int kqhead = 0;
static int kqtail = 0;
static taskq_t readers;

int con_initted = 0;
critsect_t con_statcs;

#define SHIFTLOCK(x) ((x) & (SC_META_CAPSLOCK | SC_META_NUMLOCK | SC_META_SCRLOCK))
#define SHIFTED() (meta & (SC_META_LSHIFT | SC_META_RSHIFT))
#define CTRLED() (meta & (SC_META_LCTRL | SC_META_RCTRL))
#define CAPSLOCK() (meta & SC_META_CAPSLOCK)
#define NUMLOCK() (meta & SC_META_NUMLOCK)

#define CTRLMASK 0x1f


static int kbd_intr(unsigned intr);

/* public for debugger */
void con_descan(uint8_t sc);
static void con_descan_all(void);


void con_init(void)
{
    qhead = 0;
    qtail = 0;
	kqhead = 0;
	kqtail = 0;
    lost = 0;

	taskq_init(&readers);
    intr_add_handler(KBD_IRQ, &kbd_intr);

	CS_INIT(&con_statcs);

	con_initted = 1;
}

/* unlike the TTY drivers, we will always use the console, so
 * the open() and close() calls are just stubs that always 
 * succeed.
 */
int con_open(int unit)
{
	UNREFERENCED(unit);
	return 0;
}

int con_close(int unit)
{
	UNREFERENCED(unit);
	return 0;
}

int kbd_intr(unsigned intr)
{
    uint8_t data;
	int got = 0;
    UNREFERENCED(intr);

	while (inb(KBD_CMD) & KBD_CMD_DRDY) {
		data = inb(KBD_DATA);

		if (data == 0x3b) {
            /* we're going to enter the debugger, which is going to 
             * hook the keyboard interrupt out from under us and 
             * expect to get input. So we need to EOI. 
             */
            intr_cli();
            intr_eoi(KBD_IRQ);
            /* throw away everything */
            qhead = 0;
            qtail = 0;
            kqhead = 0;
            kqtail = 0;

            /* this will not return until the debugger session is over.
             */
            _asm int 3;
			
            /* returning non-zero here means that the interrupt 
             * service code will not EOI on our behalf 
             */
            return 1;
		}

		/* TODO LED's should be switched here */

		/* index variables are protected by cli */
		if (((qtail + 1) % MAXQ) == qhead) {
			lost++;
			return 0;
		}

    	scqueue[qtail] = data;
    	qtail = (qtail + 1) % MAXQ;
		got++;
	}

	/* if there was data, wake up the reader(s). For a character device,
	 * if there are mutiple readers, it's up to queue ordering who gets
	 * data.
	 */
	if (got) {
		while (readers.head) {
			/* task unblock removes the task from the blocked queue 
			*/
			task_unblock(readers.head);
		}
	}

    return 0;
}

/* Remove keys from the scancode queue and buffer them as 
 * ASCII + extended characters
 */
void con_descan(uint8_t sc)
{
	scandef_t *psc;
	uint8_t key;
	int up;
	int ext;
	int exto;
	int shifted;

	if (((kqtail + 1) % MAXQ) == kqhead) {
		/* outgoing queue is full */
		return;
	}	

	/* is it an extended key prefix? remember for next key. */
	if (sc == SC_EXT_PREFIX) {
		prefix = SC_EXT_PREFIX;
		return;
	}

	ext = (prefix == SC_EXT_PREFIX);
	prefix = 0;

	/* is it a key-up event? the scancode is all the other bits. */
	up = 0;
	if (sc & SC_KEYUP) {
		sc &= ~SC_KEYUP;
		up = 1;
	}

	/* if it's a scancode we don't understand, ignore it. */
	if (sc > nscascii) {
		return;
	}

	psc = &scascii[sc];

	/* If it was prefixed with E0 and we don't have a table
	 * entry for it, ignore it.
	 * TODO print screen and pause/break are special cases
	 * here
	 */
	if (ext && ((psc->flags & SC_E0) == 0)) {
		return;
	}

	/* if it's a meta key, then all it does is toggle the proper
	 * flags in the shift flags 
	 */
	if (psc->flags & SC_META) {
		uint16_t metabit = SC_GET_META(psc);
		if (ext) {
			if (metabit == SC_META_LCTRL) {
				metabit = SC_META_RCTRL;
			} else if (metabit == SC_META_LALT) {
				metabit = SC_META_RALT;
			}
		}

		if (SHIFTLOCK(metabit)) {
			if (!up) {
				meta ^= metabit;
			}
		} else if (up) {
			meta &= ~metabit;
		} else {
			meta |= metabit;
		}
		return;
	}

	/* for all other keys, we don't want to do anything on release */
	if (up) {
		return;
	}	

	exto = psc->flags & SC_EXT;

	if ((psc->flags & SC_CTRL) && CTRLED()) {
		key = (uint8_t)(psc->ascii & CTRLMASK);
	} else if (psc->flags & SC_NUML) {
		if (ext) {
			/* a prefixed key on the numpad is always the extendded
			 * key version
			 */
			key = psc->shifted;
			exto = 1;
		} else {
			if (NUMLOCK()) {
				key = psc->ascii;
			} else {
				key = psc->shifted;
				exto = 1;
			}
		}
	} else {
		shifted = 0;
		if (CAPSLOCK() && (psc->flags & SC_CAPSLOCK)) {
			shifted = !shifted;
		}

		if (SHIFTED() && (psc->flags & SC_SHIFTS)) {
			shifted = !shifted;
		}

		key = (char)(shifted ? psc->shifted : psc->ascii);
	}

	/* TODO constant - marks key as extended for user reader
	 * process
	 */
	if (exto) {
		key |= 0x80;
	}
	
	keyqueue[kqtail] = key;
	kqtail = (kqtail + 1) % MAXQ;
} 

void con_descan_all(void)
{
    uint8_t sc;

	for(;;) {
		intr_cli();
		sc = 0;
		if (qhead != qtail) {
			sc = scqueue[qhead];
			qhead = (qhead + 1) % MAXQ;
		}
		intr_sti();

		if (sc == 0) {
			break;
		}

		con_descan(sc);
	}
}

int con_read(int unit, uint8_t __far *buf, int size)
{
	int got = 0;
	uint8_t by = 0;
    int gotby = 0;

    if (unit != 0) {
        return -ENODEV;
    }

	if (size == 0) {
		return 0;
	}

	for(;;) {
		con_descan_all();

		if (kqhead != kqtail) {
			break;
		}
		task_block(&readers);
	}

	while (kqhead != kqtail && size--) {
		by = keyqueue[kqhead];
		kqhead = (kqhead + 1) % MAXQ;
        *buf++ = by;
        got++;
    }

	return got;
}

/* A non-blocking read. Returns 0 on no key. This is exclusively for use by
 * the kernel.
 */
uint8_t con_getch(void)
{
	int sti;
	uint8_t by = 0;

	sti = intr_cli();
	con_descan_all();
	if (kqhead != kqtail) {
		by = keyqueue[kqhead];
		kqhead = (kqhead + 1) % MAXQ;
	}
	intr_restore(sti);
	return by;
}
