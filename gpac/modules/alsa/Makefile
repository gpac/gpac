include ../../config.mak

vpath %.c $(SRC_PATH)/modules/alsa

CFLAGS=-I"$(SRC_PATH)/include" $(OPTFLAGS) $(oss_cflags)
LDFLAGS+=$(oss_ldflags)

ifeq ($(DEBUGBUILD),yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD),yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

#common obj
OBJS= alsa.o

SRCS := $(OBJS:.o=.c) 

LIB=gm_alsa$(DYN_LIB_SUFFIX)


all: $(LIB)

$(LIB): $(OBJS)
	$(CC) $(SHFLAGS) $(LDFLAGS) -o ../../bin/gcc/$@ $(OBJS) $(EXTRALIBS) $(LDFLAGS) -lasound -L../../bin/gcc -lgpac


clean: 
	rm -f $(OBJS) ../../bin/gcc/$(LIB)

dep: depend

depend:
	rm -f .depend	
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

distclean: clean
	rm -f Makefile.bak .depend

-include .depend
