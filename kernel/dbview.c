#include "dbview.h"

#include "blkcache.h"
#include "console/console.h"
#include "dis386.h"
#include "intr.h"
#include "ktypes.h"
#include "panic.h"
#include "ports.h"
#include "printk.h"
#include "process.h"
#include "task.h"
#include "sched/taskp.h"
#include "x86.h"

#include <string.h>

#define MAX_ARGS 16

#define STEP_VEC    1
#define BREAK_VEC   3
#define KBD_VEC     9
#define KBD_IRQ     1

#define INT3_OPCODE 0xcc

typedef enum db_state_t db_state_t;
enum db_state_t {
    db_running,
    db_stopped,
    db_stepping,
};

typedef void (__far *inthandler_t)(void);

#define INTVEC(n) ((inthandler_t __far *)MKFP(0, n * 4))

typedef struct breakpoint_t breakpoint_t;
struct breakpoint_t {
    uint8_t __far *addr;
    uint32_t linear;        /* duplicated but we use it a lot */
    int valid : 1;
    int enabled : 1;
    uint8_t orig_op;        /* what we replaced w/ an int 3 */
};

#define MAX_BKPTS  32

extern blkptr_t blocklist;
extern task_t *tsk_freel;
extern task_t tasks[MAX_TASK];

extern void __far db_intr(void);
extern void __far db_kbdintr(void);
extern uint8_t db_get_scancode(void);
extern void con_descan(uint8_t sc);

db_state_t db_state = db_running;

static inthandler_t old_kbdintr;
static uint16_t old_flags;
static unsigned saved_imask;
static db_state_t next_state;

static char command[78];
static int cargc;
static char *cargv[MAX_ARGS];
static breakpoint_t bkpts[MAX_BKPTS];

/* debugger core */
static void db_command_loop(void);
static uint8_t db_getchar(void);
static void db_read_command(void);
static void db_parse_command(void);
static void db_set_error(char *err);
static void db_enable_breakpoints(void);
static void db_disable_breakpoints(void);
static void motoff(void);

/* debugger commands */
static void in_port(void);
static void go(void);
static void out_port(void);
static void set_breakpoint(void);
static void show_bcstats(void);
static void show_breakpoints(void);
static void show_disasm(void);
static void show_mem(void);
static void show_proc(void);
static void show_regs(void);
static void show_task(void);
static void show_tasks(void);
static void step(void);

/* command helpers */
static char task_state(task_t *pt);
static int to_hex(char *p);
static void __far *parse_address(uint16_t defseg, char *str);

static char regnames[] = "ESDSDISIBPSPBXDXCXAXIPCSFL";

typedef struct dbcmd_t dbcmd_t;
struct dbcmd_t {
    char *cmd;
    void (*handler)(void);
};

static dbcmd_t cmds[] =
{
    { "b",      &set_breakpoint },
    { "bcstats",&show_bcstats },
    { "bl",     &show_breakpoints },
    { "in",     &in_port },
    { "go",     &go },
    { "mem",    &show_mem },
    { "out",    &out_port },
    { "proc",   &show_proc },
    { "regs",   &show_regs },
    { "s",      &step },
    { "task",   &show_task },
    { "tasks",  &show_tasks },
    { "disasm", &show_disasm },
    { NULL,     NULL}
};

/* initialize the debugger by hooking the breakpoint and single step
 * interrupts 
 */
void db_init(void)
{
    inthandler_t __far *p_sstp  = INTVEC(STEP_VEC);
    inthandler_t __far *p_break = INTVEC(BREAK_VEC);
    *p_sstp = &db_intr;
    *p_break = &db_intr;

    db_state = db_running;
}

/* common entry point into the debugger from a breakpoint or single step
 * when this is called, interrupts are still disabled
 */
