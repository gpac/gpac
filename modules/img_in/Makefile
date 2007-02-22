include ../../config.mak

vpath %.c $(SRC_PATH)/modules/img_in

CFLAGS= $(OPTFLAGS) -Wall -I$(SRC_PATH)/include

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

NEED_LOCAL_LIB="no"
LOCAL_LIB=../../bin/gcc
LINKLIBS= -lgpac

#common obj
OBJS=img_dec.o img_in.o bmp_dec.o

#png config
ifeq ($(CONFIG_PNG), no)
else
OBJS+=png_dec.o
LINKLIBS+= -lpng -lz
CFLAGS+=-DGPAC_HAS_PNG
#local png lib
ifeq ($(CONFIG_PNG), local)
NEED_LOCAL_LIB="yes"
CFLAGS+= -I$(LOCAL_INC_PATH)/png
endif
endif

ifeq ($(CONFIG_ZLIB),local)
NEED_LOCAL_LIB="yes"
CFLAGS+= -I$(LOCAL_INC_PATH)/zlib
endif

#jpeg config
ifeq ($(CONFIG_JPEG), no)
else
OBJS+=jpeg_dec.o
LINKLIBS+= -ljpeg
CFLAGS+=-DGPAC_HAS_JPEG
#local png lib
ifeq ($(CONFIG_JPEG), local)
NEED_LOCAL_LIB="yes"
CFLAGS+= -I$(LOCAL_INC_PATH)/jpeg
endif
endif

#big-endian config (needed for PNG dec)
ifeq ($(IS_BIGENDIAN), yes)
CFLAGS+=-DGPAC_BIG_ENDIAN
endif


#png config
ifeq ($(CONFIG_JP2), no)
else
OBJS+=jp2_dec.o
LINKLIBS+= -lopenjpeg
CFLAGS+=-DGPAC_HAS_JP2
#local openjpeg lib
ifeq ($(CONFIG_JP2), local)
NEED_LOCAL_LIB="yes"
CFLAGS+= -I$(LOCAL_INC_PATH)/openjpeg
endif
endif


#add local lib path
ifeq ($(NEED_LOCAL_LIB), "yes")
LOCAL_LIB+=-L../../extra_lib/lib/gcc
endif



SRCS := $(OBJS:.o=.c) 

LIB=gm_img_in.$(DYN_LIB_SUFFIX)
ifeq ($(CONFIG_WIN32),yes)
LINKLIBS+=-lwinmm
LDFLAGS+=-export-symbols img_in.def 
endif

all: $(LIB)


$(LIB): $(OBJS)
	$(CC) $(SHFLAGS) $(LDFLAGS) -o ../../bin/gcc/$@ $(OBJS) $(EXTRALIBS) -L$(LOCAL_LIB) $(LINKLIBS)


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
