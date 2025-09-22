include ../../config.mak

vpath %.c $(SRC_PATH)/modules/dektec_out

CFLAGS=-I"$(SRC_PATH)/include" $(OPTFLAGS)

ifeq ($(DEBUGBUILD),yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD),yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

#common obj
OBJS= dektec_video.o

SRCS := $(OBJS:.o=.cpp) 

LIB=gm_dektec_out$(DYN_LIB_SUFFIX)
ifeq ($(CONFIG_WIN32),yes)
#LDFLAGS+=-export-symbols dektec_out.def 
endif


all: $(LIB)


$(LIB): $(OBJS)
	$(CXX) $(SHFLAGS) -o ../../bin/gcc/$@ $(OBJS) $(EXTRALIBS) -L../../bin/gcc -lgpac $(LDFLAGS)
ifeq ($(STATIC_BUILD),yes)
	$(CXX) $(SHFLAGS) -o ../../bin/gcc/gm_dektec_out-static$(DYN_LIB_SUFFIX) $(OBJS) $(EXTRALIBS) -L../../bin/gcc -lgpac_static $(LDFLAGS)
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
