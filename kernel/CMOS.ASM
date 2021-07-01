        .model small
        .code

CMOS_REG    equ     70h
CMOS_DATA   equ     71h

        public _cmos_read
        ;
        ; uint8_t cmos_read(uint8_t reg)
        ;
_cmos_read proc
        push    bp
        mov     bp, sp
        pushf
        cli
        
        mov     dx, CMOS_REG
        mov     al, byte ptr [bp+4] ; register
        out     dx, al
        rept 8
          nop
        endm
        inc     dx
        xor     ax, ax
        in      al, dx

        popf
        pop     bp
        ret
_cmos_read endp

        end
