#ifndef DIS386_H_
#define DIS386_H_

#include "ktypes.h"

typedef struct disinstr_t disinstr_t;
struct disinstr_t {
    int bytes;
    char mne[64];
    char args[64];
};

void disasm(uint8_t __far *csip, uint32_t offset, disinstr_t *out, int is32);

#endif
