	.model	small
	.code
	.386
	
	.data

regs	db	'DISIBPSPBXDXCXAXCSDSESSSFLRX'
count	dw	0

	.code
	public	_bluescreen
_bluescreen proc
	pushf
	cli
	push	ss
	push	es
	push	ds
	push	cs
	pusha
	mov	bp, sp

	mov	ax, dgroup
	mov	ds, ax

	mov	ax, 0B800h
	mov	es, ax
	xor	di, di
	mov	cx, 80 * 25
	cld
	mov	ax, 01F20h
	rep	stosw

	mov	di, 160 + 20
	mov	cx, 14		; registers
	lea	si, regs
		
again:	mov	ax, [bp]
	add	bp, 2 
	
	movsb
	inc	di
	movsb
	add	di, 3
	
		

	mov	dx, cx
	mov	cx, 4
	add	di, 8
hexit:	dec	di
	dec	di
	mov	bx, ax
	and	bx, 000fh
	or	bx, 0030h
	cmp	bx, 003ah
	jb	putit
	add	bx, 7
putit:	mov	byte ptr es:[di], bl
	ror	ax, 4 	
	loop	hexit
	mov	cx, dx
	add	di, 160-6
	loop	again
	
	cli
	hlt
_bluescreen endp

	end	
