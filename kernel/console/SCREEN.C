#include "console.h"

#include "device.h"
#include "intr.h"
#include "ports.h"
#include "printk.h"
#include "task.h"

#include <errno.h>
#include <stdarg.h>
#include <string.h>

enum attrcolor_t {
	ac_black,
	ac_blue,
	ac_green,
	ac_cyan,
	ac_red,
	ac_purple,
	ac_brown,
	ac_grey
};

const uint8_t ac_blink = 0x80;
const uint8_t ac_hilight = 0x08;

#define ATTR(bg, fg, flags) \
	(uint8_t)((((bg) & 0x0f) << 4) | ((fg) & 0x0f) | ((flags) & 0x88))

static const uint8_t defattr = 0x0a;

typedef struct concell_t concell_t;
struct concell_t 
{
	uint8_t data;
	uint8_t attr;
};

#define PAGES 4

typedef struct con_page_t con_page_t;
struct con_page_t {
    int curs_x;
    int curs_y;
};

static int page = 0;
static con_page_t pages[PAGES];
static const int scr_x = 80;
static const int scr_y = 25;
static const int scr_cells = 80 * 25;
static concell_t __far *scrram = MKFP(0xb800, 0x0000);
static int con_stcol;

static void con_stputc(void *arg, int ch);
static void con_scroll(void);
static void con_left(void);
static void con_right(void);
static void con_cursync(void);
static void con_enter_cs(void);
static void con_leave_cs(void);

#define MAXY() (page == 0 ? scr_y - 1 : scr_y)

void con_clrscr(void)
{
	int i;
	register concell_t __far *p = scrram;
	concell_t space;

	space.data = ' ';
	space.attr = defattr;

	con_enter_cs();

    if (page == 0) {
        for (i = 0; i < scr_cells - scr_x; i++) {
            *p++ = space; 
        }

        space.attr = 0x1F;
        for (i = 0; i < scr_x; i++) {
            *p++ = space;
        }
    } else {
        for (i = 0; i < scr_cells; i++) {
            *p++ = space; 
        }    
    }

    pages[page].curs_x = 0;
    pages[page].curs_y = 0;
	con_cursync();

	con_leave_cs();
}

void con_clreol(void)
{
	int i;
	register concell_t __far *p = scrram;
	concell_t space;

	space.data = ' ';
	space.attr = defattr;

	con_enter_cs();

    p += pages[page].curs_y * scr_x + pages[page].curs_x;

    for (i = pages[page].curs_x; i < scr_x; i++) {
        *p++ = space;
    }

	con_leave_cs();
}

/* this is only for the kernel to display debug/status info on the 
 * console's lowest row
 */
void con_status(int col, char __far *fmt, ...)
{
	va_list va;
	va_start(va, fmt);

    con_enter_cs();

	con_stcol = col;
	vprintf(con_stputc, NULL, fmt, va);

    con_leave_cs();

	va_end(va);

}

void con_stputc(void *arg, int ch)
{
    /* NB the status bar only shows up on page 0, but we draw to it 
     * even when other pages are up so it'll always be up to date
     */
    concell_t __far *scr = MKFP(0xb800, 0);

    UNREFERENCED(arg);

	if (ch < ' ' || ch > 126 || con_stcol >= scr_x) {
		return;
	}

    con_enter_cs();
	scr[scr_cells - scr_x + con_stcol++].data = (uint8_t)ch;
    con_leave_cs();
}

void con_scroll(void)
{
	int i;
	register concell_t __far *p = scrram;
	concell_t space;
    int lastrow = MAXY() - 1;
    
    con_enter_cs();
	
    _fmemmove(
		scrram,
		scrram + scr_x,
		lastrow * scr_x * sizeof(concell_t)
	);

	pages[page].curs_y--;

	space.data = ' ';
	space.attr = defattr;

	p += lastrow * scr_x;
	for (i = 0; i < scr_x; i++) {
		*p++ = space;
	}
	con_cursync();

    con_leave_cs();
}

void con_right(void)
{
    con_page_t *pg = &pages[page];

    con_enter_cs();
	
    if (++pg->curs_x == scr_x) {
		pg->curs_x = 0;
		if (++pg->curs_y == MAXY()) {
			con_scroll();
		}
	}		
	con_cursync();

    con_leave_cs();
}

