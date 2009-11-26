include ../../config.mak

vpath %.c $(SRC_PATH)/applications/mp4client

CFLAGS= $(OPTFLAGS) -I$(SRC_PATH)/include -I../../

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

#not the same path as local in target
ifneq ($(CROSS_COMPILING), yes)
ifeq ($(BUILD_INSTALL), yes)
INSTALL_FLAGS=-DGPAC_MODULES_PATH=\"$(moddir)\"
else
INSTALL_FLAGS=
endif
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
ifneq ($(TRISCOPE_MODE),no)
LINKLIBS+= -L$(prefix)/lib
else
ifeq ($(CONFIG_JS),prefix)
LINKLIBS+= -L$(prefix)/lib
endif
endif
endif
endif

ifeq ($(CONFIG_JPEG), no)
else
LINKLIBS+= -ljpeg
endif

ifeq ($(CONFIG_JS), prefix)
LINKLIBS+= -ljs
else
endif

ifeq ($(CONFIG_PNG), no)
else    
LINKLIBS+= -lpng
endif

ifneq ($(TRISCOPE_MODE),no)
CFLAGS+=-DGPAC_TRISCOPE_MODE
#if not local, they are supposed to be dynamic in system - need to link them
ifeq ($(CONFIG_RENOIR_DRIVERS), unknown)
LINKLIBS+= -lViRenDrivers_linux
endif    
endif    




SRCS := $(OBJS:.o=.c) 

all: $(PROG)

MP4Client$(EXE): $(OBJS)
#	$(CC) $(LDFLAGS) -o ../../bin/gcc/$@ $(OBJS) -L../../bin/gcc -lgpac $(LINKLIBS) -lz  
#ifeq ($(STATICBUILD),yes)
#ifeq ($(TRISCOPE_MODE),simulator)
#	$(CC) $(LDFLAGS) -o ../../bin/gcc/MP4Client_static $(OBJS) -L../../bin/gcc -L../../src/compositor/triscope_renoir -lgpac_static -lz $(LINKLIBS) -lViRenDrivers_linux -lEmulRen_linux -lViRen_linux -lToolTraceManager -lToolMemoryManager -lOsalRealtime -lOsalFile -lOsalUriHandler -lOsalCharStar -lOsalMath -lpthread -ldl
#else
#ifneq ($(TRISCOPE_MODE),no)
#	$(CC) $(LDFLAGS) -o ../../bin/gcc/MP4Client_static $(OBJS) -L../../bin/gcc -L../../src/compositor/triscope_renoir -lgpac_static -lz $(LINKLIBS) -lViRenDrivers_linux -lViRen_linux -lToolTraceManager -lToolMemoryManager -lOsalRealtime -lOsalFile -lOsalUriHandler -lOsalCharStar -lOsalMath -lpthread -ldl
#else
	#$(CC) $(LDFLAGS) -o ../../bin/gcc/MP4Client_static $(OBJS) -L../../bin/gcc -lgpac_static -lz $(LINKLIBS) -lpthread -ldl
##endif
##endif
##endif
	$(CC) $(LDFLAGS) -o ../../bin/gcc/$@ $(OBJS) -L../../bin/gcc -L../../extra_lib/lib/gcc -lgpac -lz $(LINKLIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(INSTALL_FLAGS) -c -o $@ $< 

clean: 
	rm -f $(OBJS) ../../bin/gcc/$(PROG)
       
#this creates an executable with libgpac_static.a (statically linked)
static-bin:
ifeq ($(TRISCOPE_MODE),simulator)
	$(CC) $(LDFLAGS) -o ../../bin/gcc/MP4Client_static $(OBJS) -L../../bin/gcc -L../../src/compositor/triscope_renoir -lgpac_static -lz $(LINKLIBS) -lViRenDrivers_linux -lEmulRen_linux -lViRen_linux -lToolTraceManager -lToolMemoryManager -lOsalRealtime -lOsalFile -lOsalUriHandler -lOsalCharStar -lOsalMath -lpthread -ldl
else
ifneq ($(TRISCOPE_MODE),no)
	$(CC) $(LDFLAGS) -o ../../bin/gcc/MP4Client_static $(OBJS) -L../../bin/gcc -L../../src/compositor/triscope_renoir -lgpac_static -lz $(LINKLIBS) -lViRenDrivers_linux -lViRen_linux -lToolTraceManager -lToolMemoryManager -lOsalRealtime -lOsalFile -lOsalUriHandler -lOsalCharStar -lOsalMath -lpthread -ldl
else
	$(CC) $(LDFLAGS) -o ../../bin/gcc/MP4Client_static $(OBJS) -L../../bin/gcc -lgpac_static -lz $(LINKLIBS) -lpthread -ldl
endif
endif

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
