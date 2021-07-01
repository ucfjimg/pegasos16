#include "memory.h"
#include "panic.h"
#include "printk.h"

typedef struct memhdr_t memhdr_t;
struct memhdr_t
{
	uint16_t next;
	uint16_t para;
	uint16_t flags;
	uint16_t pad[5];
};

typedef struct nearhdr_t nearhdr_t;
struct nearhdr_t
{
	nearhdr_t *next;
	uint16_t size;
};

/* to avoid having another word, we pack the used bit in the
 * bottom of the size word. this is ok because this heap
 * is word aligned so the bottom bit of the block size is
 * always zero.
 */
#define NHDR_GET_SIZE(x)    (((x)->size) & 0xfffe)
#define NHDR_SET_SIZE(x,sz) (x)->size = (((x)->size & 1) | (size_t)((sz) & 0xfffe))
#define NHDR_GET_USED(x)    (((x)->size) & 0x0001)
#define NHDR_SET_USED(x)    (x)->size |= 0x0001
#define NHDR_SET_FREE(x)    (x)->size &= ~0x001

#define NHDR_FIRST()  ((nearhdr_t*)heap)

#define NEAR_HEAP_SIZE	32768

static uint8_t heap[NEAR_HEAP_SIZE];



#define DMA_START 0x0050
#define DMAP(offs) ((memhdr_t __far *)MKFP(DMA_START, (offs)))

#define ROUND_TO_PARA(x) (((x) + PARASIZE - 1) >> PARASHIFT) 

#define DMANULL 0xffff

#define BLK_INUSE 0x0001

#define FMAP(seg) ((memhdr_t __far *)MKFP((seg), 0))

static uint16_t dmaheap;
static uint16_t farheap;

#define BIOS_SEG 	0x0040
#define BIOS_RAMK	0x0013

void mem_init(uint16_t dmaend, uint16_t ksegend)
{
	memhdr_t __far *h = DMAP(0);
	uint16_t __far *ramk = MKFP(BIOS_SEG, BIOS_RAMK);
	uint16_t topseg = *ramk << (KSHIFT - PARASHIFT);
	nearhdr_t *nhdr = NHDR_FIRST();


	kprintf("mem: total conventional memory %uk\n", *ramk);

	h->next = DMANULL;
	h->para = dmaend-DMA_START-1;
	h->flags = 0;
	dmaheap = 0;

	farheap = ksegend;
	h = MKFP(ksegend, 0);
	h->next = 0;
	h->para = topseg-farheap-1;
	h->flags = 0;

	nhdr->next = NULL;
	NHDR_SET_SIZE(nhdr, NEAR_HEAP_SIZE - sizeof(*nhdr));
}

uint8_t __far *mem_dma_alloc(uint16_t sz)
{
	uint16_t boffs;
	uint16_t noffs;
	memhdr_t __far *h = NULL;
	memhdr_t __far *nh;

	/* protect against overflow - you aren't getting a block
	 * this big anyway.
	 */
	if (sz > 0xfff0) {
		return NULL;
	}

	sz = ROUND_TO_PARA(sz);

	for (boffs = dmaheap; boffs != DMANULL; boffs = DMAP(boffs)->next) {
		h = DMAP(boffs);

		if (h->flags & BLK_INUSE) {
			continue;
		}

		if (h->para >= sz) {
			break;
		}
	}
	
	if (boffs == DMANULL) {
		return NULL;
	}

	h->flags |= BLK_INUSE;

	if (h->para > sz+1) {
		noffs = boffs + ((sz + 1) << PARASHIFT);
		nh = DMAP(noffs);

		nh->next = h->next;
		h->next = noffs;

		nh->para = h->para - 1 - sz;
		h->para = sz;
		
		nh->flags &= ~BLK_INUSE;
	}
	
	return (void __far *)(h + 1);
}

void mem_dma_free(void __far *p)
{
	uint16_t seg;
	uint16_t offs;
	uint16_t poffs;
	uint16_t boffs;
	memhdr_t __far *prev;
	memhdr_t __far *curr;
	memhdr_t __far *next;

	seg = KSEG_OF(p);
	offs = KOFFS_OF(p);

	if (seg != DMA_START) {
		panic("mem_dma_free: pointer is not in DMA segment");
	}

	
	offs -= PARASIZE;
	poffs = DMANULL;

	for (boffs = dmaheap; boffs != DMANULL; boffs = DMAP(boffs)->next) {
		if (boffs == offs) {
			break;
		}
		poffs = boffs;
	}

	if (boffs == DMANULL) {
		panic("mem_dma_free: pointer is not a valid DMA block");
	}

	curr = DMAP(boffs);

	if (poffs != DMANULL) {
		prev = DMAP(poffs);
		if ((prev->flags & BLK_INUSE) == 0) {
			/* previous is free, we can merge it */
			prev->para += 1 + curr->para;
			prev->next = curr->next;

			boffs = poffs;
			curr = DMAP(boffs);
		}
	}

	if (curr->next != DMANULL) {
		next = DMAP(curr->next);
		if ((next->flags & BLK_INUSE) == 0) {
			/* next is free, we can merge it */
			curr->para += 1 + next->para;
			curr->next = next->next;
		}
	}
	
	curr->flags &= ~BLK_INUSE;
}

