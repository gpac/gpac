include ../../config.mak

vpath %.c $(SRC_PATH)/modules/mp3_in

CFLAGS= $(OPTFLAGS) -I"$(SRC_PATH)/include"

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

#common obj
OBJS= mp3_in.o



#mad config
ifeq ($(CONFIG_MAD), no)
else
OBJS+=mad_dec.o
CFLAGS+=-DGPAC_HAS_MAD
#local mad lib
ifeq ($(CONFIG_MAD), local)
EXTRALIBS+=-L../../extra_lib/lib/gcc
CFLAGS+= -I"$(LOCAL_INC_PATH)/mad"
endif
EXTRALIBS+= -lmad
endif

SRCS := $(OBJS:.o=.c) 

LIB=gm_mp3_in.$(DYN_LIB_SUFFIX)
ifeq ($(CONFIG_WIN32),yes)
#LDFLAGS+=-export-symbols mp3_in.def 
endif


all: $(LIB)


$(LIB): $(OBJS)
	$(CC) $(SHFLAGS) $(LDFLAGS) -o ../../bin/gcc/$@ $(OBJS) -L../../bin/gcc -lgpac $(EXTRALIBS)
ifeq ($(STATICBUILD),yes)
	$(CC) $(SHFLAGS) $(LDFLAGS) -o ../../bin/gcc/gm_mp3_in-static.$(DYN_LIB_SUFFIX) $(OBJS) -L../../bin/gcc -lgpac_static $(EXTRALIBS) 
endif


clean: 
	rm -f $(OBJS) ../../bin/gcc/$(LIB)

dep: depend

depend:
	rm -f .depend	
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

distclean: clean
	rm -f Makefile.bak .depend

-include .depend
