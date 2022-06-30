include ../../config.mak

vpath %.c $(SRC_PATH)/modules/jack

CFLAGS=-I"$(SRC_PATH)/include" $(OPTFLAGS) $(oss_cflags) -Wno-deprecated-declarations

ifeq ($(DEBUGBUILD),yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD),yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

#common obj
OBJS= jack.o

SRCS := $(OBJS:.o=.c) 


LIB=gm_jack$(DYN_LIB_SUFFIX)

all: $(LIB)

$(LIB): $(OBJS)
	$(CC) $(SHFLAGS) $(LDFLAGS) -o ../../bin/gcc/$@ $(OBJS) $(EXTRALIBS) -L../../bin/gcc -lgpac -L$(prefix)/$(libdir)  -ljack

clean: 
	rm -f $(OBJS) ../../bin/gcc/$(LIB)

dep: depend

depend:
	rm -f .depend	
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

distclean: clean
	rm -f Makefile.bak .depend

-include .depend
