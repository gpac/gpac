include ../../config.mak

vpath %.c $(SRC_PATH)/modules/avcap

CFLAGS= $(CXXFLAGS) -I"$(SRC_PATH)/include" $(AVCAP_CFLAGS)

ifeq ($(DEBUGBUILD),yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD),yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

LINKLIBS= $(AVCAP_LDFLAGS) -L../../bin/gcc -lgpac


#common objects
OBJS=avcap.o

SRCS := $(OBJS:.o=.c) 

LIB=gm_avcap$(DYN_LIB_SUFFIX)


all: $(LIB)


$(LIB): $(OBJS)
	$(CXX) -w $(SHFLAGS) -o ../../bin/gcc/$@ $(OBJS) $(LINKLIBS) $(LDFLAGS)

clean: 
	rm -f $(OBJS) ../../bin/gcc/$(LIB)

dep: depend

depend:
	rm -f .depend	
	$(CXX) -MM $(CFLAGS) $(SRCS) 1>.depend

distclean: clean
	rm -f Makefile.bak .depend

-include .depend
