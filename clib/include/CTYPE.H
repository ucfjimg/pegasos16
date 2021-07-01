#ifndef CTYPE_H_
#define CTYPE_H_

extern int toupper(int ch);
extern int isspace(int ch);

#define _CT_SPACE 0x01
#define _CT_LOWER 0x02
#define _CT_UPPER 0x04
#define _CT_DIGIT 0x08
#define _CT_PRINT 0x10
#define _CT_XDIGIT 0x20
#define _CT_CNTRL 0x40

#define _CT_ALPHA (_CT_LOWER | _CT_UPPER)
#define _CT_ALNUM (_CT_ALPHA | _CT_DIGIT)

extern unsigned char _ct_attr[256];

#define isalnum(c) (_ct_attr[(c)&0xff] & _CT_ALNUM)
#define isalpha(c) (_ct_attr[(c)&0xff] & _CT_ALPHA)
#define iscntrl(c) (_ct_attr[(c)&0xff] & _CT_CNTRL)
#define isdigit(c) (_ct_attr[(c)&0xff] & _CT_DIGIT)
#define isgraph(c) (_ct_attr[(c)&0xff] & _CT_PRINT)
#define islower(c) (_ct_attr[(c)&0xff] & _CT_LOWER)
#define isprint(c) (_ct_attr[(c)&0xff] & (_CT_SPACE | _CT_PRINT))
#define ispunct(c) \
    (_ct_attr[(c)&0xff] & (_CT_PRINT | _CT_SPACE | _CT_ALPHA | _CT_DIGIT) == _CT_PRINT)
#define isspace(c) (_ct_attr[(c)&0xff] & _CT_SPACE)
#define isupper(c) (_ct_attr[(c)&0xff] & _CT_UPPER)
#define isxdigit(c) (_ct_attr[(c)&0xff] & _CT_UPPER)

#define _CT_ASCII_LOWER 0x20
#define toupper(ch) (islower(ch) ? ((ch) & ~_CT_ASCII_LOWER) : ch)
#define tolower(ch) (isupper(ch) ? ((ch) | _CT_ASCII_LOWER) : ch)

#endif

