#ifndef CMOS_H_
#define CMOS_H_

#include "ktypes.h"
#include <sys/types.h>

extern uint8_t cmos_read(uint8_t reg);

#define CMOS_FDD_REG    0x10
#define CMOS_FDD_NONE   0
#define CMOS_FDD_360    1
#define CMOS_FDD_1_2    2
#define CMOS_FDD_720    3
#define CMOS_FDD_1_44   4
#define CMOS_FDD_MAX    4

/* r is the value in register 10 */
#define CMOS_FDD_UNIT_0(r) (((r) >> 4) & 0x0f)
#define CMOS_FDD_UNIT_1(r) ((r) & 0x0f)

#endif
