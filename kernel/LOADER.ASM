	.model tiny
	.code
	.386
	
	org	100h
	BASE = -100h

exehdr	struct  1
sig	dw	?	; MZ signature
extra	dw	?	; bytes on last page
pages	dw	?	; number of whole/partial pages (512 bytes)
nreloc	dw	?	; # relocations
hdrpara	dw	?	; # paragraphs in header
minaloc	dw	?	; required paragraphs
maxaloc	dw	?       ; max paragraphs
ssinit	dw	?       ; SS:SP relocatable
spinit	dw	?
chksum	dw	?	; checksum
ipinit	dw	?	; CS:IP relocatable
csinit	dw	?
reloc	dw	?	; offset of reloc table
ovly	dw	?	; overlay data
ovdata	dw	?       ; ...
exehdr	ends

	memK = 0413h
	para = 16
	parashift = 4
	kshift = 10
	pagesize = 512
	pageshift = 9

	;
	; this is the segment where the kernel executable
	; image will end up. the DMA pool starts at 50:0,
	; so tuning this number changes how much memory is
	; available there. 0200 gives about 6.75k.
	; 
	loadseg = 0200h

entry:
	; dl contains BIOS drive we're booting from	
	mov	cs:[drive + BASE], dx

	; AX = top segment of memory
	xor	ax, ax
	mov	es, ax
	mov	ax, word ptr es:[memK]
	shl	ax, kshift-parashift
	; BX = # para in loader
	; AX = where to relocate loader to
	mov	bx, (tail-entry+para-1) / para
	sub	ax, bx
	shl	bx, parashift-1
	mov	cx, bx
	; CX = # words to move to end of memory
	push	cs
	pop	ds
	mov	es, ax
	call	here
here:	pop 	si
	sub	si, here-entry
	xor	di, di
	cld
	rep	movsw
	lea	si, there + BASE
	push	es
	push	si
	retf	
	
there:	
        push	loadseg	; loader CS (where EXE will reloc to)
;
; Get the segment pointing to the exe file after us
; which is paragraph aligned
;
	mov	si, offset tail + BASE
	add	si, para-1
	shr	si, parashift
	mov	ax, ds
	add	ax, si
	mov	ds, ax

;
; check signature
;
       	xor	si, si
	mov	ax, (exehdr ptr [si]).sig
	cmp	ax, 05a4dh		; DOS EXE signature
	jne	fail

;
; set up to do relocations
;
; dx = seg of where code will live eventually
; bx = seg of where code lives now
; cx = # relocs
; ds:si points at reloc table
;
 ;	mov	dx, ds
  	pop	dx
	mov	bx, ds
	add	bx, (exehdr ptr [si]).hdrpara
	mov	cx, (exehdr ptr [si]).nreloc
	mov	si, (exehdr ptr [si]).reloc
	cld
fixup:
	lodsw
	mov	di, ax
	lodsw
	add	ax, bx
	mov	es, ax
	add	es:[di], dx
	loop	fixup

;
; set initial registers
;
	
	xor	si, si
	mov	ax, (exehdr ptr [si]).ssinit
	add	ax, dx
	cli
	mov	ss, ax
	mov	sp, (exehdr ptr[si]).spinit
	sti
	mov	ax, (exehdr ptr [si]).csinit
	add	ax, dx

;
; AX is relocated CS
; BX is seg of EXE past header (old)
; DX is seg of EXE past header (new)
; SI is zero
; 
;
; arguments to kernel entry
;	

	push	word ptr cs:[drive + BASE]
	push 	dx
	push	(exehdr ptr [si]).hdrpara
	push	(exehdr ptr [si]).extra
	push	(exehdr ptr [si]).pages
	push	(exehdr ptr [si]).minaloc

	push	ax
	push	(exehdr ptr [si]).ipinit

;
; Move code. DS:SI still points at exehdr
;
	mov	cx, (exehdr ptr [si]).pages
movek:	mov	es, dx
	mov	ds, bx
	xor	si, si
	xor	di, di
	push	cx
	mov	cx, pagesize / 2
	rep	movsw
	add	bx, pagesize / para
	add	dx, pagesize / para
	pop	cx
	loop	movek

	retf	


fail:	push	cs
	pop	ds
	lea	si, offset corrupt + BASE
fail0:	lodsb
	or	al, al
fail1:	jz	fail1
	mov	ah, 0eh
	mov	bx, 0007h
	int	10h
	jmp	fail0
corrupt	db	0dh, 0ah, "kernel image is corrupt", 0dh, 0ah, 0	
drive	dw	?
tail:
	len = (tail - entry) and 0fh
	padd = (16 - len) and 0fh
	db	padd dup(0)
	end entry

