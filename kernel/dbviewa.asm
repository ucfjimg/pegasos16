        .model small
        .386

        include task.inc

        .data
        extrn   _curtask : word
        extrn   _db_state : word
        
db_running equ  0
db_stopped equ  1
db_stepping equ 2

STACK_SIZE equ  512                     ; in 16-bit words

        public  intstk
        public  kbdstk

        dw      (STACK_SIZE-1) dup(0)
intstk  dw      0

        dw      (STACK_SIZE-1) dup(0)
kbdstk  dw      0

        .code
        extrn   _db_intr_c : near
        extrn   swtosi : near

        ;
        ; Common interrupt handler for int 1 (single step) and
        ; int 3 (breakpoint)
        ;
        public  _db_intr
_db_intr proc far
        ; save registers in taskreg_t format
        pusha
        push    ds
        push    es

        ; set up addressability
        mov     ax, dgroup
        mov     ds, ax
        
        ; ensure we didn't get an interrupt in a state we 
        ; shouldn't have (i.e. already stopped in debugger)
        cmp     _db_state, db_stopped
        jne     stateok

        ; bad state, ignore interrupt
        pop     es
        pop     ds
        popa
        iret

stateok:
        ; save current ss:sp in current task as a normal task
        ; switch would
        mov     si, _curtask
        mov     word ptr ((task ptr [si]).stack), sp
        mov     word ptr ((task ptr [si]).stack)+2, ss

        ; set up debugger's stack (ax is still dgroup)
        mov     ss, ax
        mov     sp, offset intstk

        ; call C code to do the rest of then handler
        call    _db_intr_c

        ; the state is saved as a normal task switch, so use
        ; the scheduler to get going again
done:      
        mov     si, _curtask
        jmp     swtosi
_db_intr endp

KDATA   equ     60h                     ; keyboard data port
KCMD    equ     64h                     ; keyboard command port
KREADY  equ     01h                     ; scan code ready bit
OCW1    equ     20h                     ; PIC command port to send EOI
EOI     equ     20h                     ; end-of-interrupt command

        .data

        ;
        ; a circular queue for scancodes
        ;
SCSIZE  equ     20h                     ; size of scan code queue in bytes
SCMASK  equ     (SCSIZE - 1)            ; mod mask

scbuf   db      SCSIZE dup(0)           ; scan code queue
schead  dw      0                       ; head and tail pointers
sctail  dw      0

        .code
        ;       
        ; debugger keyboard interrupt. we don't use the system one because 
        ; we don't want to disturb state.
        ;
_db_kbdintr proc far
        ; save registers
        pusha
        push    ds
        
        ; set up addressability
        mov     ax, dgroup
        mov     ds, ax

        ; send end of interrupt to signal more interrupts ok when
        ; we sti eventually
        mov     al, EOI
        out     OCW1, al

        ; set up registers to work with the queue
        mov     si, offset scbuf        ; base of queue
        mov     cx, schead              ; head and tail pointers
        mov     bx, sctail

        ; try to read one scan code and exit if it isn't there
stroke: in      al, KCMD
        test    al, KREADY
        jz      done
        in      al, KDATA
        
        ; compute tail + 1 in the circular queue
        mov     di, bx
        inc     di
        and     di, SCMASK

        ; if the queue is full, we have to drop it
        cmp     di, cx
        je      stroke

        ; otherwise, save it and increment the tail
        mov     [bx+si], al     
        mov     bx, di
        jmp     stroke

done:
        ; save modified tail pointer
        mov     sctail, bx

        ; restore registers and return
        pop     ds
        popa
        iret
_db_kbdintr endp

        ;
        ; wait for a scan code to be available and return it
        ;
        public  _db_get_scancode
_db_get_scancode proc near
        ; protect scancode queue with cli
        pushf
        cli

        ; if head == tail, the queue is empty
