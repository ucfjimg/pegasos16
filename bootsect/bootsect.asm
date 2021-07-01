;
; Boot sector for floppies (FAT12)
;
        .model  tiny
        .code

        org     100h

;
; Adjust for the fixed boot sector load address of 0000:7c00
;
LOAD =  07c00h
BASE =  LOAD - 0100h


entry:
        jmp     start
        nop
;
; space for BPB
;
; NB these are default values for a 1.44M floppy
;
sysid   db      8 dup(20h)
bysec   dw      512                     ; bytes per sector
secclus db      1                       ; sectors per cluster
reserv  dw      1                       ; reserved sectors
nfat    db      2                       ; # of fats
rootent dw      224                     ; # root directory entries
totsec  dw      2880                    ; total sectors
meddesc db      0f0h                    ; media descriptor
secfat  dw      9                       ; sectors per fat
sectrk  dw      18                      ; sectors per track
heads   dw      2                       ; # heads
hidden  dd      0                       ; hidden sectors
lrgnsec dd      0                       ; 32-bit sec count if >64k
        db      0                       ; drive number
        db      0                       ; low bit == dirty
        db      029h                    ; EBPB flag
        dd      012345678h              ; VOLSER
        db      "NO NAME    "           ; volume label
        db      "FAT12   "              ; fs type    

;
; local variables
;
drive   db      ?                       ; drive #
seccyl  dw      ?                       ; sectors per cyl
rootsec dw      ?                       ; first sec of root

start:
;
; set up stack and segments
;
        cli
        mov     ax, cs
        mov     ss, ax
        lea     sp, entry + BASE
        mov     ds, ax        
        mov     es, ax
        sti

;
; compute/save some things
;
    
        mov     [drive + BASE], dl      ; save drive #
        mov     ax, [sectrk + BASE]
        mul     [heads + BASE]
        mov     [seccyl + BASE], ax     ; sectors per cylinder
    
;
; reset the disk system
;
        xor     ax, ax
        int     13h                     ; we should have drive in dl
        jc      fail
    
;
; find root directory
;
        xor     bx, bx
        mov     ax, [secfat + BASE]
        mov     bl, [nfat + BASE]
        mul     bx
        add     ax, word ptr [hidden + BASE]
        add     ax, [reserv + BASE]
        mov     [rootsec + BASE], ax    

        mov     bx, [rootent + BASE]
        mov     cl, 4                   ; TODO this assumes 512 byte secs
        shr     bx, cl
        mov     cx, bx                  ; # sectors of root dir
dirsec: push    cx        

        mov     di, 07e00h
        call    readsec
    
        mov     cx, 0010h               ; entries per sector

;
; check one entry for kernel filename
;    
dnext:  push    cx
        push    di
        mov     cx, namelen
        mov     si, offset kernel + base
        cld
        repe    cmpsb
        je      foundk    
        pop     di
        pop     cx
        add     di, 0020h               ; dir entry size
        loop    dnext    

        pop     cx
        inc     ax                      ; next dir sector
        loop    dirsec
        jmp     fail                    ; ran out of directory

foundk: add    sp, 6                    ; clean all stuff off stack
        mov    si, di
        add    si, 1Ah-0Bh              ; advance to cluster
        lodsw                           ; cluster
        test   ax, ax                   ; empty -> fail
        jz     fail
        mov    dx, ax                   ; cluster
        lodsw                           ; size
        mov    bx, ax
        lodsw                           ; size upper word
        test   ax, ax                   ; can't be >64k
        jnz    fail

;
; now we have size in bytes in BX and starting cluster in DX
;    
        mov     si, [bysec + BASE]
        mov     ax, dx
        sub     ax, 2                   ; datasec starts at cluster 2
        xor     dx, dx
        mov     dl, [secclus + BASE]
        mul     dx                      ; ax in sectors, dx = 0
        add     ax, [rootsec + BASE]    ; to start of root dir
        mov     dx, [rootent + BASE]    ; figure out
        mov     cl, 4
        shr     dx, cl                  ; # root dir sectors
        add     ax, dx                  ; AX -> first sector
        xchg    ax, bx                  ; AX = bytes, BX = first sec
        add     ax, si                  ; += bytes per sector
        dec     ax
        xor     dx, dx
        div     si                      ; AX = # sectors in file

        mov     cl, 4
        shr     si, cl                  ; SI = paragraphs per sector
        mov     cx, ax                  ; # sectors
        mov     ax, bx                  ; starting sector
;
; do the read. note that 0000:7E00 (where we want to read)
; is 07E0:0000
;
        mov     bx, 07E0h               ; starting segment
        xor     di, di                  ; offset 
kread:  mov     es, bx                  
        push    cx                      ; save count
        push    bx
        push    ax
        call    readsec
        pop     ax
        pop     bx
        pop     cx
        inc     ax                      ; sector++
        add     bx, si                  ; seg += para per sector
        loop    kread
;
; make COM file addressability for kernel
;
        xor     dh, dh
        mov     dl, [drive + BASE]    
        mov     ax, cs    
        add     ax, 07e00h shr 4
        xor     bx, bx
        mov     ds, ax
        mov     es, ax
        cli
        mov     ss, ax
        mov     sp, 0FFFEh
        sti
        push    ax
        push    bx
        retf    
    
;
; print the message in ds:si, wait for a keypress, and then
; attempt to reboot
;
fail:
        mov     si, offset nosys + BASE 
print:  lodsb
        or      al, al                  ; done?
        jz      done
        mov     ah, 0Eh                 ; print
        mov     bx, 7                   ; page 0, grey
        int     10h
        jmp     print

done:
        xor    ah, ah
        int    16h                      ; wait for keypress
        int    19h                      ; try to boot again
        jmp    done                     ; if int 19h failed


;
; Read the sector at LBA AX into the address at ES:DI
;  
;
readsec:    
        xor     dx, dx
        div     [seccyl + BASE]         ; AX=cyl, DX=remainder
        xchg    ax, dx                  ; DX=cyl
        div     byte ptr [sectrk+BASE]  ; AL=head, AH=sector
        inc     ah                      ; sector is 1-based
        mov     bl, dh
        mov     cl, 6
        shl     bl, cl
        or      bl, ah
        mov     bh, dl
        mov     cx, bx                  ; CL=cyl, CH=sec
        mov     dh, al                  ; DH=head
        mov     dl, [drive + BASE]      ; DL=drive
        mov     ax, 0201h               ; read one sector
        mov     bx, di                  ; ES:BX -> buffer
    
        int     13h
        jc      fail
        
        ret


nosys:
        db      0Dh, 0Ah
        db      "Non-system disk. Please insert bootable disk and press any key."
        db      0Dh, 0Ah, 0


kernel  db      'KERNEL     '
namelen =       11

        org     02feh
;
; boot signature
;
        db      055h, 0aah

        end     entry
