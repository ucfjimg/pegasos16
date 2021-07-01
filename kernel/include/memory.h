#ifndef MEMORY_H_
#define MEMORY_H_

#include "ktypes.h"
#include <sys/types.h>

extern void mem_init(uint16_t dmaend, uint16_t kendseg);

/* Allocate memory from the DMA pool - guaranteed to not
 * cross a 64k physical boundary. This pool is very small.
 * 
 * Paragraph aligned.
 */
extern uint8_t __far *mem_dma_alloc(uint16_t sz);
extern void mem_dma_free(void __far *blk);
extern void mem_dma_dump(void);

/* Allocate a far block from the main heap. This is most of main
 * memory.
 * 
 * Paragraph aligned.
 */
extern void __far *kmalloc(uint32_t size);
extern void kfree(void __far *ptr);

/* Allocate a near block from the near heap. This is for things 
 * that should be a near pointer to be fast or must have the
 * same segment as DS/SS for memory model reasons. This pool
 * is about 32k.
 *
 * Word aligned.
 */
extern void *near_malloc(size_t size);
extern void near_free(void *p);
extern void near_dump(void);


#endif
