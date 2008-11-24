include ../../../config.mak

vpath %.c $(SRC_PATH)/applications/testapps/loadcompare

CFLAGS= $(OPTFLAGS) -I$(SRC_PATH)/include

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

CFLAGS+=$(XML2_CFLAGS)

#common obj
OBJS= loadcompare.o ../../../modules/svg_loader/svg_parser.o ../../../modules/svg_loader/lsr_parser.o  

LINKFLAGS=-L../../../bin/gcc
ifeq ($(CONFIG_WIN32),yes)
EXE=.exe
PROG=LoadCompare$(EXE)
#LINKFLAGS+=-lgpac_static -lz $(EXTRALIBS)
LINKFLAGS+=-lgpac
else
EXT=
PROG=LoadCompare
#LINKFLAGS+=-lgpac_static $(EXTRALIBS) $(GPAC_SH_FLAGS) -lz
LINKFLAGS+=-lgpac -lz $(XML2_LFLAGS)
endif


SRCS := $(OBJS:.o=.c) 

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(LDFLAGS) -o ../../../bin/gcc/$@ $(OBJS) $(LINKFLAGS)


%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $< 


clean: 
	rm -f $(OBJS) ../../../bin/gcc/$(PROG)

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
