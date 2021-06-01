include ../../config.mak

vpath %.c $(SRC_PATH)/modules/test_filter

CFLAGS=-I"$(SRC_PATH)/include" 

ifeq ($(DEBUGBUILD),yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD),yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

ifeq ($(CONFIG_DARWIN),yes)
LINKFLAGS+=
else
LINKFLAGS+=-Wl,-Bsymbolic
endif

#common obj
OBJS=test_filter.o

ifeq ($(CONFIG_WIN32),yes)
else
EXTRALIBS+=-ldl
endif

LINKFLAGS+=-lgpac

SRCS := $(OBJS:.o=.c) 

LIB=gf_testfilter$(DYN_LIB_SUFFIX)

all: $(LIB)

$(LIB): $(OBJS)
	$(CC) $(SHFLAGS) -o ../../bin/gcc/$@ $(OBJS) $(LINKFLAGS) $(LDFLAGS)

clean: 
	rm -f $(OBJS) ../../bin/gcc/$(LIB)

dep: depend

depend:
	rm -f .depend	
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

distclean: clean
	rm -f Makefile.bak .depend

-include .depend
