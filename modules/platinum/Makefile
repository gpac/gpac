include ../../config.mak

vpath %.c $(SRC_PATH)/modules/platinum

CFLAGS= $(CXXFLAGS) -I"$(SRC_PATH)/include" -I$(SRC_PATH)/extra_lib/include/platinum/ -DGPAC_HAVE_CONFIG_H -I../..

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

LINKLIBS= -lgpac -lPlatinum -lPltMediaServer -lPltMediaConnect -lPltMediaRenderer -lNeptune -lZlib -lpthread -laxTLS
LOCAL_LIB=../../bin/gcc -L../../extra_lib/lib/gcc

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

ifeq ($(CONFIG_DARWIN), yes)
LINKLIBS+= -lobjc
LDFLAGS+=-framework Foundation
endif


all: $(LIB)


$(LIB): $(OBJS)
	$(CXX) $(SHFLAGS) $(LDFLAGS) -o ../../bin/gcc/$@ $(OBJS) $(EXTRALIBS) -L$(LOCAL_LIB) $(LINKLIBS)

clean: 
	rm -f $(OBJS) ../../bin/gcc/$(LIB)

dep: depend

depend:
	rm -f .depend	
	$(CXX) -MM $(CFLAGS) $(SRCS) 1>.depend

distclean: clean
	rm -f Makefile.bak .depend

-include .depend
