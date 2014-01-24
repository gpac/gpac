include ../../config.mak

vpath %.c $(SRC_PATH)/modules/opensvc_dec

CFLAGS=$(OPTFLAGS) -I"$(SRC_PATH)/include" $(OSVC_CFLAGS)

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

EXTRALIBS+=-L../../bin/gcc -lgpac $(OSVC_LDFLAGS)

#common obj
OBJS= opensvc_dec.o

SRCS := $(OBJS:.o=.c) 

LIB=gm_opensvc_dec.$(DYN_LIB_SUFFIX)


all: $(LIB)


$(LIB): $(OBJS)
	$(CC) $(SHFLAGS) $(LDFLAGS) -o ../../bin/gcc/$@ $(OBJS) $(EXTRALIBS)

clean: 
	rm -f $(OBJS) ../../bin/gcc/$(LIB)

dep: depend

depend:
	rm -f .depend	
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

distclean: clean
	rm -f Makefile.bak .depend

-include .depend