void db_intr_c(void)
{
    taskreg_t __far *regs = curtask->stack;
    inthandler_t __far *p_kbd = INTVEC(KBD_VEC);

    /* set up things that get done once, when we enter from a 
     * breakpoint. Note that the debugger hotkey comes in through
     * this path via a hardcoded int 3.
     */
    if (db_state == db_running) {
        /* hook keyboard away from console driver */
        old_kbdintr = *p_kbd;
        *p_kbd = *db_kbdintr;

        /* disable all interrupts except keyboard at the PIC level */
        saved_imask = intr_get_mask();
        intr_set_mask(0xffff & ~(1 << KBD_IRQ));

        /* save task's flags (especially the interrupt state) */
        old_flags = regs->fl;

        /* disable breakpoints so disassembly works */
        db_disable_breakpoints();
    }

    /* things that get done on both initial entry and a single step
     */
    motoff();
    con_setpage(1);
    show_regs();
    db_state = db_stopped;

    db_command_loop();

    intr_cli();
    db_state = next_state;

    con_setpage(0);

    switch (db_state) {
    case db_running:
        regs->fl = old_flags;
        *p_kbd = old_kbdintr;
        intr_set_mask(saved_imask);
        db_enable_breakpoints();
        break;
    
    case db_stepping:
        regs->fl &= ~X86_INTR_ENABLE;
        regs->fl |=  X86_TRAP_ENABLE;
        break;

    default:
        panic("invalid state leaving debugger");
    }
}

/* wait for a keystroke and return it. 
 */
uint8_t db_getchar(void)
{
    uint8_t sc;
    uint8_t ch;

    for(;;) {
        /* this will block until a scancode is available */
        sc = db_get_scancode();
        
        /* use the console driver to map scancodes to ASCII+fn keys */
        con_descan(sc);
        ch = con_getch();

        /* not all scancodes immediately map to a character, 
         * we have to check 
         */
        if (ch) {
            break;
        }
    }

    return ch;
}

/* read a full command into the `command` global variable, with simple
 * editing
 */
void db_read_command(void)
{
    int cmdidx = 0;
    uint8_t ch;

    con_gotoxy(0, 24);
    kprintf(":");
    con_clreol();

    for(;;) {
        ch = db_getchar();

        if (ch >= 0x20 && ch < 0x7f) {
            if (cmdidx < sizeof(command) - 1) {
                con_char(ch);
                command[cmdidx++] = ch;
                continue;
            }    
        }

        if (ch == '\b' && cmdidx) {
            con_char(ch);
            con_char(' ');
            con_char(ch);
            cmdidx--;
            continue;
        }

        if (ch == '\n') {
            command[cmdidx] = '\0';
            break;
        }        
    }
}

/* parse the global `command` into `cargc` and `cargv`
 */
void db_parse_command(void)
{
    char *pc = command;

    cargc = 0;

    while (*pc && cargc < MAX_ARGS) {
        while (*pc == ' ') {
            pc++;
        }

        if (!*pc) {
            break;
        }

        cargv[cargc++] = pc;

        while (*pc && *pc != ' ') {
            pc++;
        }

        if (*pc == ' ') {
            *pc++ = '\0';
        }
    }
}

/* read in a command, parse it, and execute it
 */
void db_command_loop(void)
{
    dbcmd_t *pc;
    next_state = db_stopped;
    
    while (next_state == db_stopped) {
        db_read_command();
        db_parse_command();

        if (cargc == 0) {
            continue;
        }

        for (pc = cmds; pc->cmd; pc++) {
            if (strcmp(pc->cmd, cargv[0]) == 0) {
                break;
            }
        }

        if (pc->handler) {
            db_set_error("");
            pc->handler();
        } else {
            db_set_error("bad command");
        }
    }
}

/* display an error in the line above the command prompt
 */
void db_set_error(char *err)
{
    con_gotoxy(0, 23);
    con_clreol();
    kprintf("%s", err);
}

/* enable breakpoints before running 
 */
void db_enable_breakpoints(void)
{
    int i;
    
    for (i = 0; i < MAX_BKPTS; i++) {
        breakpoint_t *bkpt = &bkpts[i];
        if (!(bkpt->valid && bkpt->enabled)) {
            continue;
        }

        *bkpt->addr = INT3_OPCODE;
    }
}

/* replace all enabled breakpoints with their original ops
 * so disassembly works properly. if we've just stopped
 * at a breakpoint, put CS:IP back at the breakpoint address.
 * We have to check this to avoid messing with int 3's that
 * are hardcoded, like the one for the hotkey.
 */
