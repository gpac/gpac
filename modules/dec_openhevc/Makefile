include ../../config.mak

vpath %.c $(SRC_PATH)/modules/dec_openhevc

CFLAGS= $(OPTFLAGS) -I"$(SRC_PATH)/include"

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif


LINKFLAGS+=-L../../bin/gcc $(OHEVC_LDFLAGS)


#common obj
OBJS=dec_openhevc.o
SRCS=$(SRC_PATH)/src/filters/dec_openhevc.c


ifeq ($(CONFIG_WIN32), yes)
else
EXTRALIBS+=-ldl
endif


LIB=gf_openhevc_dec$(DYN_LIB_SUFFIX)

all: $(LIB)

dec_openhevc.o : $(SRC_PATH)/src/filters/dec_openhevc.c
	@echo "  CC $<"
	$(CC) $(CFLAGS) -c -o $@ $<


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
