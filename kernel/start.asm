;
; this is the entry point into the kernel
;
	.model small
	.dosseg
	
	.data
	
	extrn	__edata : byte
	extrn	__end : byte

	.code

	public __acrtused 
	__acrtused = 9876h

	extrn _main : near

entry:	mov	dx, dgroup
	mov	ds, dx
		
	;
	; zero out BSS
	;
	mov	es, dx
	lea	di, __edata
	lea	cx, __end
	sub	cx, di
	xor	ax, ax
	cld
	rep	stosb
	
	;
	; Adjust SS:SP so SS=DS (small model, stack is in dgroup)
	;
	mov	bx, ss
	sub	bx, dx
	mov	cl, 4
	shl	bx, cl
	mov	ax, sp
	add	ax, bx
	cli
	mov	ss, dx
	mov	sp, ax
	sti
	
	call	_main
n02:
	jmp	n02

	.stack
		
	end entry
