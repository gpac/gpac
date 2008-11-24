include ../../config.mak

vpath %.c $(SRC_PATH)/modules/gpac_js

CFLAGS= $(OPTFLAGS) -I$(SRC_PATH)/include

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

LINKLIBS= -lgpac
LOCAL_LIB=../../bin/gcc

#common objects
OBJS=gpac_js.o

ifeq ($(CONFIG_JS),no)
else
CFLAGS+=-DGPAC_HAS_SPIDERMONKEY $(JS_FLAGS)
ifeq ($(CONFIG_JS),local)
NEED_LOCAL_LIB="yes"
LOCAL_LIB+=-L../../extra_lib/lib/gcc
endif
LINKLIBS+= $(JS_LIBS)
endif

SRCS := $(OBJS:.o=.c) 

LIB=gm_gpac_js.$(DYN_LIB_SUFFIX)
ifeq ($(CONFIG_WIN32),yes)
LDFLAGS+=-export-symbols gpac_js.def 
endif

ifeq ($(WANT_PIC),yes)
OBJSPIC=$(OBJS:.o=.opic)
else
OBJSPIC=$(OBJS)
endif

all: $(LIB)


$(LIB): $(OBJSPIC)
	$(CC) $(SHFLAGS) $(LDFLAGS) -o ../../bin/gcc/$@ $(OBJSPIC) $(EXTRALIBS) -L$(LOCAL_LIB) $(LINKLIBS)


%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $< 


clean: 
	rm -f $(OBJS) ../../bin/gcc/$(LIB)

dep: depend

depend:
	rm -f .depend	
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

distclean: clean
	rm -f Makefile.bak .depend



# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