void db_disable_breakpoints(void)
{
    taskreg_t __far *regs = curtask->stack;

    int i;
    uint8_t __far *csip;
    uint32_t linhit;

    csip = MKFP(regs->cs, regs->ip);
    linhit = KLINEAR(csip) - 1;

    for (i = 0; i < MAX_BKPTS; i++) {
        breakpoint_t *bkpt = &bkpts[i];
        if (!(bkpt->valid && bkpt->enabled)) {
            continue;
        }

        if (bkpt->linear == linhit) {
            /* back up so CS:IP points at the original opcode.
             * we know there's only one breakpoint per linear address,
             * as it's enforced when breakpoints are set.
             */
            regs->ip--;
        }

        *bkpt->addr = bkpt->orig_op;
    }
}


/* Turn off the floppy drive motors
 */
void motoff(void)
{
    /* this really ought to be a helper on the fdd driver */
    outb(0x3f2, 0x0c);
}


/*****************************************************************************
 * Command handlers
 */

/* read and print a value from an I/O port
 */
void in_port(void)
{
    int port;
    int value;

    if (cargc != 2) {
        db_set_error("in: need port");
        return;
    }

    if ((port = to_hex(cargv[1])) == -1) {
        db_set_error("in: invalid port");
        return;
    }

    value = inb(port);
    con_gotoxy(0, 23);
    con_clreol();
    kprintf("in(%04x) -> %02x", port, value & 0xff);
}

/* continue execution
 */
void go(void)
{
    next_state = db_running;
}

/* write a value to an I/O port
 */
void out_port(void)
{
    int port;
    int value;

    if (cargc != 3) {
        db_set_error("out: need port and value");
        return;
    }

    if ((port = to_hex(cargv[1])) == -1) {
        db_set_error("out: invalid port");
        return;
    }

    if ((value = to_hex(cargv[2])) == -1) {
        db_set_error("out: invalid value");
        return;
    }

    outb(port, (uint8_t)value);
    con_gotoxy(0, 23);
    con_clreol();
    kprintf("out(%04x) <- %02x", port, value & 0xff); 
}

/* set an int 3 breakpoint
 */
void set_breakpoint(void)
{
    uint8_t __far *loc;
    uint32_t linear;
    int i;
    int slot = -1;
    breakpoint_t *bkpt;

    if (cargc != 2) {
        db_set_error("b: need breakpoint address");
        return;
    }

    loc = parse_address(get_cs(), cargv[1]);
    if (!loc) {
        return;
    }

    linear = KLINEAR(loc);

    for (i = 0; i < MAX_BKPTS; i++) {
        bkpt = &bkpts[i];

        if (!bkpt->valid && slot == -1) {
            slot = i;
        }

        if (bkpt->valid && bkpt->linear == linear) {
            db_set_error("b: there is already a breakpoint at this address");
            return;
        }
    }
    
    if (slot == -1) {
        db_set_error("b: there are no free breakpoint slots");
        return;
    }

    bkpt = &bkpts[slot];    
    bkpt->addr = loc;
    bkpt->linear = linear;
    bkpt->valid = 1;
    bkpt->enabled = 1;
    bkpt->orig_op = *loc;
}

/* show information about the block cache
 */
void show_bcstats(void)
{
    blkptr_t blk;
    int x;
    int y;
    bcstats_t stats;

    con_clrscr();
    x = 2;
    y = 2;

    blk = blocklist;
    do {
        con_gotoxy(x, y);
        kprintf("# %5lu REF %5d DATA %lp", 
            blk->blkno,
            blk->inuse,
            blk->data
        );

        if (++y == 18) {
            x += 40;
            y = 2;
        }
    blk = blk->next;
    } while (blk != blocklist);

    bc_get_stats(&stats);

    con_gotoxy(0, 0);
    kprintf("GETS %lu PUTS %lu READS %lu WRITES %lu",
        stats.gets,
        stats.puts,
        stats.reads,
        stats.writes
    );
}

/* show all valid breakpoints
 */
void show_breakpoints(void)
{
    int i;
    int scr;

    con_clrscr();

    for (i = 0; i < 2; i++) {
        con_gotoxy(1 + 41 * i, 0);
        kprintf("##   CS:IP   E");
    }

    for (i = 0, scr = 0; i < MAX_BKPTS; i++) {
        breakpoint_t *bkpt = &bkpts[i];
        if (bkpt->valid) {
            con_gotoxy(scr < 16 ? 1 : 42, 1 + (scr & 0x0f));
            kprintf("%2d %lp %u", i, bkpt->addr, bkpt->enabled);
        }
    }
}

/* disassemble code
 */
