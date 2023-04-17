include ../../config.mak

vpath %.c $(SRC_PATH)/applications/gpac

CFLAGS=-I"$(SRC_PATH)/include" $(OPTFLAGS)

ifeq ($(DEBUGBUILD),yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD),yes)
ifeq ($(CONFIG_DARWIN),yes)
CFLAGS+=-fprofile-instr-generate
LDFLAGS+=-fprofile-instr-generate
else
CFLAGS+=-pg
LDFLAGS+=-pg
endif
endif

LINKFLAGS=-L../../bin/gcc
ifeq ($(CONFIG_WIN32),yes)
EXE=.exe
PROG=gpac$(EXE)
LINKFLAGS+=$(UNICODEFLAGS)
else ifeq ($(CONFIG_EMSCRIPTEN),yes)
EXT="."$(EM_TYPE)
PROG="gpac"$(EXT)

#export defaul symbols and modularize as libgpac
exports=-sMODULARIZE=1 -sEXPORT_NAME=libgpac -sEXPORTED_RUNTIME_METHODS=callMain,cwrap,stackSave,stackRestore,stackAlloc,addFunction,FS,IDBFS,allocateUTF8OnStack,getValue,setValue,UTF8ToString
ifeq ($(DISABLE_THREADS),no)
LINKFLAGS+=$(exports),PThread -sPTHREAD_POOL_SIZE=1 -sPTHREAD_POOL_SIZE_STRICT=0
else
LINKFLAGS+=$(exports)
endif
#default demo uses IDBFS
LINKFLAGS+=-lidbfs.js
else
EXT=
PROG=gpac
endif

ifeq ($(STATIC_BUILD),yes)
##include static modules and other deps for libgpac
include ../../static.mak
ifneq ($(PKG_CONFIG), no)
LINKFLAGS+=$(shell $(PKG_CONFIG) ../../gpac.pc --libs --static | sed 's/-lgpac //'  )
else
LINKFLAGS+= -lgpac_static
endif
LINKFLAGS+= $(GPAC_SH_FLAGS)
LINKFLAGS+=$(EXTRALIBS)
else
LINKFLAGS+=-lgpac
ifeq ($(CONFIG_EMSCRIPTEN),yes)
else ifeq ($(CONFIG_DARWIN),yes)
#LINKFLAGS+= -Wl,-rpath,'@loader_path'
else
LINKFLAGS+= -Wl,-rpath,'$$ORIGIN' -Wl,-rpath-link,../../bin/gcc
endif
endif


#common objs - insert after ../static if any to overwrite list of objects
OBJS=gpac.o compositor_tools.o gpac_help.o
ifeq ($(CONFIG_EMSCRIPTEN),yes)
OBJS+= ../mp4box/mp4box.o ../mp4box/filedump.o ../mp4box/fileimport.o
endif


SRCS := $(OBJS:.o=.c)

ifeq ($(CONFIG_DARWIN),yes)
OBJS+= carbon_hook.o
LDFLAGS += -framework Carbon
endif

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
