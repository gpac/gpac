include ../../config.mak

vpath %.c $(SRC_PATH)/applications/mp4client

CFLAGS= $(OPTFLAGS) -I"$(SRC_PATH)/include" -I../../

LINKLIBS=$(GPAC_SH_FLAGS)

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

ifeq ($(GPACREADONLY), yes)
CFLAGS+=-DGPAC_READ_ONLY
endif

#common obj
OBJS= main.o extract.o

ifeq ($(CONFIG_WIN32),yes)
EXE=.exe
PROG=MP4Client$(EXE)
else
EXT=
PROG=MP4Client
endif

SRCS := $(OBJS:.o=.c) 

all: $(PROG)

MP4Client$(EXE): $(OBJS)
	$(CC) $(LDFLAGS) -o ../../bin/gcc/$@ $(OBJS) -L../../bin/gcc -lgpac $(LINKLIBS)

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