check:  mov     bx, schead
        cmp     bx, sctail
        jne     gotsc

        ; block waiting for a keystroke. the hlt will wait for an interrupt
        ; and the interrupt routine will return to the following instruction
        sti
        hlt
        cli
        jmp     check

        ; the queue isn't empty, so return the next scancode.
        ; interrupts are still disabled so it's safe to manipulate
        ; the queue. bx is still the head.
gotsc:  xor     ax, ax
        mov     al, [scbuf+bx]
        inc     bx
        and     bx, SCMASK
        mov     schead, bx

        popf
        ret
_db_get_scancode endp
























;;;;;;;;;;;;;;;;;; OLD ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; kbdhooked db    0
; okbdint dd      0
; kbdvec  equ     4 * 9
;         ;
;         ; the debugger is NOT reentrant via int 3! It only has
;         ; one stack.
;         ;
; CANARY  equ     055aah
; int3mrk dw      CANARY
;         dw      255 dup(0)
;         public  int3stk
; int3stk dw      0

; kbdmrk  dw      CANARY
;         dw      255 dup(0)
; ;kbdstk  dw      0

; dbmrk   dw      CANARY
;         dw      253 dup(0)
; dbsp    dw      ?
; dbss    dw      ?

; chkstk  macro
;         push    bx
;         mov     bx, int3mrk
;         cmp     bx, CANARY
;         je      stk3ok
;         jmp     _stack_panic
; stk3ok:
;         mov     bx, kbdmrk
;         cmp     bx, CANARY
;         je      stkkok
;         jmp     _stack_panic
; stkkok:
;         mov     bx, dbmrk
;         cmp     bx, CANARY
;         je      stkdok
;         jmp     _stack_panic
; stkdok:
;         pop     bx
;         endm

;         .code

;         extrn   _db_view_scancode : near
;         extrn   _db_enter_c : near
;         extrn   _db_leave_c : near
;         extrn   swtosi : near

;         ;
;         ; hook the keyboard
;         ;
;         public  _db_hook_kbd
; _db_hook_kbd proc near
;         cmp     kbdhooked, 0
;         jne     hooked
;         xor     bx, bx
;         mov     es, bx
;         mov     bx, es:[kbdvec]
;         mov     word ptr [okbdint], bx
;         mov     bx, es:[kbdvec+2]
;         mov     word ptr [okbdint+2], bx
;         mov     es:[kbdvec], offset _db_kbdintr
;         mov     es:[kbdvec+2], cs
;         inc     kbdhooked
; hooked:
;         ret
; _db_hook_kbd endp

;         ;
;         ; unhook the keyboard
;         ;
;         public  _db_unhook_kbd
; _db_unhook_kbd proc near
;         cmp     kbdhooked, 0
;         je      unhooked
;         xor     bx, bx
;         mov     es, bx
;         mov     bx, word ptr [okbdint]
;         mov     es:[kbdvec], bx
;         mov     bx, word ptr [okbdint+2]
;         mov     es:[kbdvec+2], bx
;         dec     kbdhooked
; unhooked:
;         ret
; _db_unhook_kbd endp

;         ;
;         ; enter the debugger 
;         ;      
;         public  _db_enter
; _db_enter proc  near
;         ;
;         ; we want this to look like an iret frame
;         ;
;         ; near ret addr                 ; bp+20
;         push    cs                      ; bp+18
;         pushf                           ; bp+16
;         pusha                           ; bp+0, 8 registers
;         mov     bp, sp
;         mov     ax, [bp+16]             ; flags
;         xchg    ax, [bp+20]             ; flags at top
;         mov     [bp+16], ax             ; ret offset where flags were
        
;         ; finish setting up taskregs_t frame
;         push    ds
;         push    es
        
;         ; set up segments
;         mov     ax, dgroup
;         mov     ds, ax
        
;         ; prevent reentrancy
;         cmp     _db_state, db_stopped
;         jne     exit
        
