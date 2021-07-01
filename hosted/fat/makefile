ROOT=..\..

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

$(OBJ)\path.obj: 	path.c 		fat.h
$(OBJ)\cluster.obj: 	cluster.c 	fat.h
$(OBJ)\dir.obj: 	dir.c 		fat.h
$(OBJ)\fileobj.obj: 	fileobj.c 	fat.h
$(OBJ)\fileops.obj: 	fileops.c 	fat.h

$(OBJ):
	mkdir $(OBJ)

clean:
	if exist $(OUT) del $(OUT)
	for %%f in ($(OBJS)) do if exist %%f del %%f

