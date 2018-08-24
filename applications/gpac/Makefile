include ../../config.mak

vpath %.c $(SRC_PATH)/applications/gpac

CFLAGS= $(OPTFLAGS) -I"$(SRC_PATH)/include"

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

LINKFLAGS=-L../../bin/gcc
ifeq ($(CONFIG_WIN32),yes)
EXE=.exe
PROG=gpac$(EXE)
else
EXT=
PROG=gpac
endif

ifeq ($(STATICBUILD),yes)
##include static modules and other deps for libgpac
include ../../static.mak

LINKFLAGS+=-lgpac_static
LINKFLAGS+= $(GPAC_SH_FLAGS)
LINKFLAGS+=$(EXTRALIBS)
else
LINKFLAGS+=-lgpac 
ifeq ($(CONFIG_DARWIN),yes)
#LINKFLAGS+= -Wl,-rpath,'@loader_path'
else
LINKFLAGS+= -Wl,-rpath,'$$ORIGIN' -Wl,-rpath-link,../../bin/gcc
endif
endif


#common objs - insert after ../static if any to overwrite list of objects
OBJS= main.o

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
