include ../../config.mak

vpath %.c $(SRC_PATH)/modules/openhevc_dec

CFLAGS=$(OPTFLAGS) -I"$(SRC_PATH)/include" $(OHEVC_CFLAGS)

ifeq ($(CONFIG_LINUX), yes)
LDFLAGS+=-Wl,-Bsymbolic
endif

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

LINKFLAGS+=-L../../bin/gcc $(OHEVC_LDFLAGS)

ifeq ($(STATICBUILD),yes)
##include static modules and other deps for libgpac
include ../../static.mak
LINKFLAGS+=-lgpac_static
LINKFLAGS+=$(EXTRALIBS)
else
LINKFLAGS+=-lgpac
endif

#common obj
OBJS= openhevc_dec.o

SRCS := $(OBJS:.o=.c) 

LIB=gm_openhevc_dec$(DYN_LIB_SUFFIX)


all: $(LIB)


$(LIB): $(OBJS)
	$(CC) $(SHFLAGS) -o ../../bin/gcc/$@ $(OBJS) $(LINKFLAGS) $(LDFLAGS)

clean: 
	rm -f $(OBJS) ../../bin/gcc/$(LIB)

dep: depend

depend:
	rm -f .depend	
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

distclean: clean
	rm -f Makefile.bak .depend

-include .depend
