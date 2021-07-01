;
;  int13h wrappers
;

		.model small, c 
		.code

		;
		; extern int bios_reset_drv(int drive);
		;

bios_reset_drv	proc c, drive: byte
		push	dx

		mov	ah, 00h	    	; reset
		mov	dl, [drive] 	; drive # (0x80 means HDD as well as FDD)
		int	13h
		jc	fail

		xor	ax, ax

exit:		pop	dx
		ret

fail:		xor	ax, ax
		dec	ax
		jmp	exit
bios_reset_drv	endp

drvgeom 	struc
heads 		dw	?		; # heads
cyls		dw	?		; # cylinders
secs		db	?		; # sectors per track
drives  	db  	?       	; # of physical drives
drvgeom 	ends

		;
		; extern int bios_drvgeom(int drive, drvgeom *geom);
		;
bios_drvgeom	proc c, drive: byte, geom: ptr drvgeom
		push	es
		push	di
		push	bx
		push	cx
		push	dx
		
		xor	ax, ax
		mov	es, ax
		mov	di, ax

		mov	dl, [bp+4]		; drive number
		mov	ah, 08h			; read drive parameters
		int	13h
		jc      fail
		
		les	di, [bp+6]		; geom ptr
		
		mov     es:[di+drvgeom.drives], dl

		mov	al, dh	        ; heads - 1, increment in word
		xor	ah, ah
		inc	ax
		mov	es:[di+drvgeom.heads], ax

		mov	ax, cx			; low 6 bits: # sectors
		and	ax, 003fh
		mov	es:[di+drvgeom.secs], al

		mov 	ax, cx			; CH = low 8 bits cyls-1, CL high 2 bits is high 2 bits
		xchg	al, ah
		mov	cl, 6
		shr	ah, cl
		inc	ax
		mov	es:[di+drvgeom.cyls], ax
				
		xor	ax, ax
exit:
		pop	dx
		pop	cx
		pop	bx
		pop	di
		pop	es
		ret

fail:		xor 	ax, ax
		dec 	ax
		jmp 	exit
bios_drvgeom	endp
	   
		;
		; extern int bios_media_change(int drive);
		;
		; returns -1 on error 
		;          0 if the media has not changed
		;          1 if the media has changed
		;          2 if there is no media
		;
bios_media_change proc c, drive: byte
		push	dx
		
		mov	ah, 16h		; change of disk status
		mov	dl, [bp+4]	; drive number
		int	13h
		jnc	ok

		cmp	ah, 06h		; media removed
		jne	fail		; fail on any other error

		mov	ax, 2		; no media 
		jmp	exit

ok:		xchg	ah, al		; else just return BIOS code
		xor    	ah, ah		; in ax								
		
exit:
		pop    	dx
		ret

fail:		xor	ah, ah
		xor	ax, ax
		dec	ax
bios_media_change endp


		;
		; extern int bios_read_blk(int drive, int cyl, int head, int sec, uint8_t far *buffer);
		;

		public bios_read_blk
bios_read_blk	proc near
		mov	ax, 0201h	; read fn, 1 sector
		jmp	readwrite
bios_read_blk	endp

		; extern int bios_write_blk(int drive, int cyl, int head, int sec, uint8_t far *buffer);
		;
		public bios_write_blk
bios_write_blk	proc near
		mov	ax, 0301h	; write fn, 1 sector
		jmp	readwrite
bios_write_blk	endp

		;
		; common routine for BIOS read/write. parameters are identical between the two
		; functions bios_read_blk and bios_write_blk. ah contains the int 13h command
		; for the operation we want
		;
readwrite	proc c private, drive: byte, cyl: word, head: byte, sector: word, buf: far ptr byte
		push	bx
		push	cx
		push	dx
		push	es
		push	ax

		;
		; this is a little messy because of how >256 cylinder drives were
		; added after the first BIOS interfaces were defined, but before
		; the extended AH=$4X LBA-based interfaces existed.
		;
		; ES:BX		= buffer
		; AH   		= command (read/write)
		; AL    	= # sectors
		; CL[5:0]	= (1-based) sector number
		; CL[7:6]	= top two bits of 10-bit cylinder address
		; CH		= bottom 8 bits of 10-bit cylinder adress
		; DH        	= head
		; DL        	= drive #
		;
		mov	ax, [cyl]
		xchg	ah, al		; AH now has low 8 bits of cyl
		mov	cl, 6
		shr	al, cl		; top bits of AL are top 2 bits of cyl
		mov	bx, [sector]
		and	bx, 03fh
		or	al, bl		; bottom bits of AL are now sec
		mov	cx, ax		; and CX is properly set up
		pop	ax			; we can now restore command and sector count
		mov	dh, [head]
		mov	dl, [drive]
		les	bx, [buf]
		int	13h			; try to do operation
		jnc	success

		; oops. ah says what went wrong
		xchg	ah, al
		xor	ah, ah
		neg	ax
		jmp	exit

success:
		xor	ax, ax

exit:
		pop	es
		pop	dx
		pop	cx
		pop	bx
		ret
readwrite	endp


		end


