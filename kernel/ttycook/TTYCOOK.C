#include "ttycook.h"

#include "device.h"
#include "memory.h"
#include "printk.h"
#include "task.h"

#include <errno.h>

#define Q_NEXT(x, sz) (((x) + 1) & ((sz) - 1))
#define Q_PREV(x, sz) (((x) + (sz) - 1) & ((sz) - 1))


#define TTYC_ECHO(pc, ch) \
    { \
        CS_ENTER(&(pc)->echocs); \
        (pc)->echobuf[(pc)->etail] = (ch); \
        (pc)->etail = Q_NEXT((pc)->etail, TTYC_ECHO_QUEUE); \
        CS_LEAVE(&(pc)->echocs); \
    }

static void ttyc_echotask(ttycook_t *pc);

void ttyc_init(ttycook_t *pc)
{
    pc->head = 0;
    pc->tail = 0;
    pc->crs = 0;
    if (pc->echotask == NULL) {
        CS_INIT(&pc->echocs);
        pc->ehead = 0;
        pc->etail = 0;
        pc->echotask = task_reserve(256, TASK_PRI_NORM);
        task_sch_async(pc->echotask, &ttyc_echotask, 1, pc);
    }
}

void ttyc_putc(ttycook_t *pc, uint8_t ch)
{
    int left;

    if (ch == '\b' || ch == 127 /* DEL */) {
        if (pc->head == pc->tail) {
            return;
        }
        pc->tail = Q_PREV(pc->tail, TTYC_LINE_SIZE);
        TTYC_ECHO(pc, '\b');
        TTYC_ECHO(pc, ' ');
        TTYC_ECHO(pc, '\b');
        task_unblock(pc->echotask);
        return;
    }

    if (pc->head <= pc->tail) {
        left = TTYC_LINE_SIZE - pc->tail + pc->head - 1;
    } else {
        left = pc->head - pc->tail;
    }

    if (left <= 1 || (left <= 2 && ch != '\r' && ch != '\n')) {
        return;
    }

    if (ch == '\r') {
        ch = '\n';
    }

    pc->buf[pc->tail] = ch;
    pc->tail = Q_NEXT(pc->tail, TTYC_LINE_SIZE);

    if (ch == '\n') {
        TTYC_ECHO(pc, '\r')
        TTYC_ECHO(pc, '\n')
        pc->crs++;
    } else if (ch < 0x20) {
        TTYC_ECHO(pc, '^');
        ch |= 0x40;
        TTYC_ECHO(pc, ch);
    } else {
        TTYC_ECHO(pc, ch);
    }
    /* TODO what we REALLY mean here is signal an event. 
     */
    task_unblock(pc->echotask);
}

int ttyc_read(ttycook_t *pc, uint8_t __far *buf, int size)
{
    uint8_t ch;
    int got = 0;

    if (!pc->crs || pc->head == pc->tail) {
        return 0;
    }

    while (pc->crs && got < size) {
        ch = pc->buf[pc->head];
        pc->head = Q_NEXT(pc->head, TTYC_LINE_SIZE);
        *buf++ = ch;
        got++;

        if (ch == '\n') {
            pc->crs--;
            break;
        }

        if (pc->head == pc->tail) {
            break;
        }
    }

    return got;
}

tty_iob_t *ttyc_cook_output(uint8_t __far *buf, int size)
{
    int len;
    int i, o;
    tty_iob_t *iob;

    /* figure out the size 
     * TODO overflow
     */
    len = size;
    for (i = 0; i < size; i++) {
        if (buf[i] != '\n') {
            continue;
        }

        /* add a CR to the LF */
        len++;
    }

    iob = near_malloc(sizeof(tty_iob_t));
    if (iob == NULL) {
        return iob;
    }    

    iob->data = kmalloc(len);
    if (iob->data == NULL) {
        near_free(iob);
        return NULL;
    }

    for (i = 0, o = 0; i < size; i++) {
        if (buf[i] == '\n') {
            iob->data[o++] = '\r';
        }
        iob->data[o++] = buf[i];
    }

    iob->size = len;

    return iob;
}

/* unreachable code 
 */
#pragma warning( disable : 4702 ) 
void ttyc_echotask(ttycook_t *pc)
{
    int empty;
    int got;
    uint8_t writebuf[16];

    for(;;) {
        CS_ENTER(&pc->echocs);
        empty = pc->ehead == pc->etail;
        CS_LEAVE(&pc->echocs);

        if (empty) {
            task_block(&pc->blockq);
            continue;
        }

        got = 0;
        /* this must be fast as the critical section is also taken
         * at irq priority 
         */
        CS_ENTER(&pc->echocs);
        while (got < sizeof(writebuf) && pc->ehead != pc->etail) {
            writebuf[got++] = pc->echobuf[pc->ehead];
            pc->ehead = Q_NEXT(pc->ehead, TTYC_ECHO_QUEUE);
        }
        CS_LEAVE(&pc->echocs);

        /* this can block, we're running at a reasonable priority */
        pc->write(pc->unit, writebuf, got);
    }
}

