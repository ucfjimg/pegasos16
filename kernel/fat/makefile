ROOT=..\..
KERNEL=1

!INCLUDE $(ROOT)\master.mak

OBJS=\
	$(OBJ)\cluster.obj \
	$(OBJ)\dir.obj \
	$(OBJ)\fileobj.obj \
	$(OBJ)\fileops.obj \
	$(OBJ)\path.obj

OUT=$(LIBPATH)\fat$(SFX).lib

all: \
	$(OBJ) \
	$(OUT)

$(OUT): $(OBJS)
	lib $@ @<<
-+$(?: = &^
-+);	
<<

$(OBJ)\cluster.obj:	cluster.c
$(OBJ)\dir.obj: 	dir.c
$(OBJ)\fileobj.obj: 	fileobj.c
$(OBJ)\fileops.obj: 	fileops.c
$(OBJ)\path.obj: 	path.c

$(OBJ):
	mkdir $(OBJ)

clean:
	if exist $(OUT) del $(OUT)
	for %%f in ($(OBJS)) do if exist %%f del %%f
	if exist $(BLDINC)\fat.h del $(BLDINC)\fat.h

