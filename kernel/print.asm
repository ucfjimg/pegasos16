	.model small, c

        .code

print   proc c, s:ptr sbyte

	push	si
	push	bx

	mov	si, s
pr00:	lodsb
	or	al, al
	jz	pr01
	mov	bx, 0007h
	mov	ah, 0eh
	int	10h
	jmp	pr00

pr01:	pop	bx
	pop	si

	ret
print	endp

;kputc   proc c, x: byte
;	push	bx
;	mov	bx, 000Ah	; page and color
;	mov	al, [x]
;	cmp	al, 10
;	jne	notlf
;	push	ax
;	mov	ax, 0e0dh	; if lf, print cr first
;	int 	10h
;	pop	ax
;notlf:	mov	ah, 0eh
;	int	10h
;	pop	bx
;	ret
;kputc	endp

        end
