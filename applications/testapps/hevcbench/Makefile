include ../../../config.mak

vpath %.c $(SRC_PATH)/applications/testapps/hevcbench

CFLAGS= $(OPTFLAGS) -I"$(SRC_PATH)/include" $(SDL_CFLAGS) $(OGL_INCLS) $(OHEVC_CFLAGS)

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

#common obj
OBJS= main.o

LINKFLAGS=-L../../../bin/gcc
ifeq ($(CONFIG_WIN32),yes)
EXE=.exe
PROG=hevcbench$(EXE)
else
EXT=
PROG=hevcbench
endif
LINKFLAGS+=-lgpac $(SDL_LIBS) $(OGL_LIBS) $(OHEVC_LDFLAGS)


SRCS := $(OBJS:.o=.c) 

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) -o ../../../bin/gcc/$@ $(OBJS) $(LINKFLAGS) $(LDFLAGS)

clean: 
	rm -f $(OBJS) ../../../bin/gcc/$(PROG)

dep: depend

depend:
	rm -f .depend	
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

distclean: clean
	rm -f Makefile.bak .depend

-include .depend
