include ../../config.mak

vpath %.c $(SRC_PATH)/modules/ffmpeg_in

CFLAGS= $(OPTFLAGS) -I$(SRC_PATH)/include

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

LOCAL_LIB=
LINKLIBS=-lgpac -lavcodec -lavformat -lz

#common obj
OBJS=ffmpeg_decode.o ffmpeg_demux.o ffmpeg_load.o

#local ffmpeg lib
ifeq ($(CONFIG_FFMPEG), local)
LOCAL_LIB=-L../../extra_lib/lib/gcc
CFLAGS+= -I$(LOCAL_INC_PATH)
endif

ifeq ($(CONFIG_ZLIB), local)
LOCAL_LIB=-L../../extra_lib/lib/gcc
endif


SRCS := $(OBJS:.o=.c) 

LIB=gm_ffmpeg_in.$(DYN_LIB_SUFFIX)
ifeq ($(CONFIG_WIN32),yes)
LDFLAGS+=-export-symbols ffmpeg_in.def 
endif

#need to add cross-libraries if not local
ifeq ($(CROSS_COMPILING),yes)
EXTRALIBS+=-L$(prefix)/lib
endif

ifeq ($(WANT_PIC),yes)
OBJSPIC=$(OBJS:.o=.opic)
else
OBJSPIC=$(OBJS)
endif

all: $(LIB)


$(LIB): $(OBJSPIC)
	$(CC) $(SHFLAGS) $(LDFLAGS) -o ../../bin/gcc/$@ $(OBJSPIC) -L../../bin/gcc $(LOCAL_LIB) $(LINKLIBS) $(EXTRALIBS)


%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $< 


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
