	.model small
	.code
	.386
	
	;
	; void outb(uint16_t port, uint8_t data);
	;
	public _outb
_outb	proc	near
	push	bp
	mov	bp, sp
	mov	dx, word ptr [bp+4]
	mov	al, byte ptr [bp+6]
	out	dx, al
	pop	bp
	ret
_outb	endp

	;
	; uint8_t inb(uint16_t port); 
	;
	public _inb
_inb	proc	near
	push	bp
	mov	bp, sp
	mov	dx, [bp+4]
	in	al, dx
	xor	ah, ah
	pop	bp
	ret
_inb	endp

	end

	


