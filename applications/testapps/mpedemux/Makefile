include ../../../config.mak

vpath %.c $(SRC_PATH)/applications/test_apps/mpedemux

CFLAGS= $(OPTFLAGS) -I"$(SRC_PATH)/include"

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
OBJS= main.o

LINKFLAGS=-L../../../bin/gcc
ifeq ($(CONFIG_WIN32),yes)
EXE=.exe
PROG=mpedemux$(EXE)
LINKFLAGS+=-lgpac_static -lz $(EXTRALIBS)
#LINKFLAGS+=-lgpac
else
EXT=
PROG=mpedemux
LINKFLAGS+=-lgpac_static $(EXTRALIBS) $(GPAC_SH_FLAGS) -lz
#LINKFLAGS+=-lgpac -lz
endif


SRCS := $(OBJS:.o=.c) 

all: LIBGPAC $(PROG)

LIBGPAC: 
	$(MAKE) -C ../../../src

$(PROG): $(OBJS)
	$(CC) $(LDFLAGS) -o ../../../bin/gcc/$@ $(OBJS) $(LINKFLAGS)

clean: 
	rm -f $(OBJS) ../../../bin/gcc/$(PROG)

dep: depend

depend:
	rm -f .depend	
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

distclean: clean
	rm -f Makefile.bak .depend

-include .depend
