include ../../config.mak

VPATH=$(SRC_PATH)/applications/Osmo4_wx

CFLAGS= $(OPTFLAGS) -Wall -I$(SRC_PATH)/include

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

#common obj
OBJS= wxOsmo4.o wxGPACControl.o fileprops.o Playlist.o menubtn.o

ifeq ($(BUILD_INSTALL), yes)
INSTALL_FLAGS=-DGPAC_PLUGIN_PATH=\"$(moddir)\"
else
INSTALL_FLAGS=
endif

LINKFLAGS=-L../../bin/gcc -lm4systems

ifeq ($(CONFIG_WIN32),yes)
EXE=.exe
PROG=Osmo4$(EXE)
else
EXT=
PROG=Osmo4
endif


SRCS := $(OBJS:.o=.cpp) 

all: $(PROG)

Osmo4$(EXE): $(OBJS)
	$(CC) $(LDFLAGS) -o ../../bin/gcc/$@ $(OBJS) -L../../bin/gcc -lm4systems $(WX_LFLAGS)

%.o: %.cpp
	$(CXX) $(CFLAGS) $(INSTALL_FLAGS) $(WX_CFLAGS) -c -o $@ $< 

clean: 
	rm -f $(OBJS) ../../bin/gcc/$(PROG)

install: 
	rm -f wxOsmo4.o
	make -override BUILD_INSTALL=yes all
	install -c -s -m 755 ../../bin/gcc/Osmo4 "$(prefix)/bin"

uninstall:
	rm -rf $(prefix)/bin/Osmo4

dep:

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