void show_disasm(void)
{
    disinstr_t dis;
    int i;
    uint8_t __far *pc = (uint8_t __far *)(void (__far *)(void))&show_disasm;

    con_clrscr();

    if (cargc > 1) {
        pc = parse_address(get_cs(), cargv[1]);
    }

    for (i = 0; i < 23; i++) {
        disasm(pc, KOFFS_OF(pc), &dis, 0);
        con_gotoxy(2, i);
        kprintf("%lp %s", pc, dis.mne);
        con_gotoxy(25, i);
        kprintf("%s", dis.args);
        pc += dis.bytes;
    }
}

/* show memory in a hex dump
 */
void show_mem(void)
{
    int i, j;
    uint8_t __far *pm;

    if (cargc != 2) {
        db_set_error("mem: need address");
        return;
    }

    con_clrscr();

    pm = parse_address(get_ds(), cargv[1]);
    
    for (i = 0; i < 23; i++) {
        con_gotoxy(0, i);

        kprintf("%lp  ", pm);

        for (j = 0; j < 16; j++) {
            kprintf("%02x%c",
                pm[j],
                (j == 7) ? '-' : ' '
            );
        }

        kputc('|');
        for (j = 0; j < 16; j++) {
            uint8_t ch = pm[j];
            if (ch < 0x20 || ch >= 0x7f) {
                ch = '.';
            }
            kputc(ch); 
        }
        kputc('|');

        pm += 16;
    }
}

/* show detailed information about one process
 */
void show_proc(void)
{
    int tp;
    process_t *proc;
    int y = 0;
    int i;

    if (cargc != 2) {
        db_set_error("proc: need address");
        return;
    }

    if ((tp = to_hex(cargv[1])) == -1) {
        db_set_error("proc: invalid address");
        return;
    }

    con_clrscr();   
    proc = (process_t*)tp;

    con_gotoxy(2, y++);
    kprintf("PROC %p", proc);

    con_gotoxy(2, y++);
    kprintf("PID %d", proc->pid);

    con_gotoxy(2, y++);
    kprintf("TASK %p", proc->task);

    con_gotoxy(2, y++);
    kprintf("IMAGE %lp", proc->image);

    for (i = 0; i < PROC_FILES; i++) {
        con_gotoxy(2, y++);
        kprintf("FD %3d KFD %3d", i, proc->fds[i].kfd);
    }
}

/* show the registers at the time of breakpoint and a few lines of 
 * disassembly at CS:IP
 */
void show_regs(void)
{
    disinstr_t dis;

    static const int nregs = 13;

    uint16_t __far *uregs = curtask->stack;
    taskreg_t __far *regs = curtask->stack;
    uint8_t __far *pc = MKFP(regs->cs, regs->ip); 
    int i;

    if (uregs == NULL) {
        con_gotoxy(0, 23);
        con_clreol();
        kprintf("current task has no stack");
        return;
    }

    con_clrscr();

    for (i = 0; i < nregs; i++) {
        con_gotoxy(2, i+1);
        kprintf("%c%c  %04x",
            regnames[2*i],
            regnames[2*i+1],
            uregs[i]        
        );
    }

    for (i++; i < 23; i++) {
        disasm(pc, KOFFS_OF(pc), &dis, 0);
        con_gotoxy(2, i);
        kprintf("%lp %s", pc, dis.mne);
        con_gotoxy(25, i);
        kprintf("%s", dis.args);
        pc += dis.bytes;
    }
}

/* display detailed information about one task 
 */
