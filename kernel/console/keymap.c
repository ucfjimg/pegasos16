#include "console.h"

#define MKMETA(meta, fl) \
    { (char)(meta & 0xff), (char)((meta >> 8) & 0xff), SC_META | fl }

/* US ASCII 101-key scan code translation 
 */
scandef_t scascii[] =
{
    /* 0x00 */ { 0, 0, 0 },
    /* 0x01 */ { 0x1b, 0, 0 },              /* ESC */
    /* 0x02 */ { '1', '!', SC_SHIFTS },
    /* 0x03 */ { '2', '@', SC_SHIFTS },
    /* 0x04 */ { '3', '#', SC_SHIFTS },
    /* 0x05 */ { '4', '$', SC_SHIFTS },
    /* 0x06 */ { '5', '%', SC_SHIFTS },
    /* 0x07 */ { '6', '^', SC_SHIFTS },
    /* 0x08 */ { '7', '&', SC_SHIFTS },
    /* 0x09 */ { '8', '*', SC_SHIFTS },
    /* 0x0A */ { '9', '(', SC_SHIFTS },
    /* 0x0B */ { '0', ')', SC_SHIFTS },
    /* 0x0C */ { '-', '_', SC_SHIFTS },
    /* 0x0D */ { '=', '+', SC_SHIFTS },
    /* 0x0E */ { 0x08, 0,  0 },             /* backspace */
    /* 0x0F */ { 0x09, 0,  0 },             /* tab */
    /* 0x10 */ { 'q', 'Q', SC_SHIFTS | SC_CAPSLOCK | SC_CTRL },
    /* 0x11 */ { 'w', 'W', SC_SHIFTS | SC_CAPSLOCK | SC_CTRL },
    /* 0x12 */ { 'e', 'E', SC_SHIFTS | SC_CAPSLOCK | SC_CTRL },
    /* 0x13 */ { 'r', 'R', SC_SHIFTS | SC_CAPSLOCK | SC_CTRL },
    /* 0x14 */ { 't', 'T', SC_SHIFTS | SC_CAPSLOCK | SC_CTRL },
    /* 0x15 */ { 'y', 'Y', SC_SHIFTS | SC_CAPSLOCK | SC_CTRL },
    /* 0x16 */ { 'u', 'U', SC_SHIFTS | SC_CAPSLOCK | SC_CTRL },
    /* 0x17 */ { 'i', 'I', SC_SHIFTS | SC_CAPSLOCK | SC_CTRL },
    /* 0x18 */ { 'o', 'O', SC_SHIFTS | SC_CAPSLOCK | SC_CTRL },
    /* 0x19 */ { 'p', 'P', SC_SHIFTS | SC_CAPSLOCK | SC_CTRL },
    /* 0x1A */ { '[', '{', SC_SHIFTS | SC_CTRL },
    /* 0x1B */ { ']', '}', SC_SHIFTS | SC_CTRL },
    /* 0x1C */ { '\n', 0, SC_E0 },              /* enter */
    /* 0x1D */ MKMETA(SC_META_LCTRL, SC_E0),
    /* 0x1E */ { 'a', 'A', SC_SHIFTS | SC_CAPSLOCK | SC_CTRL },
    /* 0x1F */ { 's', 'S', SC_SHIFTS | SC_CAPSLOCK | SC_CTRL },
    /* 0x20 */ { 'd', 'D', SC_SHIFTS | SC_CAPSLOCK | SC_CTRL },
    /* 0x21 */ { 'f', 'F', SC_SHIFTS | SC_CAPSLOCK | SC_CTRL },
    /* 0x22 */ { 'g', 'G', SC_SHIFTS | SC_CAPSLOCK | SC_CTRL },
    /* 0x23 */ { 'h', 'H', SC_SHIFTS | SC_CAPSLOCK | SC_CTRL },
    /* 0x24 */ { 'j', 'J', SC_SHIFTS | SC_CAPSLOCK | SC_CTRL },
    /* 0x25 */ { 'k', 'K', SC_SHIFTS | SC_CAPSLOCK | SC_CTRL },
    /* 0x26 */ { 'l', 'L', SC_SHIFTS | SC_CAPSLOCK | SC_CTRL },
    /* 0x27 */ { ';', ':', SC_SHIFTS },
    /* 0x28 */ { '\'', '"', SC_SHIFTS },
    /* 0x29 */ { '`', '~', SC_SHIFTS },
    /* 0x2A */ MKMETA(SC_META_LSHIFT, 0),
    /* 0x2B */ { '\\', '|', SC_SHIFTS | SC_CTRL },
    /* 0x2C */ { 'z', 'Z', SC_SHIFTS | SC_CAPSLOCK | SC_CTRL },
    /* 0x2D */ { 'x', 'X', SC_SHIFTS | SC_CAPSLOCK | SC_CTRL },
    /* 0x2E */ { 'c', 'C', SC_SHIFTS | SC_CAPSLOCK | SC_CTRL },
    /* 0x2F */ { 'v', 'V', SC_SHIFTS | SC_CAPSLOCK | SC_CTRL },
    /* 0x30 */ { 'b', 'B', SC_SHIFTS | SC_CAPSLOCK | SC_CTRL },
    /* 0x31 */ { 'n', 'N', SC_SHIFTS | SC_CAPSLOCK | SC_CTRL },
    /* 0x32 */ { 'm', 'M', SC_SHIFTS | SC_CAPSLOCK | SC_CTRL },
    /* 0x33 */ { ',', '<', SC_SHIFTS },
    /* 0x34 */ { '.', '>', SC_SHIFTS },
    /* 0x35 */ { '/', '?', SC_SHIFTS | SC_E0 },
    /* 0x36 */ MKMETA(SC_META_RSHIFT, 0),
    /* 0x37 */ { '*', 0, 0 },                   /* numpad * */
    /* 0x38 */ MKMETA(SC_META_LALT, SC_E0),
    /* 0x39 */ { ' ', 0, 0 },
    /* 0x3A */ MKMETA(SC_META_CAPSLOCK, 0),
    /* 0x3B */ { SC_EXT_F1, 0, SC_EXT },
    /* 0x3C */ { SC_EXT_F2, 0, SC_EXT },
    /* 0x3D */ { SC_EXT_F3, 0, SC_EXT },
    /* 0x3E */ { SC_EXT_F4, 0, SC_EXT },
    /* 0x3F */ { SC_EXT_F5, 0, SC_EXT },
    /* 0x40 */ { SC_EXT_F6, 0, SC_EXT },
    /* 0x41 */ { SC_EXT_F7, 0, SC_EXT },
    /* 0x42 */ { SC_EXT_F8, 0, SC_EXT },
    /* 0x43 */ { SC_EXT_F9, 0, SC_EXT },
    /* 0x44 */ { SC_EXT_F10, 0, SC_EXT },
    /* 0x45 */ MKMETA(SC_META_NUMLOCK, 0),
    /* 0x46 */ MKMETA(SC_META_SCRLOCK, 0),
    /* 0x47 */ { '7', SC_EXT_HOME, SC_NUML | SC_E0 },
    /* 0x48 */ { '8', SC_EXT_UP,   SC_NUML | SC_E0 },
    /* 0x49 */ { '9', SC_EXT_PGUP, SC_NUML | SC_E0 },
    /* 0x4A */ { '-', 0, 0 },                   /* numpad - */
    /* 0x4B */ { '4', SC_EXT_LEFT, SC_NUML | SC_E0 },
    /* 0x4C */ { '5', 0, 0 },
    /* 0x4D */ { '6', SC_EXT_RIGHT, SC_NUML | SC_E0 },
    /* 0x4E */ { '+', 0, 0 },                   /* numpad - */
    /* 0x4F */ { '1', SC_EXT_END, SC_NUML | SC_E0 },
    /* 0x50 */ { '2', SC_EXT_DOWN, SC_NUML | SC_E0 },
    /* 0x51 */ { '3', SC_EXT_PGDN, SC_NUML | SC_E0 },
    /* 0x52 */ { '0', SC_EXT_INS, SC_NUML | SC_E0 },
    /* 0x53 */ { '.', SC_EXT_DEL, SC_NUML | SC_E0},
    /* 0x54 */ { 0, 0, 0 },
    /* 0x55 */ { 0, 0, 0 },
    /* 0x56 */ { 0, 0, 0 },
    /* 0x57 */ { SC_EXT_F11, 0, SC_EXT },
    /* 0x58 */ { SC_EXT_F12, 0, SC_EXT },
    /* 0x59 */ { 0, 0, 0 },
    /* 0x5A */ { 0, 0, 0 },
    /* 0x5B */ { SC_WINDOWS, 0, SC_E0 },
    /* 0x5C */ { SC_WINDOWS, 0, SC_E0 },
    /* 0x5D */ { SC_CONTEXT, 0, SC_E0 },
};

int nscascii = sizeof(scascii) / sizeof(scascii[0]);

/* extended
 * ok E0 1C - np enter 
 * ok E0 1D - ctrl
 * ok E0 35 - slash (np)
 * ok E0 47 - home
 * ok E0 48 - up
 * ok E0 49 - page up
 * ok E0 4B - left
 * ok E0 4D - right
 * ok E0 4F - end
 * ok E0 50 - down
 * ok E0 51 - page down
 * ok E0 52 - insert
 * ok E0 53 - del
 * 
 * 
 * ok E0 5B - Windows (Meta)
 * ok E0 38 - right alt
 * ok E0 5C - right Windows (Meta)
 * ok E0 5D - context
 * 
 * E0 2A E0 37 (off E0 B7 E0 AA) - print screen
 * E1 1D 45 E1 9D C5 - pause/break
 * 
 */