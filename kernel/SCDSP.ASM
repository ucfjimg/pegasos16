	.model small
	.code
	.386

	include task.inc

SC_EXIT equ 0


freestk	struc
next	dw	?
top	dw	?	
freestk ends

sysc	struc
fn	dw	?
argw	dw	?
sysc	ends

dispfrm	struc
;
; pusha
;
r_di	dw	?
r_si	dw	?
r_bp	dw	?
r_sp	dw	?
r_bx	dw	?
r_dx	dw	?
r_cx	dw	?
r_ax	dw	?
;
; segs
;
r_ds	dw	?
r_es	dw	?
;
; iret frame
;
r_ip	dw	?
r_cs	dw	?
r_fl	dw	?
dispfrm ends

;
; MUST MATCH SYSCALL.C!!!
;
KSTACK_SIZE equ	512

	.data

	extrn _nsyscall: word
	extrn _syscalls: ptr sysc


kstk	db	'kstk ', 0

	;
	; queue of tasks waiting for a kernel stack
	;
ksqhead	dw	0
ksqtail	dw	0
	
	;
	; A stack to use for exit
	;
	dw	64 dup(0)
exitstk equ .

	extrn	_freestk : word



	.code

	extrn _sc_disp : near
	extrn _kprintf : near
	extrn _bluescreen : near
	extrn _task_exit : near

SYSVEC	equ	60h

	public _sc_hook
_sc_hook proc	near
	push	es
	xor	ax, ax
	mov	es, ax

	mov	es:[SYSVEC*4], offset disp
	mov	es:[SYSVEC*4+2], cs	

	pop	es
	ret
_sc_hook endp	
	
disp 	proc
	push	es
	push	ds
	pusha

	mov	ax, dgroup
	mov	ds, ax
	mov	es, ax

notread:
	cli

	;
	; get a free kernel stack
	;
	mov	si, _freestk
	or si, si
	jnz gotstk

	;
	; TODO block task here if we're out
	;

	mov ax, 0b800h
	mov es, ax
	mov word ptr es:[158], 04F21h
	cli
	hlt

gotstk:
	mov	bx, (freestk ptr [si]).next
	mov	_freestk, bx
	;
	; get top of stack and store 
	; current stack pointer 
	;
	mov	si, (freestk ptr [si]).top
	sub	si, 4
	mov	word ptr [si], sp
	mov	word ptr [si+2], ss
	mov	ss, ax		; still has DS
	mov	sp, si
	sti
	
	;
	; now set up on kernel stack
	;
	call	_sc_disp

	;
	; ax is now the syscall #
	;

	;
	; at this point ss:sp points to the saved ss:sp of the
	; process, which is 4 down from the top of the stack
	;
	pop		dx		; return sp
	pop		cx		; return ss

	;
	; sp is now the top of the kernel stack to free
	;
	mov		si, sp
	sub		si, KSTACK_SIZE

	cli
	mov		bx, _freestk
	mov		(freestk ptr [si]).top, sp
	mov		(freestk ptr [si]).next, bx
	mov		_freestk, si

	mov		ss, cx
	mov		sp, dx
	cmp		ax, SC_EXIT
	jne		notexit
	call	_task_exit

notexit:
	sti

done:	
	popa
	pop	ds
	pop	es
	iret
disp	endp

;
; print character in al to console
;
debch 	proc	near
	push	ax
	push	bx
	mov		ah, 0eh
	mov		bx, 0007h
	int		10h
	cmp		al, 0ah
	jnz		done
	mov		al, 0dh
	int		10h
done:
	pop		bx
	pop		ax
	ret
debch	endp

;
; print near string in si to console
;
debstr	proc	near
	push	ax
	push	bx
	push	ds
	mov		ax, dgroup
	mov		ds, ax
	mov		ah, 0eh
	mov		bx, 0007h
next:
	lodsb
	or		al, al
	jnz		done
	int		10h
	cmp		al, 0Ah
	jne		next
	mov		al, 0Dh
	int		10h
	jmp		next

done:
	pop		ds
	pop		bx
	pop		ax
	ret
debstr	endp

;
; print ax in hex to the console
;
debhex proc		near
	push	cx
	push	bx
	mov		bx, 0007h
	mov		cx, 4
next:
	rol		ax, 4
	push	ax
	and		ax, 000Fh
	or		ax, 0e30h
	cmp		al, 039h
	jb		nothex
	add		al, 'A' - '9'
nothex:
	int		10h
	pop		ax
	loop	next

	pop		bx
	pop		cx
	ret
debhex endp




	end

