	.model small
	.386

	include task.inc
	
;
; a semaphore
;
sema	struct
count	dw	?	; signed count
qhead	dw	?	; blocked queue head
qtail	dw	?	; blocked queue tail	
sema	ends

;
; old and new locations of register in task frame
; for transformation into iret friendly form
;

st_o_fl	equ	20
st_o_ip	equ	22
st_o_cs	equ	24

st_n_fl	equ	24
st_n_cs	equ	22
st_n_ip	equ	20


int8vec equ	8*4
	.data
tasklock dw	0

extrn _tickcnt : dword
extrn _curtask : ptask
extrn _tqueue : ptr ptask
extrn _tsk_freel : ptask

bios8	dd	0

	.code

	extrn	_kfree : near
	extrn	_near_free : near
    extrn   _task_dequeue : near
    extrn   _task_runnable : near
	extrn	_task_wake : near

	public _task_lock
_task_lock proc near
	pushf
	cli	
	inc	[tasklock]
	popf
	ret
_task_lock endp

	public _task_unlock
_task_unlock proc near
	pushf
	cli	
	dec	[tasklock]
	popf
	ret
_task_unlock endp

	.data

msg db 'task_exit %p', 10, 0

	.code

	extrn _kprintf : near

	public	_task_exit
_task_exit proc	near
	;
	; NB we can trash whatever regs we want, since
	; we're going to restore from another context
	; anyway
	;
	cli
	mov		ax, dgroup
	mov		ds, ax
	mov     bx, [_curtask]
	push    bx
	call    _task_dequeue	
	pop		bx	

	cmp		(task ptr [bx]).state, TS_FREEING
	jne		notexit

	;
	; if the task is going away, free it
	;
	mov		si, [_tsk_freel]
	mov		(task ptr [bx]).state, TS_FREE
	mov		(task ptr [bx]).next, si
	mov		(task ptr [bx]).prev, 0
	mov		_tsk_freel, bx
	mov		ax, word ptr (task ptr [bx]).kstkbse
	or		ax, ax
	jz		nokstack
	push	ax
	call	_near_free
	;
	; NB we are living on borrowed time here as we just free'd
	; the stack we're running on. however, interrupts are disabled
	; so nothing else can write to the memory, and by the time
	; they're re-enabled we're guaranteed to have reloaded the
	; stack pointer
	;
nokstack:	
	jmp		exit		

notexit:
	mov		(task ptr [bx]).state, TS_BLOCKED
    ;
    ; find something to run
    ;
exit:
   	call    _task_runnable
   	mov     si, ax
	jmp	    swtosi
_task_exit endp


	public  _task_switch
    public iswitch

_task_switch proc	far
	;
	; on entry CS:IP already on stack
	;
	pushf
	pusha
	push	ds
	push	es

	; rearrange frame so it's the same as from a
	; hardware interrupt
	;
	mov	    bp, sp	
	mov	    ax, [bp+st_o_cs]
	mov	    bx, [bp+st_o_ip]
	mov	    cx, [bp+st_o_fl]
	
	mov	    [bp+st_n_cs], ax	
	mov	    [bp+st_n_ip], bx	
	mov	    [bp+st_n_fl], cx	

	;
	; switch the task
	;
	cli

    ;
    ; interrupt handlers jmp here when done, with the stack of whatever
    ; was interrupted already set saved
    ;
iswitch::
	mov	    si, _curtask
	mov	    word ptr ((task ptr [si]).stack), sp
	mov	    word ptr ((task ptr [si]).stack)+2, ss
	mov	    (task ptr [si]).state, TS_READY

    ;
    ; bump curtask to the back of its priority bucket
    ;
    lea     di, _tqueue
    xor     bh, bh
    mov     bl, (task ptr [si]).pri
    shl     bx, 1
    mov     ax, (task ptr [si]).next
    mov     word ptr [di+bx], ax

    ;
    ; Find something to run
    ;
    call    _task_runnable
    mov     si, ax

    ;
    ; other functions jump to swtosi when they need to directly start
    ; a task in si w/o saving context of current state
    ;
	; this is public so the debugger can use it
	;
	public	swtosi
