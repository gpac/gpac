include ../../config.mak

vpath %.c $(SRC_PATH)/modules/ffmpeg_in

CFLAGS=-fPIC $(OPTFLAGS) -Wno-deprecated-declarations -I"$(SRC_PATH)/include" $(ffmpeg_cflags)

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

LOCAL_LIB=
LINKLIBS=-lgpac -lz $(ffmpeg_lflags)

#darwin needs bz2
ifeq ($(CONFIG_DARWIN),yes)
LINKLIBS+=-lbz2
endif

#old ffmpeg lib
ifeq ($(CONFIG_FFMPEG_OLD), yes)
CFLAGS+=-DFFMPEG_OLD_HEADERS -I/usr/include
else
LINKLIBS+=-lswscale
endif

#common obj
OBJS=ffmpeg_decode.o ffmpeg_demux.o ffmpeg_load.o

#local ffmpeg lib
ifeq ($(CONFIG_FFMPEG), local)
LOCAL_LIB=-L../../extra_lib/lib/gcc
CFLAGS+= -I"$(LOCAL_INC_PATH)"
endif

ifeq ($(CONFIG_ZLIB), local)
LOCAL_LIB=-L../../extra_lib/lib/gcc
endif


SRCS := $(OBJS:.o=.c) 

LIB=gm_ffmpeg_in.$(DYN_LIB_SUFFIX)
ifeq ($(CONFIG_WIN32),yes)
#LDFLAGS+=-export-symbols ffmpeg_in.def 
endif

#need to add cross-libraries if not local
ifeq ($(CROSS_COMPILING),yes)
EXTRALIBS+=-L$(prefix)/lib
endif


all: $(LIB)


$(LIB): $(OBJS)
	$(CC) $(SHFLAGS) $(LDFLAGS) -o ../../bin/gcc/$@ $(OBJS) -L../../bin/gcc $(LOCAL_LIB) $(LINKLIBS) $(EXTRALIBS)

clean: 
	rm -f $(OBJS) ../../bin/gcc/$(LIB)

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
