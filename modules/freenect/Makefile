include ../../config.mak

vpath %.c $(SRC_PATH)/modules/freenect

CFLAGS= $(CXXFLAGS) -I"$(SRC_PATH)/include" $(FREENECT_CFLAGS)

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif


LINKLIBS=

ifeq ($(CONFIG_FREENECT), local)
LINKLIBS+=-L../../extra_lib/lib/gcc
CFLAGS+= -I"$(LOCAL_INC_PATH)"
endif

LINKLIBS+=-lfreenect -L../../bin/gcc -lgpac


#common objects
OBJS=freenect.o

SRCS := $(OBJS:.o=.c) 

LIB=gm_freenect.$(DYN_LIB_SUFFIX)


all: $(LIB)


$(LIB): $(OBJS)
	$(CXX) -w $(SHFLAGS) $(LDFLAGS) -o ../../bin/gcc/$@ $(OBJS) $(LINKLIBS)

clean: 
	rm -f $(OBJS) ../../bin/gcc/$(LIB)

dep: depend

depend:
	rm -f .depend	
	$(CXX) -MM $(CFLAGS) $(SRCS) 1>.depend

distclean: clean
	rm -f Makefile.bak .depend



# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
