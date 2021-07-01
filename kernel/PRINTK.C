#include <stdarg.h>

#include "printk.h"
#include "ktypes.h"

extern void kbreak(void);

#define FL_ZEROPAD 0x0001
#define FL_SIGNED  0x0002
#define FL_LONG    0x0004
#define FL_PTR     0x0008

#define FL_SIGNED_LONG (FL_SIGNED | FL_LONG)

static void print_int(putc_t pc, void *arg, unsigned long val, int flags, int width);
static void print_hex(putc_t pc, void *arg, unsigned long val, int flags, int width);

void _kputc(void *arg, int ch)
{
	UNREFERENCED(arg);
	kputc(ch);
}

void kprintf(char __far *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	vprintf(&_kputc, NULL, fmt, va);
	va_end(va);
}

typedef struct sprintf_t sprintf_t;
struct sprintf_t {
	char *next;
};

static void sprintc(sprintf_t *ctx, int ch)
{
	*ctx->next++ = (char)ch;
}

int sprintf(char *dst, char __far *fmt, ...)
{
	va_list va;
	sprintf_t ctx;
	ctx.next = dst;

	va_start(va, fmt);
	vprintf(&sprintc, &ctx, fmt, va);
	va_end(va);

	return ctx.next - dst;
}

void vprintf(putc_t pc, void *pcarg, char __far *fmt, va_list va)
{
	char __far *p = fmt;
	char ch;
	long arg;
	char __far *str;
	int flags;
	int width;

	for (p = fmt; *p; p++) {
		if (*p != '%') {
			pc(pcarg, *p);
			continue;
		}

		ch = *++p;
		if (ch == '\0') {
			break;
		}
		
		flags = 0;
		width = -1;
		
		if (ch == '0') {
			flags |= FL_ZEROPAD;
			ch = *++p;
			if (ch == '\0') {
				break;
			}
		}

		/* TODO isdigit
		 */
		while (ch >= '0' && ch <= '9') {
			if (width == -1) {
				width = 0;
			} else {
				width *= 10;
			}
			width += ch - '0';

			ch = *++p;
			if (ch == '\0') {
				break;
			}
		}

		if (ch == 'l') {
			flags |= FL_LONG;
			ch = *++p;
			if (ch == '\0') {
				break;
			}
		}

		if (ch == 'd') {
			flags |= FL_SIGNED;
		}

		switch (ch) {
		case 'd':
		case 'u':
		case 'x':
		case 'p':
			/* NB this is needed for proper sign extension */
			switch (flags & FL_SIGNED_LONG) {
			case FL_SIGNED_LONG:
				arg = va_arg(va, long);
				break;

			case FL_LONG:
				arg = va_arg(va, unsigned long);
				break;

			case FL_SIGNED:
				arg = va_arg(va, int);
				break;

			default:
				arg = va_arg(va, unsigned int);
				break;
			}

			if (ch == 'u' || ch == 'd') {
			 	print_int(pc, pcarg, arg, flags, width);
			} else {
				if (ch == 'p') {
					flags |= FL_PTR;
				}	
				print_hex(pc, pcarg, arg, flags, width);
			}
			break;


		case 's':
			if (flags & FL_LONG) {
				str = va_arg(va, char __far *);
			} else {
				str = va_arg(va, char __near *);
			}
			while (*str) {
				pc(pcarg, *str++);
			}
			break;

		case 'c':
			ch = va_arg(va, char);
			pc(pcarg, (uint8_t)(ch & 0xff));
			break;

		default:
			pc(pcarg, ch);
			break;
		}
	}
}

void print_int(putc_t pc, void *pcarg, unsigned long val, int flags, int width)
{
	char work[12];
	char *p = work + sizeof(work);
	int minus = 0;

	*--p = '\0';

	if (val == 0) {
		*--p = '0';
	} else {
		if (flags & FL_SIGNED) {
			long s = (long)val;
			if (s < 0) {
				minus = 1;
				val = -s;
			}
		}	

		while (val) {
			*--p = (char)('0' + (val % 10));
			val /= 10;
		}
	}

	width -= (work + sizeof(work) - p - 1);
	
	if (flags & FL_ZEROPAD) {
		for (; width > 0; width--) {
			pc(pcarg, '0');
		}
	}

	if (minus) {
		pc(pcarg, '-');
		width--;
	}

	for (; width > 0; width--) {
		pc(pcarg, ' ');
	}

	while (*p) {
		pc(pcarg, *p++);
	}

}

void print_hex(putc_t pc, void *pcarg, unsigned long val, int flags, int width)
{
	char work[12];
	char *p = work + sizeof(work);
	int minus = 0;
	int i = 0;
	int len = (flags & FL_LONG) ? 8 : 4; 

	*--p = '\0';

	if (val == 0) {
		*--p = '0';
		if (flags & FL_PTR) {
			for (i = 1; i < 4; i++) {
				*--p = '0';
			}
			if (flags & FL_LONG) {
				*--p = ':';
				for (i = 0; i < 4; i++) {
					*--p = '0';
				}
			}
		}
	} else {
		while (val || ((flags & FL_PTR) && i < len)) {
			int hexit = (int)(val & 0x0f);
			if (hexit <= 9) {
				*--p = (char)('0' + hexit);
			} else {
				*--p = (char)('A' + hexit - 10);
			}
			val >>= 4;
			i++;
			if ((flags & FL_PTR) && (flags & FL_LONG) && i == 4) {
				*--p = ':';
			}
		}
	}

	if ((flags & FL_PTR) == 0) {
		char ch = (char)((flags & FL_ZEROPAD) ? '0' : ' ');
		width -= (work + sizeof(work) - p - 1);

		for (; width > 0; --width) {
			pc(pcarg, ch);
		}
	}

	while (*p) {
		pc(pcarg, *p++);
	}
}