;         ; save stack in curtask (which will be the console interrupt task)
;         cli
;         mov     si, _curtask
;         mov     word ptr ((task ptr [si]).stack), sp
;         mov     word ptr ((task ptr [si]).stack)+2, ss

;         ; set up debugger stack
;         mov     ss, ax
;         mov     sp, offset dbsp

;         ; enter the debugger
;         xor     ax, ax                  ; from breakpoint flag
;         push    ax
;         call    _db_enter_c
;         pop     ax

;         ; wait for keyboard interrupts to drive debugger 
;         ; commands
;         sti
; stall:  hlt
;         jmp     stall

; exit:   pop     es
;         pop     ds
;         popa
;         iret
;         ret
; _db_enter endp

;         public  _db_leave
; _db_leave proc  near
;         ;
;         ; called from debugger so DS and stack are set up
;         ;
;         ; prevent reentrancy
;         cmp     _in_db_view, 0
;         jz      exit

;         ; clean up debugger
;         call    _db_leave_c
        
;         ; go back to whatever the system was doing
;         cli
;         mov     si, _curtask
;         lss     sp, (task ptr [si]).stack

;         pop     es
;         pop     ds
;         popa
;         iret
; exit:   ret
; _db_leave endp

; _db_step_go proc
;         cli
;         mov     si, _curtask
;         lss     sp, (task ptr [si]).stack

;         pop     es
;         pop     ds
;         popa
;         iret
; _db_step_go endp


;         ;
;         ; enter the debugger on an int 3 breakpoint
;         ;
;         public  _db_int3
; _db_int3 proc near
;         ; save state (taskreg style frame)
;         pusha
;         push    ds
;         push    es

;         ; set up segs 
;         mov     ax, dgroup
;         mov     ds, ax

;         ; check for stack overflow
;         chkstk

;         ; check for attempt to reenter the debugger
;         cmp     _in_db_view, 0
;         jnz     exit

;         ; save frame pointer 
;         mov     si, _curtask
;         mov     word ptr ((task ptr [si]).stack), sp
;         mov     word ptr ((task ptr [si]).stack)+2, ss

;         ; set up new stack
;         mov     ss, ax
;         lea     sp, int3stk+2

;         ; enter the debugger
;         mov     ax, 1                   ; from breakpoint flag
;         push    ax
;         call    _db_enter_c
;         pop     ax

;         ; we never return from here. the 'go' command in the debugger
;         ; will use the scheduler to resume curtask
;         sti
; h:      hlt
;         jmp     h

; exit:   pop     es
;         pop     ds
;         popa
;         iret
; _db_int3 endp

;         ;
;         ; enter the debugger on an int 3 breakpoint
;         ;
;         public  _db_int1
; _db_int1 proc near
;         ; save state
;         pusha
;         push    ds
;         push    es

;         ; set up segs and save current stack
;         mov     ax, dgroup
;         mov     ds, ax

;         ; save current stack
;         mov     si, _curtask
;         mov     word ptr ((task ptr [si]).stack), sp
;         mov     word ptr ((task ptr [si]).stack)+2, ss

;         chkstk

;         ; set up new stack
;         mov     ss, ax
;         lea     sp, int3stk+2

;         mov     ax, 1
;         push    ax
;         call    _db_enter_c
;         pop     ax

;         ; we never return from here. the 'go' command in the debugger
;         ; will use the scheduler to resume curtask
;         sti
; h:      hlt
;         jmp     h
; _db_int1 endp



;         .data
; skov    db      'stack overflow', 0

;         .code
; _stack_panic proc near
;         cli
;         mov     ax, 0b800h
;         mov     es, ax
;         xor     di, di
;         cld
;         mov     cx, 2000
;         mov     ax, 1f20h
;         rep     stosw

;         mov     ax, dgroup
;         mov     ds, ax
;         mov     si, offset skov
;         xor     di, di
;         mov     ah, 1fh
; msg:    lodsb
;         or      al, al
; stop:   jz      stop
;         stosw
;         jmp     msg
; _stack_panic endp

        end