void con_left(void)
{
    con_page_t *pg = &pages[page];

    con_enter_cs();

	if (pg->curs_x == 0) {
		if (pg->curs_y == 0) {
			return;
		}
		pg->curs_x = scr_x - 1;
		pg->curs_y--;
		return;
	}

	pg->curs_x--;
	con_cursync();

    con_leave_cs();
}

int con_maxx(void)
{
	return scr_x - 1;
}

int con_maxy(void)
{
	return MAXY() - 1;
}

void con_gotoxy(int x, int y)
{
    con_page_t *pg = &pages[page];

    con_enter_cs();

	if (x < 0) {
		pg->curs_x = 0;
	} else if (x >= scr_x) {
		pg->curs_x = scr_x - 1;
	} else {
		pg->curs_x = x;
	}

	if (y < 0) {
		pg->curs_y = 0;
	} else if (y >= MAXY()) {
		pg->curs_y = MAXY() - 1;
	} else {
		pg->curs_y = y;
	}

	con_cursync();

    con_leave_cs();
}

void kputc(int ch)
{
	if (ch == '\n') {
		con_char('\r');
	}
	con_char((uint8_t)ch);
}

void con_char(uint8_t by)
{		
    con_page_t *pg = &pages[page];
	concell_t cell;

    con_enter_cs();

	switch (by) {
		case '\r':
			pg->curs_x = 0;
			break;

		case '\n':
			if (++pg->curs_y == MAXY()) {
				con_scroll();
			}
			break;

		case '\b':
			con_left();
			break;

		default:
			cell.attr = defattr;
			cell.data = by;
			scrram[pg->curs_y * scr_x + pg->curs_x] = cell;				
			con_right();
			break;
	}

    con_leave_cs();
}

/* TODO these are hard-coded CGA/EGA/VGA but (a) we're already
 * using B800:0000 and (b) we can't run on any system that would
 * have an MDA anyway.
 */
#define CRT_INDEX_PORT	0x03d4
#define CRT_DATA_PORT	0x03d5

#define CRT_IDX_START_H     12
#define CRT_IDX_START_L	    13
#define CRT_IDX_CURSOR_H	14
#define CRT_IDX_CURSOR_L	15

void con_cursync(void)
{
    con_page_t *pg = &pages[page];
    uint16_t base = page * scr_cells;
	uint16_t cursor = base + pg->curs_y * scr_x + pg->curs_x;

	int sti;
	
	sti = intr_cli();
	outb(CRT_INDEX_PORT, CRT_IDX_CURSOR_H);
	outb(CRT_DATA_PORT, (uint8_t)(cursor >> 8));
	outb(CRT_INDEX_PORT, CRT_IDX_CURSOR_L);
	outb(CRT_DATA_PORT, (uint8_t)(cursor & 0xff));

	intr_restore(sti);
}

void con_setpage(int newpage)
{
    int sti;
    uint16_t base;

    if (newpage < 0 || newpage >= PAGES) {
        return;
    }

    sti = intr_cli();

    page = newpage;
    base = page * scr_cells;
	outb(CRT_INDEX_PORT, CRT_IDX_START_H);
	outb(CRT_DATA_PORT, (uint8_t)(base >> 8));
	outb(CRT_INDEX_PORT, CRT_IDX_START_L);
	outb(CRT_DATA_PORT, (uint8_t)(base & 0xff));

    con_cursync();

    scrram = MKFP(0xb800, 2 * base);

    intr_restore(sti);
}

int con_write(int unit, uint8_t __far *buf, int size)
{
    if (unit != 0) {
        return -ENODEV;
    }

    con_enter_cs();

    while (size--) {
		uint8_t by = *buf++;
		con_char(by);
	}

    con_leave_cs();

	return 0;
}

/* this is hacky. we prefer to use a critical section rather than stop
 * everything else but con_statusmem() can get called before con_init() when
 * drivers use it for debug.
 */
static int cs_sti = 0;
static int cs_reenter = 0;

void con_enter_cs(void)
{
    int sti = intr_cli();
    if (cs_reenter++ == 0) {
        cs_sti = sti;
    }
}

void con_leave_cs(void)
{
    if (--cs_reenter == 0) {
        intr_restore(cs_sti);
    }
}
