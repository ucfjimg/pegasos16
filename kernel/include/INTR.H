#ifndef INTR_H_
#define INTR_H_

/* An interrupt handler.
 */
typedef int (*intrfn_t)(unsigned intr);

extern int intr_enabled(void);
extern int intr_cli(void);
extern int intr_sti(void);
extern void intr_eoi(int irq);
extern void intr_restore(int st);
extern void intr_init(void);
extern int intr_add_handler(unsigned intr, intrfn_t fn);

extern unsigned intr_get_mask(void);
extern void intr_set_mask(unsigned mask);
extern void intr_mask_enable(unsigned intr);
extern void intr_mask_disable(unsigned intr);

#define INTR_N 16

#endif
