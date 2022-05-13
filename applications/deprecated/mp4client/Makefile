include ../../config.mak

vpath %.c $(SRC_PATH)/applications/mp4client

CFLAGS=-I"$(SRC_PATH)/include" $(OPTFLAGS)

LINKFLAGS=$(GPAC_SH_FLAGS)

ifeq ($(DEBUGBUILD),yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD),yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

ifeq ($(GPACREADONLY),yes)
CFLAGS+=-DGPAC_READ_ONLY
endif

ifeq ($(CONFIG_WIN32),yes)
LINKFLAGS+=$(UNICODEFLAGS)
EXE=.exe
PROG=MP4Client$(EXE)

ifeq ($(HAS_WMAIN),no)
CFLAGS+=-DNO_WMAIN
endif

else
EXT=
PROG=MP4Client
endif

ifeq ($(STATICBUILD),yes)
##include static modules and other deps for libgpac
include ../../static.mak
LINKFLAGS+=$(shell pkg-config ../../gpac.pc --libs --static | sed 's/-lgpac //'  )
LINKFLAGS+=$(GPAC_SH_FLAGS) $(EXTRALIBS)
else
LINKFLAGS+=-lgpac
ifeq ($(CONFIG_DARWIN),yes)
#LINKFLAGS+= -Wl,-rpath,'@loader_path'
else
LINKFLAGS+= -Wl,-rpath,'$$ORIGIN' -Wl,-rpath-link,../../bin/gcc
endif
endif

#common obj
OBJS= main.o
ifeq ($(CONFIG_DARWIN),yes)
OBJS+= carbon_events.o
LDFLAGS += -framework Carbon
endif


SRCS := $(OBJS:.o=.c)

ifeq ($(CONFIG_WIN32),yes)
OBJS+=$(SRC_PATH)/manifest.o
endif

all: $(PROG)

MP4Client$(EXE): $(OBJS)
	$(CC) -o ../../bin/gcc/$@ $(OBJS) -L../../bin/gcc $(LINKFLAGS) $(LDFLAGS)

clean:
	rm -f $(OBJS) ../../bin/gcc/$(PROG)

install: clean
	install -m 755 $(INSTFLAGS) ../../bin/gcc/MP4Client "$(DESTDIR)$(prefix)/bin"

uninstall:
	rm -rf $(DESTDIR)$(prefix)/bin/MP4Client

dep: depend

depend:
	rm -f .depend
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

distclean: clean
	rm -f Makefile.bak .depend

-include .depend
