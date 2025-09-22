include ../../config.mak

vpath %.c $(SRC_PATH)/applications/udptsseg

CFLAGS= $(OPTFLAGS) -I"$(SRC_PATH)/include"

ifeq ($(DEBUGBUILD),yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD),yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

#common obj
OBJS= main.o

LINKFLAGS=-L../../bin/gcc -L../../extra_lib/lib/gcc

ifeq ($(CONFIG_WIN32),yes)
EXE=.exe
PROG=udptsseg$(EXE)
ifeq ($(STATIC_BINARY),yes)
LINKFLAGS+=-lgpac_static -lz $(EXTRALIBS)
else
LINKFLAGS+=-lgpac
endif
else
EXT=
PROG=udptsseg
ifeq ($(STATIC_BINARY),yes)
LINKFLAGS+=-lgpac_static -lz $(EXTRALIBS)
else
LINKFLAGS+=-lgpac
endif
endif


SRCS := $(OBJS:.o=.c) 

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) -o ../../bin/gcc/$@ $(OBJS) $(LINKFLAGS) $(LDFLAGS)

clean: 
	rm -f $(OBJS) ../../bin/gcc/$(PROG)

dep: depend

depend:
	rm -f .depend	
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

distclean: clean
	rm -f Makefile.bak .depend

-include .depend
