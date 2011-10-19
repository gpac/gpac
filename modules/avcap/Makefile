include ../../config.mak

vpath %.c $(SRC_PATH)/modules/avcap

CFLAGS= $(CPPFLAGS) -I"$(SRC_PATH)/include" $(AVCAP_CFLAGS)

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

LINKLIBS= $(AVCAP_LDFLAGS) -lgpac


#common objects
OBJS=avcap.o

SRCS := $(OBJS:.o=.c) 

LIB=gm_avcap.$(DYN_LIB_SUFFIX)


all: $(LIB)


$(LIB): $(OBJS)
	$(CXX) $(SHFLAGS) $(LDFLAGS) -o ../../bin/gcc/$@ $(OBJS) $(LINKLIBS)


%.o: %.cpp
	$(CXX) $(CFLAGS) -c -o $@ $< 


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
