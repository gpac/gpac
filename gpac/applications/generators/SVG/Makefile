include ../../../config.mak

vpath %.c $(SRC_PATH)/applications/generators/SVG

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
OBJS= html.o laser.o main.o v1.o v2.o v3.o

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
	$(CC) -o $@ $(OBJS) $(XML2_LIBS) $(EXTRALIBS) -L../../../bin/gcc -L../../../extra_lib/lib/gcc -lgpac -lz $(LDFLAGS)


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

-include .depend
