#ifndef EXEC_H_
#define EXEC_H_

#include "ktypes.h"

typedef struct exehdr_t exehdr_t;

/* MSDOS-style executable header
 */
struct exehdr_t {
	uint16_t sig;		/* MZ signature */
	uint16_t extra; 	/* bytes on last page */
	uint16_t pages; 	/* number of whole/partial pages (512 bytes) */
	uint16_t nreloc; 	/* # relocations */
	uint16_t hdrpara; 	/* # paragraphs in header */
	uint16_t minaloc; 	/* required paragraphs */
	uint16_t maxaloc;	/* max paragraphs */
	uint16_t ssinit;	/* SS:SP relocatable */
	uint16_t spinit;
	uint16_t chksum;	/* checksum */
	uint16_t ipinit; 	/* CS:IP relocatable */
	uint16_t csinit;
	uint16_t reloc; 	/* offset of reloc table */
	uint16_t ovly;		/* overlay data */
	uint16_t ovdata;
};

/* TODO arguments would be nice */
extern long exec(char __far *fn);


#define MZ_PAGE_SIZE 512

#endif


