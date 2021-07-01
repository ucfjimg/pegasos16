//
// Useful global macros
//
#ifndef KMACRO_H_
#define KMACRO_H_

#include "ktypes.h"

// convert a far pointer to a 20-bit linear address
#define FP_TO_LINEAR(fp) \
	(((((uint32_t)(fp)) >> 12) & 0xffff0) + (((uint32_t)(fp))&0xffff))


// return the dma page of a far pointer pointer. each DMA page is 64k
// and thus in a 20 bit address space there are only 16 of them
//
#define FP_TO_DMA_PAGE(fp) \
	((FP_TO_LINEAR(fp) >> 16) & 0x0000f)


// create a far pointer from an explicit segment and offset
//
#ifndef MK_FP
# define MK_FP(seg,off) ((void _seg *)(seg) + (void near *)off)
#endif

#endif

