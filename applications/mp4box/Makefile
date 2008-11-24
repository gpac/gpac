include ../../config.mak

vpath %.c $(SRC_PATH)/applications/mp4box

CFLAGS= $(OPTFLAGS) -I$(SRC_PATH)/include

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

#file format is read-only
ifeq ($(GPACREADONLY), yes)
CFLAGS+= -DGPAC_READ_ONLY
endif

ifeq ($(DISABLE_SVG), yes)
CFLAGS+=-DGPAC_DISABLE_SVG
endif

#common obj
OBJS= main.o filedump.o fileimport.o

LINKFLAGS=-L../../bin/gcc
ifeq ($(CONFIG_WIN32),yes)
EXE=.exe
PROG=MP4Box$(EXE)
#LINKFLAGS+=-lgpac_static -lz $(EXTRALIBS)
LINKFLAGS+=-lgpac
else
EXT=
PROG=MP4Box
#LINKFLAGS+=-lgpac_static $(EXTRALIBS) $(GPAC_SH_FLAGS) -lz 
LINKFLAGS+=-lgpac -lz  $(OGL_LIBS)
endif


SRCS := $(OBJS:.o=.c) 

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(LDFLAGS) -o ../../bin/gcc/$@ $(OBJS) $(LINKFLAGS)


%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $< 


clean: 
	rm -f $(OBJS) ../../bin/gcc/$(PROG)

dep: depend

depend:
	rm -f .depend	
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

distclean: clean
	rm -f Makefile.bak .depend



# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
