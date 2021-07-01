	.model small
	.code
	.386

SYSC	equ	60h
EXIT	equ	0

	extrn __close_stdio : near

	public	_exit
_exit	proc	near
	push	bp
	mov		bp, sp
	call	__close_stdio
	mov		cx, [bp+4]
	mov		ax, EXIT
	int		SYSC
	pop		bp
	ret
_exit	endp

	end

