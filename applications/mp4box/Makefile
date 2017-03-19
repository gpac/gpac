include ../../config.mak

vpath %.c $(SRC_PATH)/applications/mp4box

CFLAGS= $(OPTFLAGS) -DGPAC_HAVE_CONFIG_H -I"$(SRC_PATH)/include"

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

LINKFLAGS=-L../../bin/gcc -L../../extra_lib/lib/gcc

ifeq ($(STATICBUILD),yes)
##include static modules and other deps for libgpac
include ../../static.mak
LINKFLAGS+=-lgpac_static
LINKFLAGS+= $(GPAC_SH_FLAGS)
LINKFLAGS+=$(EXTRALIBS)
else
LINKFLAGS+=-lgpac
endif


ifeq ($(CONFIG_WIN32),yes)
EXE=.exe
PROG=MP4Box$(EXE)
LINKFLAGS+=$(UNICODEFLAGS)

ifeq ($(HAS_WMAIN),no)
CFLAGS+=-DNO_WMAIN
endif

else

EXT=
PROG=MP4Box

ifeq ($(MP4BOX_STATIC),yes)
LINKFLAGS+= $(GPAC_SH_FLAGS)
endif

endif


#common obj
OBJS=main.o filedump.o fileimport.o 
ifeq ($(DISABLE_STREAMING),no)
OBJS+=live.o
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
