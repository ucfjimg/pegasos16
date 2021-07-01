#include "panic.h"
#include "printk.h"

/* inline asm precludes global optimization */
#pragma warning (disable: 4704)

void panic(char __far* str)
{
	int i;
	unsigned *base;

	kprintf("\n\n\aPANIC: %ls\n\n", str);

	_asm mov [base], bp;

	for (i = 0; i < 16; i++) {
		kprintf("%p ", base[1]);
		if (base == 0) {
			break;
		}

		base = (unsigned *)*base;
	}

	__asm hlt;
}


