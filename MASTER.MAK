# suffix for debug directories
!IFDEF DEBUG
SFX=D
!ENDIF

# build directories
BLD=$(ROOT)\bld
BLDLIB=$(BLD)\lib$(SFX)
BLDBIN=$(BLD)\bin$(SFX)
BLDINC=$(BLD)\inc
!IFDEF KERNEL
OBJ=kobj$(SFX)
!ELSE
OBJ=obj$(SFX)
!ENDIF
GENSRC=$(BLD)\src$(SFX)

# kernel versus hosted include and defines
!IFDEF KERNEL

INC=/X /I$(BLD)\inc /I$(ROOT)\kernel\include /I$(ROOT)\kernel /I$(ROOT)\clib\include
ENV=/DKERNEL
LIBPATH=$(BLDLIB)\kernel

!ELSE IFDEF NATIVE

INC=/X /I$(ROOT)\clib\include /I$(ROOT)\kernel
ENV=/DNATIVE
LIBPATH=$(BLDLIB)\native

!ELSE

INC=/I$(BLD)\inc /I$(ROOT)\hosted /I$(ROOT)\kernel\include /I$(ROOT)
ENV=/DHOSTED
LIBPATH=$(BLDLIB)\hosted

!ENDIF

# compilation flags
CFLAGS=/nologo /W4 /WX /Fl
LFLAGS=/nologo
AFLAGS=/nologo

!IFNDEF WANT_DEFLIBS
CFLAGS=$(CFLAGS) /Zl
!ENDIF

!IFDEF KERNEL
CFLAGS=$(CFLAGS) /Gs
!ENDIF

CFLAGS=$(CFLAGS) /AS $(INC) $(ENV)
AFLAGS=$(AFLAGS) /Fl /Sa $(INC)
 
!IFDEF DEBUG
CFLAGS=$(CFLAGS) /Zi /Od
AFLAGS=$(AFLAGS) /Zi
LFLAGS=$(LFLAGS) /CODEVIEW
!ELSE
CFLAGS=$(CFLAGS) /O1
!ENDIF

# inference rules
{}.c{$(OBJ)}.obj:
	cl $(CFLAGS) /Fo$@ /c $<

{$(GENSRC)}.c{$(OBJ)}.obj:
	cl $(CFLAGS) /Fo$@ /c $<

{}.asm{$(OBJ)}.obj:
	ml $(AFLAGS) /Fo$@ /c $<
	
	
