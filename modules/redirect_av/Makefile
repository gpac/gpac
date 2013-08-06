include ../../config.mak

vpath %.c $(SRC_PATH)/modules/redirect_av

CFLAGS= $(OPTFLAGS) -I"$(SRC_PATH)/include" $(ffmpeg_cflags)

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

LINKLIBS= -lgpac $(ffmpeg_lflags)
LOCAL_LIB=../../bin/gcc

#common objects
OBJS=redirect_av.o

ifeq ($(CONFIG_DARWIN), yes)
#fixme - use proper detection of libavdevice dependencies
LINKLIBS+=-lbz2 
endif



SRCS := $(OBJS:.o=.c) 

LIB=gm_redirect_av.$(DYN_LIB_SUFFIX)

all: $(LIB)


$(LIB): $(OBJS)
	$(CC) $(SHFLAGS) $(LDFLAGS) -o ../../bin/gcc/$@ $(OBJS) $(EXTRALIBS) -L$(LOCAL_LIB) $(LINKLIBS)


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
