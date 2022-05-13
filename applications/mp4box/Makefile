include ../../config.mak

vpath %.c $(SRC_PATH)/applications/mp4box

CFLAGS=-I"$(SRC_PATH)/include" $(OPTFLAGS)

ifeq ($(DEBUGBUILD),yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD),yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

LINKFLAGS=-L../../bin/gcc -L../../extra_lib/lib/gcc
ifeq ($(CONFIG_DARWIN),yes)
#LINKFLAGS+= -Wl,-rpath,'@loader_path'
else
LINKFLAGS+= -Wl,-rpath,'$$ORIGIN' -Wl,-rpath-link,../../bin/gcc
endif

ifeq ($(STATICBUILD),yes)
##include static modules and other deps for libgpac
include ../../static.mak
LINKFLAGS+=$(shell pkg-config ../../gpac.pc --libs --static | sed 's/-lgpac //'  )
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

ifeq ($(STATIC_BINARY),yes)
LINKFLAGS+= $(GPAC_SH_FLAGS)
endif

endif

#common obj
OBJS=mp4box.o filedump.o fileimport.o
ifeq ($(DISABLE_STREAMING),no)
OBJS+=live.o
endif



SRCS := $(OBJS:.o=.c)

ifeq ($(CONFIG_WIN32),yes)
OBJS+=$(SRC_PATH)/manifest.o
endif

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
