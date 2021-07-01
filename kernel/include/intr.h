#ifndef INTR_H_
#define INTR_H_

/* An interrupt handler.
 */
typedef void (*intrfn_t)(unsigned intr);

extern int intr_enabled(void);
extern void intr_cli(void);
extern void intr_sti();
extern void intr_init(void);
extern int intr_add_handler(unsigned intr, intrfn_t fn);

#define INTR_N 16

#endif
