include ../../config.mak

vpath %.c $(SRC_PATH)/modules/dektec_out

CFLAGS= $(OPTFLAGS) -I"$(SRC_PATH)/include" -DGPAC_HAVE_CONFIG_H -I../../

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

ifeq ($(ENABLE_JOYSTICK), yes)
CFLAGS+=-DENABLE_JOYSTICK
endif

ifeq ($(ENABLE_JOYSTICK_NO_CURSOR), yes)
CFLAGS+=-DENABLE_JOYSTICK_NO_CURSOR
endif

#common obj
OBJS= dektec_video.o

SRCS := $(OBJS:.o=.cpp) 

LIB=gm_dektec_out.$(DYN_LIB_SUFFIX)
ifeq ($(CONFIG_WIN32),yes)
#LDFLAGS+=-export-symbols dektec_out.def 
endif


all: $(LIB)


$(LIB): $(OBJS)
	$(CXX) $(SHFLAGS) $(LDFLAGS) -o ../../bin/gcc/$@ $(OBJS) $(EXTRALIBS) -L../../bin/gcc -lgpac
ifeq ($(STATICBUILD),yes)
	$(CXX) $(SHFLAGS) $(LDFLAGS) -o ../../bin/gcc/gm_dektec_out-static.$(DYN_LIB_SUFFIX) $(OBJS) $(EXTRALIBS) -L../../bin/gcc -lgpac_static
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
