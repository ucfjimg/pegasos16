#include "intr.h"

#include "ktypes.h"
#include "memory.h"
#include "ports.h"
#include "printk.h"
#include "task.h"

#include <errno.h>
#include <string.h>

#define LOPIC 0x20
#define HIPIC 0xa0

#define EOI 0x20

#define STACKSIZE 512

typedef struct intr_t intr_t;

struct intr_t {
    intrfn_t handler;
    intr_t *next;
};

typedef struct intrhead_t intrhead_t;
struct intrhead_t {
    intr_t *handlers;
    task_t *task;
};

intrhead_t intrs[INTR_N];

typedef void (__far* handler_stub_t)(void);
extern handler_stub_t intr_get_stub(int irq);

/* Initialize the interrupts.
 */
void intr_init(void)
{
    /* NB BIOS has set up the interrupt controller w/ proper modes for legacy mode
     * operation - vectors 0x08..0x0f for interrupts 0..7 and 0x70..0x77 for 
     * 8..15. We will leave these alone.
     */
    memset(intrs, 0, sizeof(intrs));
}

/* Add an interrupt handler for the given interrupt. `intr` is an
 * interrupt NUMBER, not a vector. (e.g. 0..15)
 */

int intr_add_handler(unsigned intr, intrfn_t fn)
{
    intr_t *node;
    int vec = (intr < 8) ? (8 + intr) : (0x70 + intr - 8);
    uint16_t ocw1_port;
    uint8_t ier;

    handler_stub_t stub = intr_get_stub(intr);
    handler_stub_t __far *pvec = (handler_stub_t __far *)MKFP(0, vec * 4);

    uint32_t __far *p = (uint32_t __far *)MKFP(0, vec * 4);

    if (stub == NULL) {
        return -EINVAL;
    }

    node = near_malloc(sizeof(intr_t));
    if (node == NULL) {
        return -ENOMEM;
    }

    node->handler = fn;

    intr_cli();

    if (intrs[intr].task == NULL) {
        /* NB intrrupt number is task priority */
        intrs[intr].task = task_reserve(STACKSIZE, (taskpri_t)intr);
        intrs[intr].task->state = TS_BLOCKED;
        
        /* enable the interrupt at the PIC */
        ocw1_port = 1 + (intr < 8 ? LOPIC : HIPIC);
        ier = inb(ocw1_port);
        ier &= ~(1 << (intr & 0x07));
        outb(ocw1_port, ier);
    }

    node->next = intrs[intr].handlers;
    intrs[intr].handlers = node;

    *pvec = stub;

    intr_sti();

    return 0;
}

/* return the current mask of both pics
 */
unsigned intr_get_mask(void)
{
    return (inb(HIPIC + 1) << 8) | inb(LOPIC + 1);
}

/* given the return value from intr_get_mask,
 * restore the masks to that state
 */
void intr_set_mask(unsigned mask)
{
    uint8_t lomask = (uint8_t)mask;
    uint8_t himask = (uint8_t)(mask >> 8);
    outb(LOPIC + 1, lomask);
    outb(HIPIC + 1, himask);
}

/* enable interrupts from the given irq
 */ 
void intr_mask_enable(unsigned intr)
{
    unsigned ocw1_port;
    uint8_t ier;

    ocw1_port = 1 + (intr < 8 ? LOPIC : HIPIC);
    ier = inb(ocw1_port);
    ier &= ~(1 << (intr & 0x07));
    outb(ocw1_port, ier);
}

/* disable interrupts from the given irq
 */ 
void intr_mask_disable(unsigned intr)
{
    unsigned ocw1_port;
    uint8_t ier;

    ocw1_port = 1 + (intr < 8 ? LOPIC : HIPIC);
    ier = inb(ocw1_port);
    ier |= (1 << (intr & 0x07));
    outb(ocw1_port, ier);
}

#pragma warning( disable : 4704 )
void intr_handler(int intr)
{
    intr_t *node;
    int eoi = 1;

    for (node = intrs[intr].handlers; node; node = node->next) {
        /* NB drivers should NOT, in general, EOI themselves and return 
         * non-zero. This facility is only here for the special case 
         * of interrupts that are taken over by the debugger.
         */
        if (node->handler(intr) != 0) {
            eoi = 0;
        }
    }

    /* EOI - end of interrupt. after this the interrupt can recur
     * cli here to prevent that happening until we've fully switched away
     * from this task 
     */
    intr_cli();

    if (eoi) {
        intr_eoi(intr);
    }
 
    /* this will return to the task scheduler, which will switch
     * away to something else 
     */
}

/* this sends an EOI to the interrupt controller, telling it that we're
 * finished servicing this interrupt.
 */
void intr_eoi(int irq)
{
    if (irq >= 8) {
        outb(HIPIC, EOI);
    }
    outb(LOPIC, EOI);
}
