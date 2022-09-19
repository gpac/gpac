include ../../config.mak

vpath %.c $(SRC_PATH)/applications/gpac

CFLAGS=-I"$(SRC_PATH)/include" $(OPTFLAGS)

ifeq ($(DEBUGBUILD),yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD),yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

LINKFLAGS=-L../../bin/gcc
ifeq ($(CONFIG_WIN32),yes)
EXE=.exe
PROG=gpac$(EXE)
LINKFLAGS+=$(UNICODEFLAGS)
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
ifeq ($(CONFIG_DARWIN),yes)
#LINKFLAGS+= -Wl,-rpath,'@loader_path'
else
LINKFLAGS+= -Wl,-rpath,'$$ORIGIN' -Wl,-rpath-link,../../bin/gcc
endif
endif


#common objs - insert after ../static if any to overwrite list of objects
OBJS=gpac.o compositor_tools.o gpac_help.o

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
