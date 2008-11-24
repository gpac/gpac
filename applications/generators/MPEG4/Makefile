include ../../../config.mak

vpath %.c $(SRC_PATH)/applications/generators/MPEG4

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
OBJS= main.o ../../../src/utils/list.o ../../../src/utils/error.o

ifeq ($(CONFIG_WIN32),yes)
EXE=.exe
PROG=MPEG4Gen$(EXE)
else
OBJS+=../../../src/utils/os_divers.o
EXT=
PROG=MPEG4Gen
endif

SRCS := $(OBJS:.o=.c) 

all: $(PROG)

$(PROG): $(OBJS) 
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(EXTRALIBS) 


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
