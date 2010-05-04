include ../../config.mak

vpath %.c $(SRC_PATH)/modules/platinum

CFLAGS= $(CPPFLAGS) -I$(SRC_PATH)/include -ISDK/include

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

LINKLIBS= -lgpac -lPlatinum -lPltMediaServer -lPltMediaConnect -lPltMediaRenderer -lNeptune -lZlib
LOCAL_LIB=../../bin/gcc -LSDK/lib_gcc_x86

#common objects
OBJS=GPACFileMediaServer.o GPACMediaController.o GPACMediaRenderer.o GenericDevice.o GPACPlatinum.o

ifeq ($(CONFIG_JS),no)
else
CFLAGS+= $(JS_FLAGS)
ifeq ($(CONFIG_JS),local)
NEED_LOCAL_LIB="yes"
LOCAL_LIB+=-L../../extra_lib/lib/gcc
endif
LINKLIBS+= $(JS_LIBS)
endif

SRCS := $(OBJS:.o=.c) 

LIB=gm_platinum.$(DYN_LIB_SUFFIX)
ifeq ($(CONFIG_WIN32),yes)
#LDFLAGS+=-export-symbols platinum.def 
endif


all: $(LIB)


$(LIB): $(OBJS)
	$(CXX) $(SHFLAGS) $(LDFLAGS) -o ../../bin/gcc/$@ $(OBJS) $(EXTRALIBS) -L$(LOCAL_LIB) $(LINKLIBS)


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
