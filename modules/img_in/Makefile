include ../../config.mak

vpath %.c $(SRC_PATH)/modules/img_in

CFLAGS= $(OPTFLAGS) -I$(SRC_PATH)/include

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

#common objects
OBJS=img_dec.o img_in.o bmp_dec.o png_dec.o jpeg_dec.o

#openjpeg config
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

ifeq ($(WANT_PIC),yes)
OBJSPIC=$(OBJS:.o=.opic)
else
OBJSPIC=$(OBJS)
endif

all: $(LIB)


$(LIB): $(OBJSPIC)
	$(CC) $(SHFLAGS) $(LDFLAGS) -o ../../bin/gcc/$@ $(OBJSPIC) $(EXTRALIBS) -L$(LOCAL_LIB) $(LINKLIBS)


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
