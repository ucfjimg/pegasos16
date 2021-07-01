#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct hbhdr_t hbhdr_t;
struct hbhdr_t {
    hbhdr_t *next;
    size_t bytes;
};

extern hbhdr_t *_heap;

#define INUSE 0x0001            /* low bit of `bytes` */

void *malloc(size_t bytes)
{
    hbhdr_t *pb = _heap;
    hbhdr_t *blk = NULL;

    /* all allocations are word aligned */
    bytes = (bytes + 1) & ~1;

    do {
        if ((pb->bytes & INUSE) == 0 && pb->bytes >= bytes) {
            blk = pb;
            break;
        }
        pb = pb->next;
    } while (pb != _heap);

    if (blk) {
        unsigned char *pub = (unsigned char *)(blk + 1);

        /* if the block is big enough to be worth it, split it
         * and make a free block out of what's left
         */
        if (blk->bytes >= 2 * sizeof(hbhdr_t)) {
            pb = (hbhdr_t*)(pub + bytes);
            pb->next = blk->next;
            pb->bytes = blk->bytes - bytes - sizeof(hbhdr_t);
            blk->next = pb;
            blk->bytes = bytes;
        }

        blk->bytes |= INUSE;
        return pub;    
    }

    return NULL;
}

void *calloc(size_t n, size_t size)
{
    unsigned long sz32 = n * size;
    void *p;

    /* check for overflow - we're dealing with near malloc
     * so sizes have to be 16 bit
     */
    if (sz32 & 0xffff0000ul) {
        return NULL;
    }
    
    size = (size_t)(sz32 & 0xffff);

    p = malloc(size);
    if (p) {
        memset(p, 0, size);
    }

    return p;
}

void free(void *p)
{
    hbhdr_t *curr = ((hbhdr_t*)p) - 1;
    hbhdr_t *prev, *next;

    /* free(NULL) is defined as a no-op for convenience */
    if (p == NULL) {
        return;
    }

    /* here we both find the previous block and verify that p
     * really IS a block
     */
    prev = _heap;
    do {
        if (prev->next == curr) {
            break;
        }
        prev = prev->next;
    } while (prev != _heap);

    if (prev->next != curr || (curr->bytes & INUSE) == 0) {
        /* we didn't find curr as a block, or the block was 
         * already free. this should probably assert/abort TODO 
         */
        return;
    }

    curr->bytes &= ~INUSE;

    /* it's possible for curr to be BEFORE prev in memory, if
     * prev is the last block in the heap and curr is the first.
     * in that case, we can't merge.
     */
    if (prev < curr && (prev->bytes & INUSE) == 0) {
        /* safe to merge curr into prev
         */
        prev->next = curr->next;
        prev->bytes += sizeof(hbhdr_t) + curr->bytes;
        curr = prev;
    }

    /* can we merge curr (which might already include the previous
     * block) with the following block? 
     */
    next = curr->next;
    if (curr < next && (next->bytes & INUSE) == 0) {
        curr->next = next->next;
        curr->bytes += sizeof(hbhdr_t) + next->bytes;
    }
}

/* realloc
 * if p == NULL and bytes == 0, does nothing
 * if p == NULL and bytes > 0, same as malloc
 * if p != NULL and bytes == 0, same as free
 * if p != NULL and bytes != 0, attempts to change
 *    the size of p to the given new size w/o changing the contents
 * 
 * returns NULL on failure
 */
void *realloc(void *p, size_t bytes)
{
    hbhdr_t *curr, *next;
    size_t cursz = 0;

    if (bytes == 0) {
        free(p);
        return NULL;
    }

    if (p == NULL) {
        return malloc(bytes);
    }

    bytes = (bytes + 1) & ~1;
    if (bytes == cursz) {
        return p;
    }

    /* TODO in a debug version it would be nice to walk the heap
     * here and report if p is not actually a block
     */
    curr = ((hbhdr_t*)p) - 1;
    cursz = curr->bytes & ~INUSE;

    if (bytes < cursz) {
        /* if it's worth it, cut the block size down */
        if (cursz - bytes >= 2 * sizeof(hbhdr_t)) {
            next = (hbhdr_t*)((char*)p + bytes);
            next->next = curr->next;
            next->bytes = cursz - sizeof(hbhdr_t) - bytes;
            curr->next = next;
            curr->bytes = bytes | INUSE;
        }
        return p;
    }
    
    /* we have to enlarge the block. if there's a free block after,
     * we might be able to just expand.
     */
    next = curr->next;
    if (
        next > curr && 
        (next->bytes & INUSE) == 0 &&
        cursz + sizeof(hbhdr_t) + next->bytes >= bytes
    ) {
        size_t left = cursz + sizeof(hbhdr_t) + next->bytes - bytes;
        if (left > 2 * sizeof(hbhdr_t)) {
            next = (hbhdr_t*)((char*)p + bytes);
            next->next = curr->next;
            next->bytes = left - sizeof(hbhdr_t);
            curr->next = next;
            curr->bytes = bytes | INUSE;
        } else {
            /* there isn't enough left after appending to be worth
             * splitting, so just merge the two blocks
             */
            curr->next = next->next;
            curr->bytes = (cursz + sizeof(hbhdr_t) + next->bytes) | INUSE;
        }

        return p;
    }

    /* we can't expand the block in place. have to take the slow path
     * of allocating a whole new block and copying the data over.
     */
    p = malloc(bytes);
    if (p) {
        memcpy(p, curr + 1, curr->bytes & ~INUSE);
        free(curr + 1);
    }    

    return p;
}
