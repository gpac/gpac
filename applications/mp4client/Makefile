include ../../config.mak

vpath %.c $(SRC_PATH)/applications/mp4client

CFLAGS= $(OPTFLAGS) -I$(SRC_PATH)/include

LINKLIBS=$(OGL_LIBS)

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

ifneq ($(CONFIG_JPEG), no)
LINKLIBS+= -L$(prefix)/lib
else
ifneq ($(CONFIG_PNG), no)
LINKLIBS+= -L$(prefix)/lib
else
ifneq ($(TRISCOPE_MODE), no)
LINKLIBS+= -L$(prefix)/lib
endif
endif
endif

ifeq ($(CONFIG_JPEG), no)
else
LINKLIBS+= -ljpeg
endif

ifeq ($(CONFIG_PNG), no)
else    
LINKLIBS+= -lpng
endif

ifeq ($(TRISCOPE_MODE), no)
else
LINKLIBS+= -lViRenDrivers_linux
endif    



#ifeq ($(TARGET_ARCH_X86), yes)
#EXTRALIBS+= -L/comelec/users/comalren/renoir-application-drivers/lower-layers/sh4-x86/LINUXSH4/lib/x86_linux
#ifeq ($(CC), sh4-unknown-linux-gnu-gcc)
#EXTRALIBS+= -L/comelec/users/comalren/renoir-application-drivers/lower-layers/sh4-x86/LINUXSH4/lib/sh4_linux
#endif
#endif
#endif

SRCS := $(OBJS:.o=.c) 

all: $(PROG)

MP4Client$(EXE): $(OBJS)
	$(CC) $(LDFLAGS) -o ../../bin/gcc/$@ $(OBJS) -L../../bin/gcc -lgpac -lz $(LINKLIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(INSTALL_FLAGS) -c -o $@ $< 

clean: 
	rm -f $(OBJS) ../../bin/gcc/$(PROG)

install: clean
	$(MAKE) -override BUILD_INSTALL=yes all
	install -m 755 $(INSTFLAGS) ../../bin/gcc/MP4Client "$(DESTDIR)$(prefix)/bin"

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
