#ifndef CONSOLE_H_
#define CONSOLE_H_

#include "ktypes.h"

extern void con_status(int col, char __far *fmt, ...);
extern int con_maxx(void);
extern int con_maxy(void);
extern void con_gotoxy(int x, int y);
extern void con_clrscr(void);
extern void con_clreol(void);

uint8_t con_getch(void);
void con_char(uint8_t ch);

extern void con_setpage(int page);

/* scan code modifiers */
#define SC_EXT_PREFIX 0xE0
#define SC_KEYUP    0x80

#define SC_SHIFTS	0x01	/* shifts with shift key */
#define SC_CAPSLOCK	0x02	/* honors caps lock */
#define SC_CTRL		0x04	/* honors control */
#define SC_META     0x08    /* meta key */
#define SC_EXT      0x10    /* extended */
#define SC_NUML     0x20    /* `shifted` is an extended key if NUMLOCK is on */
#define SC_E0       0x40    /* key has base meaning if prefixed w/ E0 
                             * if key is a shift key, it's a separate one for
                             * purposes of saving state (i.e. left ctrl/right ctrl)
                             */

#define SC_META_LSHIFT      0x0001
#define SC_META_LCTRL       0x0002
#define SC_META_LALT        0x0004
#define SC_META_RSHIFT      0x0008
#define SC_META_RALT        0x0010
#define SC_META_RCTRL       0x0020
#define SC_META_NUMLOCK     0x0040
#define SC_META_SCRLOCK     0x0080u
#define SC_META_CAPSLOCK    0x0100

#define SC_EXT_F1           0x01
#define SC_EXT_F2           0x02
#define SC_EXT_F3           0x03
#define SC_EXT_F4           0x04
#define SC_EXT_F5           0x05
#define SC_EXT_F6           0x06
#define SC_EXT_F7           0x07
#define SC_EXT_F8           0x08
#define SC_EXT_F9           0x09
#define SC_EXT_F10          0x0A
#define SC_EXT_F11          0x0B
#define SC_EXT_F12          0x0C

#define SC_EXT_HOME         0x20
#define SC_EXT_END          0x21
#define SC_EXT_PGUP         0x22
#define SC_EXT_PGDN         0x23
#define SC_EXT_UP           0x24
#define SC_EXT_DOWN         0x25
#define SC_EXT_LEFT         0x26
#define SC_EXT_RIGHT        0x27
#define SC_EXT_INS          0x28
#define SC_EXT_DEL          0x29
#define SC_WINDOWS          0x2A
#define SC_CONTEXT          0x2B

typedef struct scandef_t scandef_t;
struct scandef_t {
	char ascii;
	char shifted;
	uint8_t flags;
};

#define SC_GET_META(sc) \
    ((((uint16_t)((sc)->shifted & 0xff)) << 8) | (uint16_t)((sc)->ascii & 0xff))

extern scandef_t scascii[];
extern int nscascii;

#endif
