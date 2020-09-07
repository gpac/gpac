include ../../config.mak

vpath %.c $(SRC_PATH)/modules/oss_audio

CFLAGS= $(OPTFLAGS) -I"$(SRC_PATH)/include" $(OSS_CFLAGS)
LDFLAGS+=$(OSS_LDFLAGS)

ifeq ($(DEBUGBUILD),yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD),yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

ifeq ($(OSS_INC_TYPE),yes)
else
CFLAGS+=-DOSS_FIX_INC
endif

ifeq ($(TARGET_ARCH_ARMV4L),yes)
CFLAGS+=-DFORCE_SR_LIMIT
endif

#common obj
OBJS= oss.o

SRCS := $(OBJS:.o=.c) 

LIB=gm_oss_audio$(DYN_LIB_SUFFIX)


all: $(LIB)

$(LIB): $(OBJS)
	$(CC) $(SHFLAGS) $(LDFLAGS) -o ../../bin/gcc/$@ $(OBJS) $(EXTRALIBS) -L../../bin/gcc -lgpac
ifeq ($(STATICBUILD),yes)
	$(CC) $(SHFLAGS) -o ../../bin/gcc/gm_oss_audio-static$(DYN_LIB_SUFFIX) $(OBJS) $(EXTRALIBS) -L../../bin/gcc -lgpac_static $(OSS_LDFLAGS) $(LDFLAGS)
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
