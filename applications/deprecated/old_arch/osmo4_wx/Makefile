include ../../config.mak

vpath %.cpp $(SRC_PATH)/applications/osmo4_wx

CFLAGS= $(CXXFLAGS) -I"$(SRC_PATH)/include"

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

#common obj
OBJS= wxOsmo4.o wxGPACControl.o fileprops.o Playlist.o menubtn.o

ifeq ($(CONFIG_WIN32),yes)
EXE=.exe
PROG=Osmo4$(EXE)
else
EXT=
PROG=Osmo4
endif

#3 - spidermonkey support
ifeq ($(CONFIG_JS),no)
else
SCENEGRAPH_CFLAGS+=$(JS_FLAGS)
ifeq ($(CONFIG_JS),local)
NEED_LOCAL_LIB=yes
endif
LINKFLAGS+=$(JS_LIBS)
endif

CFLAGS+=$(WX_CFLAGS)

SRCS := $(OBJS:.o=.cpp) 

all: $(PROG)

Osmo4$(EXE): $(OBJS)
	$(CC) -o ../../bin/gcc/$@ $(OBJS) -L../../bin/gcc -lgpac $(WX_LFLAGS) $(LINKFLAGS) $(LDFLAGS)

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
	install -m 755 $(INSTFLAGS) ../../bin/gcc/Osmo4 \
	      $(DESTDIR)$(mac_apps)/Osmo4.app/Contents/MacOS
	echo -n 'APPLOsm4' > $(DESTDIR)$(mac_apps)/Osmo4.app/Contents/PkgInfo
else
	rm -f wxOsmo4.o
	mkdir -p $(DESTDIR)$(prefix)/bin
	install -m 755 $(INSTFLAGS) ../../bin/gcc/Osmo4 "$(DESTDIR)$(prefix)/bin"
endif

uninstall:
	rm -rf $(DESTDIR)$(prefix)/bin/Osmo4

dep:

depend:
	rm -f .depend	
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

distclean: clean
	rm -f Makefile.bak .depend

-include .depend
