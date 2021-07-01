	.model small
	.code
	.386

SYSC	equ	    60h
OPEN    equ	    1
CLOSE   equ     2
READ    equ     3
WRITE   equ     4
EXEC	equ		5
SC_WAIT	equ		6
SC_DUP	equ		7
SC_SLEEP equ	8

MAXERR  equ     -32

    .data
    public  _errno
_errno dw   0

    .code

;
; common exit code. ax contains the return code. if it's
; in the error range, set errno and translate to -1; else
; do nothing to it
;
_errcheck   proc    near
    mov     [_errno], 0
    cmp     ax, MAXERR
    jb      ok
    neg     ax
    mov     [_errno], ax
    mov     ax, -1
ok: ret
_errcheck   endp

	public	_open
_open	proc	near
	push	bp
	mov	    bp, sp
	mov	    cx, [bp+4]
    mov     dx, [bp+6]
    mov     bx, [bp+8]
	mov	    ax, OPEN
	int	    SYSC
    call    _errcheck
	pop	    bp
	ret
_open	endp

	public	_close
_close	proc	near
	push	bp
	mov	    bp, sp
	mov	    cx, [bp+4]
	mov	    ax, CLOSE
	int	    SYSC
    call    _errcheck
	pop	    bp
	ret
_close	endp

	public	_read
_read	proc	near
	push	bp
	mov	    bp, sp
    push    si
	mov	    cx, [bp+4]
    mov     dx, [bp+6]
    mov     bx, [bp+8]
    mov     si, [bp+10]
	mov	    ax, READ
	int	    SYSC
    call    _errcheck
    pop     si
	pop	    bp
	ret
_read	endp

	public	_write
_write	proc	near
	push	bp
	mov	    bp, sp
    push    si
	mov	    cx, [bp+4]
    mov     dx, [bp+6]
    mov     bx, [bp+8]
    mov     si, [bp+10]
	mov	    ax, WRITE
	int	    SYSC
    call    _errcheck
    pop     si
	pop	    bp
	ret
_write	endp

	public	_exec
_exec	proc	near
	push	bp
	mov	    bp, sp
    push    si
	mov	    cx, [bp+4]
    mov     dx, [bp+6]
	mov	    ax, EXEC
	int	    SYSC
    call    _errcheck
    pop     si
	pop	    bp
	ret
_exec	endp

	public	_wait
_wait	proc	near
	push	bp
	mov	    bp, sp
    push    si
	mov	    cx, [bp+4]
    mov     dx, [bp+6]
	mov	    ax, SC_WAIT
	int	    SYSC
    call    _errcheck
    pop     si
	pop	    bp
	ret
_wait	endp

	public	_sleep
_sleep	proc	near
	push	bp
	mov	    bp, sp
    push    si
	mov	    cx, [bp+4]
    mov     dx, [bp+6]
	mov	    ax, SC_SLEEP
	int	    SYSC
    call    _errcheck
    pop     si
	pop	    bp
	ret
_sleep	endp

	public	_dup
_dup	proc	near
	push	bp
	mov	    bp, sp
    push    si
	mov	    cx, [bp+4]
	mov	    ax, SC_DUP
	int	    SYSC
    call    _errcheck
    pop     si
	pop	    bp
	ret
_dup	endp
	end
