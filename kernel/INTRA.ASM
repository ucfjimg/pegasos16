		.model small
		.code
		.386

		include task.inc

		.data


intrhead struc
hndlers	dw		?
task	dw		?
intrhead ends

		extrn _intrs: intrhead

		.code

		extrn _intr_handler : near
		extrn _task_sch_async : near
        extrn iswitch : near


INTRFLAG equ	0200h

		public _intr_enabled
_intr_enabled proc near
		pushf
		pop		ax
		and 	ax, INTRFLAG
		ret
_intr_enabled endp
	
		public _intr_cli
_intr_cli proc near
		pushf
		pop		ax
		and		ax, INTRFLAG
		cli
		ret
_intr_cli endp

		public _intr_sti
_intr_sti proc near
		pushf
		pop		ax
		and		ax, INTRFLAG
		sti
		ret
_intr_sti endp

		public _intr_restore
_intr_restore proc near
		push	bp
		mov		bp, sp
		mov		ax, [bp+4]
		test	ax, INTRFLAG
		jz		clear
		sti
		jnz		done
clear:	cli
done:	pop		bp
		ret
_intr_restore endp

istub	macro	intr
		public	_istub_&intr
_istub_&intr:
		pusha
		push	ds
		push	es
		mov		bx, intr
		jmp		_intr_go
		endm

		;
		; Generate the entry stubs for the interrupts
		;
i = 0
repeat 16
		istub %i
		i = i + 1
endm

        .data

pstub   macro   intr
        dw      _istub_&intr
endm

        public _ihandlers
_ihandlers label word

i = 0
repeat 16
        pstub   %i
        i = i + 1
endm

        .code

        public _intr_get_stub
_intr_get_stub proc near
        push    bp
        mov     bx, [bp+4]
        cmp     bx, 16
        jae     range

        shl     bx, 1
        mov     ax, _ihandlers[bx]
        mov     dx, cs
        jmp     done

range:  xor     ax, ax 
        mov     dx, ax

done:
        pop     bp
        ret
_intr_get_stub endp
    
		.data
		dw		128 dup(?)
tstack	label	word

		.code
_intr_go:
		mov		ax, dgroup
		mov		ds, ax
		mov		cx, ss
		mov		dx, sp

		mov 	bp, sp
		mov		si, [bp+24] ;flags

		;
		; NB it is ONLY safe to use this stack because interrupts are off
		; for the entire time we're using it! 
		; save off original SS:SP which -> task frame
		mov		ss, ax
		mov		sp, offset tstack
		push	cx
		push	dx

		mov		ax, bx				; save intr #
		shl		bx, 2				; NB sizeof(intrhead) == 4
		mov		dx, (intrhead ptr _intrs[bx]).task
		test	dx, dx
		jz		nointr

		
;		extern void task_sch_async(task_t *tsk, tasync_t tn, int argw, ...);

		push	ax					; arg is intr #
		push	1					; argw
		push	offset _intr_handler
		push	dx					; task
		call	_task_sch_async
		add		sp, 8

		; restore original interrupt stack frame
		mov		bp, sp
		lss		sp, [bp]

		;
		; at this point we have a task frame on the stack of whatever we
		; interrupted (curtask). we want to give up the current time slice
		; but only if there's something higher priority to run
		; 
        jmp     iswitch

nointr:	
		;
		; this is not a happy place. we've gotten an interrupt that
		; nobody expected. check to see if it's spurious and if not,
		; EOI it.
		;
		; TODO this is busted! We don't take care of moving off tstack!!!		

		pop		es
		pop		ds
		popa
		iret
		end
