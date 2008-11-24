include ../../config.mak

vpath %.c $(SRC_PATH)/modules/ogg

CFLAGS= $(OPTFLAGS) -I$(SRC_PATH)/include

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

#common obj
OBJS= ogg_in.o ogg_load.o

SRCS := $(OBJS:.o=.c) 

NEED_LOCAL_LIB="no"
LOCAL_LIB=../../bin/gcc
LINKLIBS= -lgpac -logg

ifeq ($(CONFIG_OGG), local)
NEED_LOCAL_LIB="yes"
endif

ifeq ($(CONFIG_VORBIS), no)
else
OBJS+= vorbis_dec.o
LINKLIBS+= -lvorbis
CFLAGS+=-DGPAC_HAS_VORBIS
ifeq ($(CONFIG_VORBIS), local)
NEED_LOCAL_LIB="yes"
CFLAGS+= -I$(LOCAL_INC_PATH)
endif
endif

ifeq ($(CONFIG_THEORA), no)
else
OBJS+= theora_dec.o
LINKLIBS+= -ltheora
CFLAGS+=-DGPAC_HAS_THEORA
ifeq ($(CONFIG_THEORA), local)
NEED_LOCAL_LIB="yes"
CFLAGS+= -I$(LOCAL_INC_PATH)
endif
endif


#add local lib path
ifeq ($(NEED_LOCAL_LIB), "yes")
LOCAL_LIB+=-L../../extra_lib/lib/gcc
endif


LIB=gm_ogg_xiph.$(DYN_LIB_SUFFIX)
ifeq ($(CONFIG_WIN32),yes)
LDFLAGS+=-export-symbols ogg.def 
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
