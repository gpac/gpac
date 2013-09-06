include ../../config.mak

vpath %.c $(SRC_PATH)/modules/mse_in

CFLAGS= $(OPTFLAGS) -w -I"$(SRC_PATH)/include" -Wall

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g -DDEBUG
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

#libsj specific flags
ifeq ($(CONFIG_JS),no)
else
CFLAGS+=$(JS_FLAGS)
ifeq ($(CONFIG_JS),local)
NEED_LOCAL_LIB="yes"
LOCAL_LIB+=-L../../extra_lib/lib/gcc
endif
ifeq ($(CONFIG_JS),prefix)
NEED_LOCAL_LIB="yes"
LOCAL_LIB+=-L$(prefix)/lib
endif
LINKLIBS+= $(JS_LIBS)
endif

#common obj
OBJS=mse_in.o

SRCS := $(OBJS:.o=.c) 

LIB=gm_mse_in.$(DYN_LIB_SUFFIX)

all: $(LIB)

$(LIB): $(OBJS)
	$(CC) $(SHFLAGS) $(LDFLAGS) -o ../../bin/gcc/$@ $(OBJS) $(EXTRALIBS) -L../../bin/gcc -lgpac
ifeq ($(STATICBUILD),yes)
	$(CC) $(SHFLAGS) $(LDFLAGS) -o ../../bin/gcc/gm_mse_in-static.$(DYN_LIB_SUFFIX) $(OBJS) $(EXTRALIBS) -L../../bin/gcc -lgpac_static
endif

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
