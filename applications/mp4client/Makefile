include ../../config.mak

vpath %.c $(SRC_PATH)/applications/mp4client

CFLAGS= $(OPTFLAGS) -Wall -I$(SRC_PATH)/include

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

ifeq ($(BUILD_INSTALL), yes)
INSTALL_FLAGS=-DGPAC_MODULES_PATH=\"$(moddir)\"
else
INSTALL_FLAGS=
endif

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
	$(CC) $(LDFLAGS) -o ../../bin/gcc/$@ $(OBJS) -L../../bin/gcc -lgpac -lz

%.o: %.c
	$(CC) $(CFLAGS) $(INSTALL_FLAGS) -c -o $@ $< 

clean: 
	rm -f $(OBJS) ../../bin/gcc/$(PROG)

install: clean
	make -override BUILD_INSTALL=yes all
	install -c -s -m 755 ../../bin/gcc/MP4Client "$(prefix)/bin"

uninstall:
	rm -rf $(prefix)/bin/MP4Client

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
