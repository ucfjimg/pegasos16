#include "dma.h"
#include "ktypes.h"
#include "panic.h"
#include "ports.h"
#include "printk.h"
#include "task.h"
#include <sys/types.h>

/* ports that are on the actual DMA chips */
#define DMA8_START_PORT(ch)  (((ch) & 0x03) << 1)
#define DMA8_COUNT_PORT(ch)  (DMA8_START_PORT(ch) + 1)
#define DMA8_STATUS_PORT 0x08           /* read */
#define DMA8_COMMAND_PORT 0x08          /* write */
#define DMA8_REQUEST_PORT 0x09
#define DMA8_1CHAN_MASK_PORT 0x0a
#define DMA8_MODE_PORT 0x0b
#define DMA8_FF_RESET_PORT 0x0c
#define DMA8_INTERMEDIATE_PORT 0x0d     /* read */
#define DMA8_MASTER_RESET_PORT 0x0d     /* write */
#define DMA8_MASK_RESET_PORT 0x0e
#define DMA8_MCHAN_MASK_PORT 0x0f

#define DMA16_START_PORT(ch) (0xc0 | (((ch) & 0x03) << 2))
#define DMA16_COUNT_PORT(ch)  (DMA16_START_PORT(ch) + 1)
#define DMA16_STATUS_PORT 0xd0          /* read */
#define DMA16_COMMAND_PORT 0xd0         /* write */
#define DMA16_REQUEST_PORT 0xd2
#define DMA16_1CHAN_MASK_PORT 0xd4
#define DMA16_MODE_PORT 0xd6
#define DMA16_FF_RESET_PORT 0xd8
#define DMA16_INTERMEDIATE_PORT 0xda    /* read */
#define DMA16_MASTER_RESET_PORT 0xda    /* write */
#define DMA16_MASK_RESET_PORT 0xdc
#define DMA16_MCHAN_MASK_PORT 0xde

/* page registers to fill out the address space to 20 bits 
 * unfortunately these ports are all over the place, not 
 * sequential
 */
static const uint8_t page_ports[] =
{
    0x87,
    0x83,
    0x81,
    0x82,
    0x8f,
    0x8b,
    0x89,
    0x8a,  
};


#define DMA_1CHAN_MASK      0x04
#define DMA_1CHAN_UNMASK    0x00

#define IS_16BIT_CHANNEL(ch) ((ch) > 3)

#define MAX_ADDRESS 0xffffful
#define MAX_XFER 0x10000ul
#define ADDR_PAGE(addr) (uint8_t)(((addr) >> 16) & 0x0f)

static critsect_t dma_cs;

/* Initialize the DMA driver. 
 */
void dma_init(void)
{
    CS_INIT(&dma_cs);
}

/* mask the given channel so DRQ's are ignored for now. 
 */
void dma_mask_channel(int chan)
{
    /* this and unmask are safe w/o taking the critical section
     * as they won't interfere with programming other operations
     * nor mess with the hi/lo flipflop
     */ 
    uint16_t port = IS_16BIT_CHANNEL(chan) ? DMA16_1CHAN_MASK_PORT : DMA8_1CHAN_MASK_PORT;
    uint8_t val = (uint8_t)(DMA_1CHAN_MASK | (chan & 3));
    outb(port, val);
}

/* unmask the given channel so DRQ's are acknowledged. 
 */
void dma_unmask_channel(int chan)
{
    uint16_t port = IS_16BIT_CHANNEL(chan) ? DMA16_1CHAN_MASK_PORT : DMA8_1CHAN_MASK_PORT;
    uint8_t val = (uint8_t)(DMA_1CHAN_UNMASK | (chan & 3));
    outb(port, val);    
}

/* set up a DMA transfer. chan and count are obvious; count should NOT be
 * decremented by one. linaddr is the 20-bit linear address; i.e. 
 * (seg << 4) + offset. mode should be DMA_MODE_READ or DMA_MODE_WRITE, 
 * or'ed with one of the three transfer modes.
 */
int dma_setup(int chan, uint32_t linaddr, uint32_t count, uint8_t mode)
{
    uint8_t page;
    uint8_t top_page;
    uint16_t port;

    /* validate address is in first 1M of memory and transfer
     * doesn't overrun a 64k physical block 
     */
    if (linaddr > MAX_ADDRESS) {
        kprintf("DMA: transfer to %08lx which is outside RAM\n", linaddr);
        return -1;
    }

    if (count > MAX_XFER) {
        kprintf("DMA: count %08lx is more than 64k\n", count);
        return -1;
    }

    page = ADDR_PAGE(linaddr);
    top_page = ADDR_PAGE(linaddr + count - 1);
    if (page != top_page) {
        kprintf("DMA: address %08lx count %08lx crosses physical 64k page\n",
            linaddr,
            count
        );
    }

    /* controller is programmed with count-1 to allow a full 64k count to
     * be put in a 16-bit register
     */
    count--;

    CS_ENTER(&dma_cs);

    if (IS_16BIT_CHANNEL(chan)) {
        panic("16-bit DMA is not supported yet!");
    } else {
        /* to save registers, the chip maintains a FF so that an 8 bit 
         * register can take a low byte followed by a high byte for a 16-bit
         * value. this resets the FF to be ready for a low order byte.
         */
        outb(DMA8_FF_RESET_PORT, 0xff);    
        port = DMA8_START_PORT(chan);
        outb(port, (uint8_t)(linaddr & 0xff));
        outb(port, (uint8_t)((linaddr >> 8) & 0xff));

        outb(DMA8_FF_RESET_PORT, 0xff);    
        port = DMA8_COUNT_PORT(chan);
        outb(port, (uint8_t)(count & 0xff));
        outb(port, (uint8_t)((count >> 8) & 0xff));

        outb(page_ports[chan], page);

        mode |= (chan & 0x03);
        outb(DMA8_MODE_PORT, mode);
    }

    dma_unmask_channel(chan);

    CS_LEAVE(&dma_cs);    

    return 0;
}