void show_task(void)
{
    int tp;
    task_t *pt;
    taskreg_t __far *pr;
    char st;
    uint16_t __far *regs;
    int i, j;
    const char *rn;
    uint16_t ss, bp;
    uint16_t __far *pbp;

    if (cargc != 2) {
        db_set_error("task: need address");
        return;
    }

    if ((tp = to_hex(cargv[1])) == -1) {
        db_set_error("task: invalid address");
        return;
    }

    con_clrscr();   

    pt = (task_t*)tp;

    con_gotoxy(1, 0);
    kprintf("PREV NEXT ST PR   STACK     CS:IP   OWNR PROC DUE");

    st = task_state(pt);

    con_gotoxy(1, 1);
    kprintf("%p %p %c%c ", 
        pt->prev, 
        pt->next, 
        st,
        (pt == get_curtask()) ? '<' : ' '
    );

    kprintf("%2d ", pt->pri);
    kprintf("%lp ", pt->stack);

    if (pt->stack) {
        pr = (taskreg_t __far *)pt->stack;
        kprintf("%p:%p", pr->cs, pr->ip);
    } else {
        kprintf("         ");
    }

    kprintf(" %p %p %ld", pt->owner, pt->process, pt->due);

    regs = (uint16_t __far *)pt->stack;

    rn = regnames;
    con_gotoxy(2, 3);

    kprintf("  REGS");
    for (i = 0; *rn; rn += 2, i++) {
        con_gotoxy(2, i+4);
        kprintf("%c%c ", rn[0], rn[1]);
        if (regs) {
            kprintf("%04x", regs[i]);
        } else {
            kprintf("----");
        }
    }

    /* if it's a kernel task */
    ss = KSEG_OF(pt->stack);
    if (ss == get_ds()) {
        con_gotoxy(12, 3);
        kprintf(" BP   RA  ARGS");

        bp = ((taskreg_t __far *)(pt->stack))->bp;

        for (i = 0; i < 8; i++) {
            if (bp == 0) {
                break;
            }

            pbp = (uint16_t __far *)MKFP(ss, bp);
            con_gotoxy(12, 4+i);
            kprintf("%p %p ", bp, pbp[1]);
            for (j = 0; j < 6; j++) {
                kprintf("%04x ", pbp[2+j]);
            }

            bp = pbp[0];
        }
    }
}

/* show all of the tasks and the task free list
 */
void show_tasks(void)
{
    int i;
    char st;
    task_t *pt;
    taskreg_t __far *pr;
    
    con_clrscr();

    for (i = 0; i < 2; i++) {
        con_gotoxy(1 + 41 * i, 0);
        kprintf("TASK ST PR   STACK     CS:IP  ");
    }

    for (i = 0; i < MAX_TASK; i++) {
        con_gotoxy(i < 16 ? 1 : 42, 1 + (i & 0x0f));
        pt = &tasks[i];
        st = task_state(pt);

        kprintf("%p %c%c ", 
            pt,
            st,
            (pt == get_curtask()) ? '<' : ' '
        );

        kprintf("%2d ", pt->pri);
        kprintf("%lp ", pt->stack);

        if (pt->stack) {
            pr = (taskreg_t __far *)pt->stack;
            kprintf("%p:%p", pr->cs, pr->ip);
        }
    } 

    con_gotoxy(0, 19);
    for (pt = tsk_freel; pt; pt=pt->next) {
        kprintf("%p ", pt);
    }
}

/* exit the debugger for a single instruction
 */
void step(void)
{
    next_state = db_stepping;
}


/*****************************************************************************
 * Command helpers
 */


/* return a character representation of a task's state 
 */
char task_state(task_t *pt)
{
    switch (pt->state) {
    case TS_FREE:
        return 'F';

    case TS_FREEING:
        return 'f';
    
    case TS_BLOCKED:
        return 'B';

    case TS_READY:
        return 'R';
    
    case TS_RUN:
        return '*';
    }

    return '?';
}

/* convert the hex value in p to an integer
 */
int to_hex(char *p)
{
    int val = 0;

    while (*p) {
        val <<= 4;
        if (*p >= '0' && *p <= '9') {
            val += *p - '0';
        } else if (*p >= 'a' && *p <= 'f') {
            val += *p - 'a' + 10;
        } else if (*p >= 'A' && *p <= 'F') {
            val += *p - 'A' + 10;
        } else {
            return -1;
        }
        p++;
    }

    return val;
}

/* parse a segmented address to a far pointer. if the segment is
 * not given, default to `defseg`.
 */
void __far *parse_address(uint16_t defseg, char *str)
{
    char *sstr, *ostr;
    int seg, off;
    int i;

    sstr = NULL;
    ostr = str;
    for (i = 0; str[i] && str[i] != ':'; i++) {
    }

    if (str[i] == ':') {
        str[i++] = '\0';
        sstr = ostr;
        ostr = &str[i];
    }

    if (sstr) {
        seg = to_hex(sstr);
    } else {
        seg = defseg;
    }
    off = to_hex(ostr);

    if (seg == -1 || off == -1) {
        db_set_error("invalid segment or offset");
        return NULL;
    }

    return MKFP(seg, off);
}
