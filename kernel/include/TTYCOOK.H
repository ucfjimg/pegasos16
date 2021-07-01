#ifndef TTYCOOK_H_
#define TTYCOOK_H_

#include "ktypes.h"
#include "task.h"
#include "tty.h"

#define TTYC_LINE_SIZE 128
#define TTYC_ECHO_QUEUE 32

typedef struct ttycook_t ttycook_t;
struct ttycook_t {
    char buf[TTYC_LINE_SIZE];
    char echobuf[TTYC_ECHO_QUEUE];
    int tail;
    int head;
    int ehead;
    int etail;
    critsect_t echocs;
    int crs;
    task_t *echotask;
    taskq_t blockq;
    int (*write)(int unit, uint8_t __far *buf, int size);
    int unit;
};

extern void ttyc_init(ttycook_t *pc);
extern int ttyc_read(ttycook_t *pc, uint8_t __far *buf, int size);
extern void ttyc_putc(ttycook_t *pc, uint8_t ch);
extern tty_iob_t *ttyc_cook_output(uint8_t __far *buf, int size);

#endif
