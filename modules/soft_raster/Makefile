include ../../config.mak

VPATH=$(SRC_PATH)/modules/soft_raster

CFLAGS= $(OPTFLAGS) -Wall -I$(SRC_PATH)/include

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

#big-endian config (needed for ARGB pixel format)
ifeq ($(IS_BIGENDIAN), yes)
CFLAGS+=-DEVG_BIG_ENDIAN
endif


#common obj
OBJS= ftgrays.o raster_load.o raster_565.o raster_argb.o raster_rgb.o stencil.o surface.o

SRCS := $(OBJS:.o=.c) 

LIB=gm_soft_raster.$(DYN_LIB_SUFFIX)
ifeq ($(CONFIG_WIN32),yes)
LDFLAGS+=-export-symbols rast_soft.def 
endif

all: $(LIB)


$(LIB): $(OBJS)
	$(CC) $(SHFLAGS) $(LDFLAGS) -o ../../bin/gcc/$@ $(OBJS) $(EXTRALIBS) -L../../bin/gcc -lgpac


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
