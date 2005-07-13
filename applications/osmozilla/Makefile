include ../../config.mak

VPATH=$(SRC_PATH)/applications/osmozilla

CFLAGS= $(OPTFLAGS) -Wall -I$(SRC_PATH)/include \
  -I$(SRC_PATH)/extra_lib/include/gecko-sdk \
  -I$(SRC_PATH)/extra_lib/include/gecko-sdk/plugin/include  \
  -I$(SRC_PATH)/extra_lib/include/gecko-sdk/nspr/include \
  -I$(SRC_PATH)/extra_lib/include/gecko-sdk/java/include \
  -I$(SRC_PATH)/extra_lib/include/gecko-sdk/xpcom/include \

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(CONFIG_WIN32),yes)
CFLAGS+=-DXP_WIN
else
ifeq ($(CONFIG_DARWIN),yes)
CFLAGS+=-DXP_MAC
else
CFLAGS+=-DXP_UNIX -DMOZ_X11
endif
endif
CFLAGS+=-DNPBASIC_EXPORTS -DMOZILLA_STRICT_API -DXPCOM_GLUE


LINKLIBS=-L../../bin/gcc -lgpac
#LDFLAGS=     -L/usr/X11R6/lib  -lX11   -lSM -lICE -lXext -lX11 -lXpm -lXt -L/usr/lib/mozilla -lxpcom -lplds4 -lplc4 -lnspr4 -ldl -lc -lpthread   -lgtk-x11-2.0 -lgdk-x11-2.0 -latk-1.0 -lgdk_pixbuf-2.0 -lm -lpangoxft-1.0 -lpangox-1.0 -lpango-1.0 -lgobject-2.0 -lgmodule-2.0 -ldl -lglib-2.0   -pthread -lgthread-2.0 -lglib-2.0 -L../../../bin/gcc/ -lgpac

OBJS= osmozilla.o npp_gate.o np_entry.o npn_gate.o

SRCS := $(OBJS:.o=.cpp) 


LIB=nposmozilla.$(DYN_LIB_SUFFIX)
ifeq ($(CONFIG_WIN32),yes)
LINKLIBS+=-lwinmm -lgdi32
LDFLAGS+=--export-all-symbols
endif

all: $(LIB)

$(LIB): $(OBJS)
ifeq ($(CONFIG_WIN32),yes)
	windres osmozilla.rc osmoz.o
	$(CXX) $(SHFLAGS) $(LDFLAGS) -o ../../bin/gcc/$@ $(OBJS) osmoz.o $(LINKLIBS)
else
	$(CXX) $(SHFLAGS) $(LDFLAGS) $(OBJS) $(LINKLIBS) -o ../../bin/gcc/$@ 
endif
	cp nsIOsmozilla.xpt ../../bin/gcc/nposmozilla.xpt

%.o: %.cpp
	$(CXX) $(CFLAGS) -c -o $@ $< 

clean:
	rm -f $(OBJS) ../../bin/gcc/$(LIB) ../../bin/gcc/osmozilla.xpt
ifeq ($(CONFIG_WIN32),yes)
	rm -f osmoz.o
endif

  
dep: depend

depend:
	rm -f .depend	
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

distclean: clean
	rm -f Makefile.bak .depend

