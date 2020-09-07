include ../../config.mak

vpath %.c $(SRC_PATH)/modules/widgetman

CFLAGS= $(OPTFLAGS) -I"$(SRC_PATH)/include"

ifeq ($(DEBUGBUILD),yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD),yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

LINKLIBS= -lgpac -lz
LOCAL_LIB=-L../../bin/gcc

#common objects
OBJS=widgetman.o unzip.o widget.o wgt_load.o

ifeq ($(CONFIG_ZLIB),local)
CFLAGS+=-I"$(LOCAL_INC_PATH)/zlib"
LOCAL_LIB+= -L../../extra_lib/lib/gcc
endif

ifeq ($(CONFIG_JS),no)
else
CFLAGS+=$(JS_FLAGS)
ifeq ($(CONFIG_JS),local)
# if local it is staticly linked with libgpac
NEED_LOCAL_LIB=yes
LOCAL_LIB+= -L../../extra_lib/lib/gcc
else
LINKLIBS+= $(JS_LIBS)
endif
endif

SRCS := $(OBJS:.o=.c)

LIB=gm_widgetman$(DYN_LIB_SUFFIX)
ifeq ($(CONFIG_WIN32),yes)
#LDFLAGS+=-export-symbols widgetman.def
endif


all: $(LIB)


$(LIB): $(OBJS)
	$(CC) $(SHFLAGS) -o ../../bin/gcc/$@ $(OBJS) $(EXTRALIBS) $(LOCAL_LIB) $(LINKLIBS) $(LDFLAGS)

clean:
	rm -f $(OBJS) ../../bin/gcc/$(LIB)

dep: depend

depend:
	rm -f .depend
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

distclean: clean
	rm -f Makefile.bak .depend

-include .depend