void mem_dma_dump(void)
{
	uint16_t boffs;
	memhdr_t __far *p;

	for (boffs = dmaheap; boffs != DMANULL; boffs = DMAP(boffs)->next) {
		p = DMAP(boffs);
		kprintf("block %p size %p flags %p\n",
			boffs,
			p->para,
			p->flags);
	}
}

void __far *kmalloc(uint32_t size)
{
	uint16_t bseg;
	uint16_t nseg;
	uint16_t sz;
	memhdr_t __far *h = NULL;
	memhdr_t __far *nh;

	/* overflow check - ensure paragraph doesn't
	 * truncate accidental huge size
	 */
	if (size > 0xFFFF0ul) {
		return NULL;
	}

	sz = (uint16_t)ROUND_TO_PARA(size);

	for (bseg = farheap; bseg != 0; bseg = FMAP(bseg)->next) {
		h = FMAP(bseg);

		if (h->flags & BLK_INUSE) {
			continue;
		}

		if (h->para >= sz) {
			break;
		}
	}
	
	if (bseg == 0) {
		return NULL;
	}

	h->flags |= BLK_INUSE;

	if (h->para > sz+1) {
		nseg = bseg + sz + 1;
		nh = FMAP(nseg);

		nh->next = h->next;
		h->next = nseg;

		nh->para = h->para - 1 - sz;
		h->para = sz;
		
		nh->flags &= ~BLK_INUSE;
	}
	
	return (void __far *)(h + 1);
}

void kfree(void __far *ptr)
{
	uint32_t lin;
	uint8_t __far *p = ptr;
	uint16_t seg;
	uint16_t offs;
	uint16_t pseg;
	uint16_t bseg;
	memhdr_t __far *prev;
	memhdr_t __far *curr;
	memhdr_t __far *next;

	lin = KLINEAR(ptr) - sizeof(memhdr_t);
	seg = (uint16_t)(lin >> PARASHIFT);
	offs = (uint16_t)(lin & 0x000f);

	if (offs != 0) {
		kprintf("kfree %lp\n", p);
		panic("kfree: pointer is not para aligned");
	}

	pseg = 0;
	for (bseg = farheap; bseg != 0; bseg = FMAP(bseg)->next) {
		if (bseg == seg) {
			break;
		}
		pseg = bseg;
	}

	if (bseg == 0) {
		panic("kfree: pointer is not an alloc'ed block");
	}

	curr = FMAP(bseg);

	if (pseg != 0) {
		prev = FMAP(pseg);
		if ((prev->flags & BLK_INUSE) == 0) {
			/* previous is free, we can merge it */
			prev->para += 1 + curr->para;
			prev->next = curr->next;

			bseg = pseg;
			curr = FMAP(bseg);
		}
	}

	if (curr->next != 0) {
		next = FMAP(curr->next);
		if ((next->flags & BLK_INUSE) == 0) {
			/* next is free, we can merge it */
			curr->para += 1 + next->para;
			curr->next = next->next;
		}
	}
	
	curr->flags &= ~BLK_INUSE;
}


void *near_malloc(size_t size)
{
	nearhdr_t *p, *nx;

	/* round up size to be word aligned
	 */
	size = (size + sizeof(uint16_t) - 1) & ~(sizeof(uint16_t) - 1);

	for (p = NHDR_FIRST(); p; p = p->next) {
		if (NHDR_GET_USED(p)) {
			continue;
		}
		if (NHDR_GET_SIZE(p) >= size) {
			break;
		}
	}

	if (p) {
		size_t left = NHDR_GET_SIZE(p) - size;
		if (left > 2 * sizeof(nearhdr_t)) {

			/* split the block */
			uint8_t *pb = (uint8_t*)p;
			pb += sizeof(nearhdr_t) + size;
			nx = (nearhdr_t*)pb;

			NHDR_SET_SIZE(nx, left - sizeof(nearhdr_t));
			NHDR_SET_FREE(nx);

			NHDR_SET_SIZE(p, size);
			NHDR_SET_USED(p);
			p->next = nx;
		}

		NHDR_SET_USED(p);
		return p+1;
	}

	return NULL;
}

void near_free(void *ptr)
{
	nearhdr_t *fp = ((nearhdr_t*)ptr) - 1;
	nearhdr_t *pp, *cp;

	pp = NULL;
	for (cp = NHDR_FIRST(); cp; pp = cp, cp = cp->next) {
		if (cp == fp) {
			break;
		}
	}

	if (cp == NULL) {
		panic("near_free: pointer is not an alloc'ed block");
	}

	if (pp != NULL && !NHDR_GET_USED(pp)) {
		/* previous is free, we can merge it */
		NHDR_SET_SIZE(
			pp,
			NHDR_GET_SIZE(pp) + sizeof(nearhdr_t) + NHDR_GET_SIZE(cp)
		);
		pp->next = cp->next;
		cp = pp;
	}

	if (cp->next != NULL && !NHDR_GET_USED(cp->next)) {
		/* next is free, we can merge it */
		NHDR_SET_SIZE(
			cp,
			NHDR_GET_SIZE(cp) + sizeof(nearhdr_t) + NHDR_GET_SIZE(cp->next)
		);
		cp->next = cp->next->next;
	}
	
	NHDR_SET_FREE(cp);
}

void near_dump(void)
{
	nearhdr_t *cp;

	for (cp = NHDR_FIRST(); cp; cp = cp->next) {
		kprintf("hdr %p inuse %d size %5d\n",
			cp,
			NHDR_GET_USED(cp),
			NHDR_GET_SIZE(cp)
		);
	}
}
