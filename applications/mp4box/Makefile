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

#common obj
OBJS=main.o filedump.o fileimport.o 
ifeq ($(DISABLE_STREAMING),no)
OBJS+=live.o
endif

LINKFLAGS=-L../../bin/gcc -L../../extra_lib/lib/gcc

ifeq ($(CONFIG_WIN32),yes)
EXE=.exe
PROG=MP4Box$(EXE)
ifeq ($(MP4BOX_STATIC),yes)
ifneq ($(CONFIG_ZLIB),no)
LINKFLAGS+=-lz
endif
LINKFLAGS+=-lgpac_static $(EXTRALIBS)
else
LINKFLAGS+=-lgpac
endif

else

EXT=
PROG=MP4Box
ifeq ($(MP4BOX_STATIC),yes)
ifneq ($(CONFIG_ZLIB),no)
LINKFLAGS+=-lz
endif
LINKFLAGS+=-lgpac_static $(EXTRALIBS) $(GPAC_SH_FLAGS)

# spidermonkey support
ifeq ($(CONFIG_JS),no)
else
SCENEGRAPH_CFLAGS+=$(JS_FLAGS)
ifeq ($(CONFIG_JS),local)
NEED_LOCAL_LIB="yes"
endif
LINKFLAGS+=$(JS_LIBS)
endif

else
LINKFLAGS+=-lgpac
endif

endif


SRCS := $(OBJS:.o=.c) 

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(LDFLAGS) -o ../../bin/gcc/$@ $(OBJS) $(LINKFLAGS)

clean: 
	rm -f $(OBJS) ../../bin/gcc/$(PROG)

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
