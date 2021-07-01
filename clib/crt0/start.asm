;
; this is the entry point into the program
;
	.model small
	.dosseg
	.code
	.386	

	.data
	extrn __edata : byte
	extrn __end : byte

	.code

	public __acrtused 
	__acrtused = 9876h

	extrn _main : near
	extrn _exit : near
	extrn __init_stdio : near

;
; A block in the near heap
;
hbhdr	struc
next	dw		?
bytes	dw		?
hbhdr	ends

entry:	mov	dx, dgroup
	mov	ds, dx
	mov si, ax			; total # of para allocated

	;
	; zero out BSS
	;
	mov	es, dx
	lea	di, __edata
	lea	cx, __end
	sub	cx, di
	jcxz	nobss
	xor	ax, ax
	cld
	rep	stosb
nobss:	

	;
	; Adjust SS:SP so SS=DS (small model, stack is in dgroup)
	;
	mov	bx, ss		; entry stack
	sub	bx, dx	        ; minus ds
	shl	bx, 4 		; para -> bytes
	mov	ax, sp		; current sp
	add	ax, bx		; adjusted to match SS=DS 
	cli
	mov	ss, dx		; set new stack
	mov	sp, ax		; ...
	sti

	;
	; set up near heap
	;
	mov		bx, cs
	mov		ax, ds
	sub		ax, bx			; # of para of code
	mov		bx, sp			; SP is top of stack
	add		bx, 15
	shr		bx, 4			; bx = # para of data
	add		ax, bx			; ax = # para starting at cs:0 to heap
	sub		si, ax			; subtract off to get heap size in para

	;
	; make sure the number of heap para fits at end of data segment
	;
	mov		ax, bx			; # para in data+bss+stack
	add		ax, si			; + # para in heap
	jnc		fits			; branch if it fits
	mov		si, 4096		; para in 64k segment			
	sub		si, bx			; minus para of data and stack
fits:
	cmp		si, 1			; do we even have a heap? (more than one para)
	jbe		noheap			; nope, DS is totally full

	;
	; SP is the first byte unused by the stack and is word aligned
	; SI is the number of para in the heap
	;
	mov		di, sp			; -> heap header
	shl		si, 4			; bytes in heap
	sub		si, sizeof hbhdr

	mov		(hbhdr ptr [di]).next, di
	mov		(hbhdr ptr [di]).bytes, si	; NB low bit is zero, meaning 'free'
	mov		__heap, di

noheap:
	call	__init_stdio

	call	_main	        ; start program
	push	ax				; return code
	call	_exit			; never returns
n02:
	jmp	n02

	.data
	public	__heap
__heap	dw	0				; pointer to near heap

	.stack
		
	end entry
