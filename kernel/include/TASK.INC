;
; a task
;
task	struct
next	dw	?	; next and prev
prev	dw	?	; (tasks are doubly linked)
owner   dw  ?   ; owning task queue
process	dw	?	; if tied to a process, the process
stack	dd	?	; ptr to stack for resume
kstkbse dd	?	; if kernel task, the base
kstksz  dw  ?   ; if kernel task, size of stack
state	db	?	; current state
pri	db	?	; priority
task	ends

TS_FREE     equ 0
TS_RUN		equ	1
TS_READY 	equ	2
TS_BLOCKED	equ	3
TS_FREEING  equ 4

TASK_PRI_IRQ0	equ 0
TASK_PRI_IRQ15	equ 15
TASK_PRI_BOOST  equ 16
TASK_PRI_NORM   equ 17
TASK_PRI_NICE   equ 18
TASK_PRI_IDLE   equ 19
TASK_NPRI		equ 20

ptask	typedef ptr task
				

