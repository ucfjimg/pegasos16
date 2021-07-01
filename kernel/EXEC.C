#include "exec.h"

#include "fileio.h"
#include "memory.h"
#include "printk.h"
#include "process.h"
#include "task.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>

#define READ_SIZE 16384
#define RELOC_BUF 32

#define NEAR_HEAP 4096

#ifndef MIN
# define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

static uint8_t __huge *read_image(int fd, exehdr_t *hdr, int *err, uint16_t *palloc);
static int relocate(int fd, exehdr_t *hdr, uint8_t __huge *data);
static process_t *make_process(exehdr_t *hdr, uint8_t __huge *data, uint16_t palloc);
static int dup_handles(process_t *proc);
 
#pragma warning( disable : 4704 ) /* inline assembly */
long exec(char __far *fn)
{
	int fd;
	int err;
	exehdr_t hdr;
	int i;
	uint8_t __huge *data;
	uint16_t palloc;
	process_t *proc;
	size_t len;
	char *npath;

    len = _fstrlen(fn);

    npath = near_malloc(len + 1);
    if (npath == NULL) {
        kprintf("exec(): no mem\n");
        return -ENOMEM;
    }
    _fstrcpy(npath, fn);
    npath[len] = '\0';

	fd = kopen(npath, O_RDONLY);
    near_free(npath);
	
    if (fd < 0) {
		kprintf("exec(): failed to open file\n");
		return fd;
	}

	i = (int)kread(fd, &hdr, sizeof(hdr));
	if (i < 0) {
		kprintf("exec(): failed to read header\n");
		kclose(fd);
		return i;
	}

	/* palloc is the total number of parargraps allocated, including for
	 * the near heap.
	 */
	data = read_image(fd, &hdr, &err, &palloc);
	if (data == NULL) {
		kprintf("exec(): failed to read image\n");
		kclose(fd);
		return err;
	}

	err = relocate(fd, &hdr, data);
	if (err < 0) {
		kprintf("exec(): failed to relocate image\n");
		kfree(data);
		kclose(fd);
		return err;
	}

	kclose(fd);

	proc = make_process(&hdr, data, palloc);
	if (proc == NULL) {
		kprintf("exec(): failed to make process\n");
		/* NB make_process will free data on failure */
		return -ENOMEM;
	}

	err = dup_handles(proc);
	if (err != 0) {
		kprintf("exec(): failed to dup handles\n");
		proc_free(proc);
		return err;
	}
	
	kprintf("loaded %ls proc %p task %p pid %p\n", fn, proc, proc->task, proc->pid);
	return proc->pid;
}

/* Allocate and read in the executable image.
 */
uint8_t __huge *read_image(int fd, exehdr_t *hdr, int *err, uint16_t *palloc)
{
	uint8_t __huge *data;
	off_t off;
	uint16_t seg;
	uint32_t sofar;
	uint32_t size;
	uint32_t alloc;
	uint32_t hdrsize;

	*err = 0;

	size = MZ_PAGE_SIZE * hdr->pages;
	if (hdr->extra) {
		size = size - MZ_PAGE_SIZE + hdr->extra;
	}

	hdrsize = hdr->hdrpara << PARASHIFT;
	size -= hdrsize;
	off = hdrsize;

	kseek(fd, off, SEEK_SET);

	/* this will be aligned to paragraphs as NEAR_HEAP must be */
	alloc = 
		((size + PARASIZE - 1) & ~(PARASIZE - 1)) + 
		(hdr->minaloc << PARASHIFT) + 
		NEAR_HEAP;
	data = kmalloc(alloc);
	*palloc = (uint16_t)(alloc >> PARASHIFT);

	if (data == NULL) {
		kprintf("exec(): no memory for image\n");
		*err = -ENOMEM;
		return NULL;
	}

	/* NB kmalloc returns para-aligned pointers, so this
	 * is safe
	 */
	seg = KSEG_OF(data) + (KOFFS_OF(data) >> PARASHIFT);
	data = MKFP(seg, 0);

	sofar = 0;
	while (size) {
		size_t toread = (size_t)MIN(READ_SIZE, size);
		size_t got = kread(fd, data + sofar, toread);
		if (got != toread) {
			/* error reading exe! */
			*err = -EIO;
			kfree(data);
			return NULL;
		}

		sofar += toread;
		size -= toread;
	}

	return data;
}

/* read and apply relocations to the loaded image
 */
int relocate(int fd, exehdr_t *hdr, uint8_t __huge *data)
{
	uint16_t seg = KSEG_OF(data);

	uint16_t __far *reloc;
	unsigned i;

	/* relocation buffer  
	 */
	reloc = kmalloc(RELOC_BUF * 2 * sizeof(uint16_t));
	if (reloc == NULL) {
		return -ENOMEM;
	}

	kseek(fd, hdr->reloc, SEEK_SET);
	while (hdr->nreloc) {
		uint16_t nreloc = MIN(RELOC_BUF, hdr->nreloc);
		size_t sz = nreloc * 2 * sizeof(uint16_t);
		size_t got = kread(fd, reloc, sz);
		if (got != sz) {
			kfree(reloc);
			return -EIO;
		}

		for (i = 0; i < nreloc; i++) {
			uint16_t offs = reloc[2*i];
			uint32_t lin = reloc[2*i+1] << PARASHIFT;
			lin += offs;

			/* NB using __huge pointer here makes the compiler do the
			 * work of >64k offsets into the image by adjusting the
			 * segment.
			 */
			*(uint16_t __huge *)(data + lin) += seg;
		}

		hdr->nreloc -= nreloc;
	}

	kfree(reloc);
	return 0;
}


process_t *make_process(exehdr_t *hdr, uint8_t __huge *data, uint16_t palloc)
{
	process_t *proc = proc_alloc();
	uint16_t seg = KSEG_OF(data);

	if (proc == NULL) {
		kprintf("make_process: proc_alloc() failed\n");
		kfree(data);
		return proc;
	}

	proc->image = data;

	proc->task = task_from_proc(
		proc,
		hdr->csinit + seg,
		hdr->ipinit,
		hdr->ssinit + seg,
		hdr->spinit,
		palloc
	);

	if (proc->task == NULL) {
		kprintf("make_process: task_from_proc() failed\n");
		proc_free(proc);
		return NULL;
	}

	proc->parent = get_curproc();

	return proc;
}

int dup_handles(process_t *proc)
{
	int i;
	process_t *parent = proc->parent;
	
	if (parent == NULL) {
		return 0;
	}

	for (i = 0; i < PROC_FILES; i++) {
		if (parent->fds[i].kfd != -1) {
			int newfd = kdup(parent->fds[i].kfd);
			if (newfd < 0) {
				return newfd;
			}
			proc->fds[i].kfd = newfd;
		}
	}

	return 0;
}
