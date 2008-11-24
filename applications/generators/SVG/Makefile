include ../../../config.mak

vpath %.c $(SRC_PATH)/applications/generators/SVG

CFLAGS= $(OPTFLAGS) -I$(SRC_PATH)/include

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

#common obj
OBJS= html.o laser.o main.o v1.o v2.o v3.o ../../../src/utils/list.o ../../../src/utils/error.o

CFLAGS+=-g
LDFLAGS+=-g
CFLAGS+=$(XML2_CFLAGS)

ifeq ($(CONFIG_WIN32),yes)
EXE=.exe
PROG=SVGGen$(EXE)
EXTRALIBS+=-lwsock32 -lz
else
EXT=
PROG=SVGGen
endif

SRCS := $(OBJS:.o=.c) 

all: $(PROG)

SVGGen$(EXE): $(OBJS) 
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(XML2_LFLAGS) $(EXTRALIBS)


%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $< 


clean: 
	rm -f $(OBJS) $(PROG)

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
