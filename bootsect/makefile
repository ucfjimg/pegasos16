ROOT=..
!INCLUDE $(ROOT)\master.mak

OUT=$(BLDBIN)\bootsect.com
$(OUT): bootsect.asm
	ml /nologo /Fe$@ /Fl $?
	
clean:
	if exist $(OUT) del $(OUT)
 
