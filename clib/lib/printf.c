#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <io.h>

void putchar(char ch);

#define FL_ZEROPAD 0x0001
#define FL_SIGNED  0x0002
#define FL_LONG    0x0004
#define FL_PTR     0x0008

#define FL_SIGNED_LONG (FL_SIGNED | FL_LONG)

static int print_int(unsigned long val, int flags, int width);
static int print_hex(unsigned long val, int flags, int width);


/* TODO placeholder */
void putchar(char ch)
{
	write(0, &ch, 1);
}

int printf(const char *fmt, ...)
{
	const char *p = fmt;
	int wrote = 0;

	va_list va;
	char ch;
	long arg;
	char __far *str;
	int flags;
	int width;
	size_t len;

	va_start(va, fmt);
	
	for (p = fmt; *p; p++) {
		if (*p != '%') {
			putchar(*p);
			wrote++;
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
			 	wrote += print_int(arg, flags, width);
			} else {
				if (ch == 'p') {
					flags |= FL_PTR;
				}	
				wrote += print_hex(arg, flags, width);
			}
			break;

		case 's':
			if (flags & FL_LONG) {
				str = va_arg(va, char __far *);
			} else {
				str = va_arg(va, char __near *);
			}
			len = _fstrlen(str);
			write(0, str, len);
			break;

		case 'c':
			ch = va_arg(va, char);
			putchar((unsigned char)(ch & 0xff));
			wrote++;
			break;

		default:
			putchar(ch);
			wrote++;
			break;
		}
	}

	return wrote;
}

int print_int(unsigned long val, int flags, int width)
{
	int wrote = 0;

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
			putchar('0');
			wrote++;
		}
	}

	if (minus) {
		putchar('-');
		wrote++;
		width--;
	}

	for (; width > 0; width--) {
		putchar(' ');
		wrote++;
	}

	while (*p) {
		putchar(*p++);
		wrote++;
	}

	return wrote;
}

int print_hex(unsigned long val, int flags, int width)
{
	int wrote = 0;
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
			putchar(ch);
			wrote++;
		}
	}

	while (*p) {
		putchar(*p++);
		wrote++;
	}

	return wrote;
}