swtosi::	
	mov	    (task ptr [si]).state, TS_RUN
	mov	    _curtask, si
	lss	    sp, (task ptr[si]).stack	

	;
	; return to new task
	;
noswitch::
	pop	    es
	pop	    ds
	popa
	iret
_task_switch endp

;
; switch to the given task (or past it, if it's blocked)
;
; at this point, the caller (C code) is responsible for
; having unlinked curtask and putting it into a wait
; queue somewhere, so we aren't allowed to do anything
; but save state to it here.
;
; interrupts must be disabled on entry!
;
	public	_task_yield_to
_task_yield_to proc	near
	push	bp
	mov	    bp, sp
	;
	; save task frame
	;
	push	0200h   		; interrupt enable flags
	push	cs
	push	offset resume	; iret frame
	pusha
	push	ds
	push	es		        ; registers

	mov	    si, [_curtask]
	mov	    word ptr (task ptr [si]).stack, sp	
	mov	    word ptr (task ptr [si]).stack+2, ss
	
    mov	    si, [bp+4]	; next task 
	jmp	    swtosi		

resume:
	pop	    bp
	ret
_task_yield_to endp


	.data

spinner db '-\|/'
spindex dw 0

	.code

	public	_timer_handler
_timer_handler proc
	;
	; IRET frame pushed. Interrupts off
	;
	; save rest of task context
	;
	pusha
	push	ds
	push	es
	
	mov	    ax, dgroup
	mov	    ds, ax

	mov		bx, spindex
	mov		cl, spinner[bx]
	inc		bx
	and		bx, 3
	mov		spindex, bx 

	mov 	ax, 0b800h
	mov		es, ax
	mov		di, 4158
	std
	mov		ch, 0ch
	mov		ax, cx
	stosw

	mov		ah, 0ch
	mov		dx, 020h
	mov		al, 8+3	; read in service
	out		dx, al
	nop
	nop
	nop
	in		al, dx
	push	ax
	and		al, 15
	or		al, 30h
	stosw
	pop		ax
	shr		al, 4
	and		al, 15
	or		al, 30h
	stosw

	mov		al, 8+2	; read in request
	out		dx, al
	nop
	nop
	nop
	in		al, dx
	push	ax
	and		al, 15
	or		al, 30h
	stosw
	pop		ax
	shr		al, 4
	and		al, 15
	or		al, 30h
	stosw
	cld
	; 
	; increment ticks since boot and wake up any sleeping
	; tasks that come due
	;
	add	    word ptr _tickcnt, 1
	adc	    word ptr _tickcnt+2, 0
	call	_task_wake

	;
	; build iret frame for BIOS handler
	;
	; push	0		; flags; NOTE: interrupts off!
	; call	[bios8]

	;
	; if we don't call BIOS we have to EOI
	;
	mov		dx, 20h
	mov		al, 20h
	out		dx, al
	
	;
	; context switch tasks
	;	
doswtch:
	cmp	    [tasklock], 0
	jnz	    noswitch
	jmp	    iswitch
_timer_handler endp

	public	_task_hook_timer
_task_hook_timer proc near
	push	es
	cli
	xor	    ax, ax
	mov	    es, ax
	mov	    ax, es:[int8vec]
	mov	    word ptr [bios8], ax
	mov	    ax, es:[int8vec+2]
	mov	    word ptr [bios8+2], ax 

	mov	    word ptr es:[int8vec], offset _timer_handler
	mov	    word ptr es:[int8vec+2], cs
	sti
	pop	    es
	ret
_task_hook_timer endp

	public	_task_async_stub
_task_async_stub proc
	;
	; On entry here, the top of the stack has the near pointer to
	; the kernel callback, followed by the arguments to the callback
	;
	pop	bx

	push	offset _task_exit 	; return address
	push	bx			; async routine
	ret
_task_async_stub endp

;
; helper to get kernel's CS/DS from C
;
	public	_get_cs
_get_cs	proc	near
	mov	    ax, cs
	ret
_get_cs	endp

	public	_get_ds
_get_ds	proc	near
	mov	    ax, ds
	ret
_get_ds	endp

	public	_get_ss
_get_ss	proc	near
	mov	    ax, ss
	ret
_get_ss	endp


	end
