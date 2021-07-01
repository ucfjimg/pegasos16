#include "ctype.h"


/* ctype table for US ASCII
 */
unsigned char _ct_attr[256] = 
{
    /* 00 */ _CT_CNTRL,
    /* 01 */ _CT_CNTRL,
    /* 02 */ _CT_CNTRL,
    /* 03 */ _CT_CNTRL,
    /* 04 */ _CT_CNTRL,
    /* 05 */ _CT_CNTRL,
    /* 06 */ _CT_CNTRL,
    /* 07 */ _CT_CNTRL,
    /* 08 */ _CT_CNTRL,
    /* 09 */ _CT_CNTRL | _CT_SPACE,                 /* tab */
    /* 0a */ _CT_CNTRL | _CT_SPACE,                 /* newline */
    /* 0b */ _CT_CNTRL | _CT_SPACE,                 /* vertical tab */
    /* 0c */ _CT_CNTRL | _CT_SPACE,                 /* formfeed */
    /* 0d */ _CT_CNTRL | _CT_SPACE,                 /* carriage return */
    /* 0e */ _CT_CNTRL,
    /* 0f */ _CT_CNTRL,
    /* 10 */ _CT_CNTRL,
    /* 11 */ _CT_CNTRL,
    /* 12 */ _CT_CNTRL,
    /* 13 */ _CT_CNTRL,
    /* 14 */ _CT_CNTRL,
    /* 15 */ _CT_CNTRL,
    /* 16 */ _CT_CNTRL,
    /* 17 */ _CT_CNTRL,
    /* 18 */ _CT_CNTRL,
    /* 19 */ _CT_CNTRL,
    /* 1a */ _CT_CNTRL,
    /* 1b */ _CT_CNTRL,
    /* 1c */ _CT_CNTRL,
    /* 1d */ _CT_CNTRL,
    /* 1e */ _CT_CNTRL,
    /* 1f */ _CT_CNTRL,
    /* 20 */ _CT_SPACE,                 /* space */
    /* 21 */ _CT_PRINT,                 /* ! */
    /* 22 */ _CT_PRINT,                 /* " */
    /* 23 */ _CT_PRINT,                 /* # */
    /* 24 */ _CT_PRINT,                 /* $ */
    /* 25 */ _CT_PRINT,                 /* % */
    /* 26 */ _CT_PRINT,                 /* & */
    /* 27 */ _CT_PRINT,                 /* ' */
    /* 28 */ _CT_PRINT,                 /* ( */
    /* 29 */ _CT_PRINT,                 /* ) */
    /* 2a */ _CT_PRINT,                 /* * */
    /* 2b */ _CT_PRINT,                 /* + */
    /* 2c */ _CT_PRINT,                 /* , */
    /* 2d */ _CT_PRINT,                 /* - */
    /* 2e */ _CT_PRINT,                 /* . */
    /* 2f */ _CT_PRINT,                 /* / */
    /* 30 */ _CT_PRINT | _CT_DIGIT | _CT_XDIGIT,    /* 0 */ 
    /* 31 */ _CT_PRINT | _CT_DIGIT | _CT_XDIGIT,
    /* 32 */ _CT_PRINT | _CT_DIGIT | _CT_XDIGIT,
    /* 33 */ _CT_PRINT | _CT_DIGIT | _CT_XDIGIT,
    /* 34 */ _CT_PRINT | _CT_DIGIT | _CT_XDIGIT,
    /* 35 */ _CT_PRINT | _CT_DIGIT | _CT_XDIGIT,
    /* 36 */ _CT_PRINT | _CT_DIGIT | _CT_XDIGIT,
    /* 37 */ _CT_PRINT | _CT_DIGIT | _CT_XDIGIT,
    /* 38 */ _CT_PRINT | _CT_DIGIT | _CT_XDIGIT,
    /* 39 */ _CT_PRINT | _CT_DIGIT | _CT_XDIGIT,    /* 9 */
    /* 3a */ _CT_PRINT,                 /* : */
    /* 3b */ _CT_PRINT,                 /* ; */
    /* 3c */ _CT_PRINT,                 /* < */
    /* 3d */ _CT_PRINT,                 /* = */
    /* 3e */ _CT_PRINT,                 /* > */
    /* 3f */ _CT_PRINT,                 /* ? */    
    /* 40 */ _CT_PRINT,                 /* @ */
    /* 41 */ _CT_PRINT | _CT_UPPER | _CT_XDIGIT,     /* A */
    /* 42 */ _CT_PRINT | _CT_UPPER | _CT_XDIGIT,
    /* 43 */ _CT_PRINT | _CT_UPPER | _CT_XDIGIT,
    /* 44 */ _CT_PRINT | _CT_UPPER | _CT_XDIGIT,
    /* 45 */ _CT_PRINT | _CT_UPPER | _CT_XDIGIT,
    /* 46 */ _CT_PRINT | _CT_UPPER | _CT_XDIGIT,     /* F */
    /* 47 */ _CT_PRINT | _CT_UPPER,
    /* 48 */ _CT_PRINT | _CT_UPPER,
    /* 49 */ _CT_PRINT | _CT_UPPER,
    /* 4a */ _CT_PRINT | _CT_UPPER,
    /* 4b */ _CT_PRINT | _CT_UPPER,
    /* 4c */ _CT_PRINT | _CT_UPPER,
    /* 4d */ _CT_PRINT | _CT_UPPER,
    /* 4e */ _CT_PRINT | _CT_UPPER,
    /* 4f */ _CT_PRINT | _CT_UPPER,
    /* 50 */ _CT_PRINT | _CT_UPPER,
    /* 51 */ _CT_PRINT | _CT_UPPER,
    /* 52 */ _CT_PRINT | _CT_UPPER,
    /* 53 */ _CT_PRINT | _CT_UPPER,
    /* 54 */ _CT_PRINT | _CT_UPPER,
    /* 55 */ _CT_PRINT | _CT_UPPER,
    /* 56 */ _CT_PRINT | _CT_UPPER,
    /* 57 */ _CT_PRINT | _CT_UPPER,
    /* 58 */ _CT_PRINT | _CT_UPPER,
    /* 59 */ _CT_PRINT | _CT_UPPER,
    /* 5a */ _CT_PRINT | _CT_UPPER,                  /* Z */
    /* 5b */ _CT_PRINT,                 /* [ */
    /* 5c */ _CT_PRINT,                 /* \ */
    /* 5d */ _CT_PRINT,                 /* ] */
    /* 5e */ _CT_PRINT,                 /* ^ */
    /* 5f */ _CT_PRINT,                 /* _ */
    /* 60 */ _CT_PRINT,                 /* ` */
    /* 61 */ _CT_PRINT | _CT_LOWER | _CT_XDIGIT,    /* a */ 
    /* 62 */ _CT_PRINT | _CT_LOWER | _CT_XDIGIT,
    /* 63 */ _CT_PRINT | _CT_LOWER | _CT_XDIGIT,
    /* 64 */ _CT_PRINT | _CT_LOWER | _CT_XDIGIT,
    /* 65 */ _CT_PRINT | _CT_LOWER | _CT_XDIGIT,
    /* 66 */ _CT_PRINT | _CT_LOWER | _CT_XDIGIT,    /* f */
    /* 67 */ _CT_PRINT | _CT_LOWER,
    /* 68 */ _CT_PRINT | _CT_LOWER,
    /* 69 */ _CT_PRINT | _CT_LOWER,
    /* 6a */ _CT_PRINT | _CT_LOWER,
    /* 6b */ _CT_PRINT | _CT_LOWER,
    /* 6c */ _CT_PRINT | _CT_LOWER,
    /* 6d */ _CT_PRINT | _CT_LOWER,
    /* 6e */ _CT_PRINT | _CT_LOWER,
    /* 6f */ _CT_PRINT | _CT_LOWER,
    /* 70 */ _CT_PRINT | _CT_LOWER,
    /* 71 */ _CT_PRINT | _CT_LOWER,
    /* 72 */ _CT_PRINT | _CT_LOWER,
    /* 73 */ _CT_PRINT | _CT_LOWER,
    /* 74 */ _CT_PRINT | _CT_LOWER,
    /* 75 */ _CT_PRINT | _CT_LOWER,
    /* 76 */ _CT_PRINT | _CT_LOWER,
    /* 77 */ _CT_PRINT | _CT_LOWER,
    /* 78 */ _CT_PRINT | _CT_LOWER,
    /* 79 */ _CT_PRINT | _CT_LOWER,
    /* 7a */ _CT_PRINT | _CT_LOWER,
    /* 7b */ _CT_PRINT,                 /* { */
    /* 7c */ _CT_PRINT,                 /* | */
    /* 7d */ _CT_PRINT,                 /* { */
    /* 7e */ _CT_PRINT,                 /* ~ */
    /* 7f */ _CT_CNTRL,                 /* DEL */

    /* 80 */ 0,
    /* 81 */ 0,
    /* 82 */ 0,
    /* 83 */ 0,
    /* 84 */ 0,
    /* 85 */ 0,
    /* 86 */ 0,
    /* 87 */ 0,
    /* 88 */ 0,
    /* 89 */ 0,
    /* 8a */ 0,
    /* 8b */ 0,
    /* 8c */ 0,
    /* 8d */ 0,
    /* 8e */ 0,
    /* 8f */ 0,
    /* 90 */ 0,
    /* 91 */ 0,
    /* 92 */ 0,
    /* 93 */ 0,
    /* 94 */ 0,
    /* 95 */ 0,
    /* 96 */ 0,
    /* 97 */ 0,
    /* 98 */ 0,
    /* 99 */ 0,
    /* 9a */ 0,
    /* 9b */ 0,
    /* 9c */ 0,
    /* 9d */ 0,
    /* 9e */ 0,
    /* 9f */ 0,
    /* a0 */ 0,
    /* a1 */ 0,
    /* a2 */ 0,
    /* a3 */ 0,
    /* a4 */ 0,
    /* a5 */ 0,
    /* a6 */ 0,
    /* a7 */ 0,
    /* a8 */ 0,
    /* a9 */ 0,
    /* aa */ 0,
    /* ab */ 0,
    /* ac */ 0,
    /* ad */ 0,
    /* ae */ 0,
    /* af */ 0,
    /* b0 */ 0,
    /* b1 */ 0,
    /* b2 */ 0,
    /* b3 */ 0,
    /* b4 */ 0,
    /* b5 */ 0,
    /* b6 */ 0,
    /* b7 */ 0,
    /* b8 */ 0,
    /* b9 */ 0,
    /* ba */ 0,
    /* bb */ 0,
    /* bc */ 0,
    /* bd */ 0,
    /* be */ 0,
    /* bf */ 0,
    /* c0 */ 0,
    /* c1 */ 0,
    /* c2 */ 0,
    /* c3 */ 0,
    /* c4 */ 0,
    /* c5 */ 0,
    /* c6 */ 0,
    /* c7 */ 0,
    /* c8 */ 0,
    /* c9 */ 0,
    /* ca */ 0,
    /* cb */ 0,
    /* cc */ 0,
    /* cd */ 0,
    /* ce */ 0,
    /* cf */ 0,
    /* d0 */ 0,
    /* d1 */ 0,
    /* d2 */ 0,
    /* d3 */ 0,
    /* d4 */ 0,
    /* d5 */ 0,
    /* d6 */ 0,
    /* d7 */ 0,
    /* d8 */ 0,
    /* d9 */ 0,
    /* da */ 0,
    /* db */ 0,
    /* dc */ 0,
    /* dd */ 0,
    /* de */ 0,
    /* df */ 0,
    /* e0 */ 0,
    /* e1 */ 0,
    /* e2 */ 0,
    /* e3 */ 0,
    /* e4 */ 0,
    /* e5 */ 0,
    /* e6 */ 0,
    /* e7 */ 0,
    /* e8 */ 0,
    /* e9 */ 0,
    /* ea */ 0,
    /* eb */ 0,
    /* ec */ 0,
    /* ed */ 0,
    /* ee */ 0,
    /* ef */ 0,
    /* f0 */ 0,
    /* f1 */ 0,
    /* f2 */ 0,
    /* f3 */ 0,
    /* f4 */ 0,
    /* f5 */ 0,
    /* f6 */ 0,
    /* f7 */ 0,
    /* f8 */ 0,
    /* f9 */ 0,
    /* fa */ 0,
    /* fb */ 0,
    /* fc */ 0,
    /* fd */ 0,
    /* fe */ 0,
    /* ff */ 0,
};

