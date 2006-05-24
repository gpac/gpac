include ../../config.mak

vpath %.c $(SRC_PATH)/applications/osmo4_wx

CFLAGS= $(OPTFLAGS) -Wall -I$(SRC_PATH)/include

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

#common obj
OBJS= wxOsmo4.o wxGPACControl.o fileprops.o Playlist.o menubtn.o

ifeq ($(BUILD_INSTALL), yes)
INSTALL_FLAGS=-DGPAC_MODULES_PATH=\"$(moddir)\"
else
INSTALL_FLAGS=
endif

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
	$(CC) $(LDFLAGS) -o ../../bin/gcc/$@ $(OBJS) -L../../bin/gcc -lgpac $(WX_LFLAGS) -lz

%.o: %.cpp
	$(CXX) $(CFLAGS) $(INSTALL_FLAGS) $(WX_CFLAGS) -c -o $@ $< 

clean: 
	rm -f $(OBJS) ../../bin/gcc/$(PROG)

install: 
ifeq ($(CONFIG_DARWIN),yes)
	mkdir -p $(DESTDIR)$(mac_apps)/Osmo4.app/Contents/MacOS
	mkdir -p $(DESTDIR)$(mac_apps)/Osmo4.app/Contents/Resources/English.lproj
	cp ./Darwin.Info.plist \
	   $(DESTDIR)$(mac_apps)/Osmo4.app/Contents/Info.plist
	cp ./Darwin.InfoPlist.strings \
	   $(DESTDIR)$(mac_apps)/Osmo4.app/Contents/Resources/English.lproj/InfoPlist.strings
	cp ./Darwin.Osmo.icns \
	   $(DESTDIR)$(mac_apps)/Osmo4.app/Contents/Resources/Osmo.icns
	install -c -s -m 755 ../../bin/gcc/Osmo4 \
	      $(DESTDIR)$(mac_apps)/Osmo4.app/Contents/MacOS
	echo -n 'APPLOsm4' > $(DESTDIR)$(mac_apps)/Osmo4.app/Contents/PkgInfo
else
	rm -f wxOsmo4.o
	make -override BUILD_INSTALL=yes all
	install -c -s -m 755 ../../bin/gcc/Osmo4 "$(prefix)/bin"
endif

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

