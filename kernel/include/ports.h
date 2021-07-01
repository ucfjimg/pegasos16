#ifndef PORTS_H_
#define PORTS_H_

#include "ktypes.h"

extern void outb(uint16_t port, uint8_t data);
extern uint8_t inb(uint16_t port);

#endif
