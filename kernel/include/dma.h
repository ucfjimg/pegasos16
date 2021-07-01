#ifndef DMA_H_
#define DMA_H_

#include "ktypes.h"

/* low 2 bits of mode are channel */
#define DMA_MODE_VERIFY 0x00
#define DMA_MODE_WRITE  0x04    /* to memory */
#define DMA_MODE_READ   0x08    /* from memory */
#define DMA_AUTOINIT    0x10    /* doesn't work */
#define DMA_ADDR_DEC    0x20    /* default is increment */
#define DMA_MODE_DEMAND 0x00
#define DMA_MODE_SINGLE 0x40
#define DMA_MODE_BLOCK  0x80

extern void dma_init(void);
extern void dma_mask_channel(int chan);
extern void dma_unmask_channel(int chan);
extern int  dma_setup(int chan, uint32_t linaddr, uint32_t count, uint8_t mode);


#endif
