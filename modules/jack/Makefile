include ../../config.mak

vpath %.c $(SRC_PATH)/modules/jack

CFLAGS= $(OPTFLAGS) -I"$(SRC_PATH)/include" $(OSS_CFLAGS) -Wno-deprecated-declarations

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

#common obj
OBJS= jack.o

SRCS := $(OBJS:.o=.c) 


LIB=gm_jack.$(DYN_LIB_SUFFIX)

all: $(LIB)

$(LIB): $(OBJS)
	$(CC) $(SHFLAGS) -o ../../bin/gcc/$@ $(OBJS) $(EXTRALIBS) -L../../bin/gcc -lgpac -L/usr/lib  -ljack

clean: 
	rm -f $(OBJS) ../../bin/gcc/$(LIB)

dep: depend

depend:
	rm -f .depend	
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

distclean: clean
	rm -f Makefile.bak .depend

-include .depend
