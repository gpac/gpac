include ../../config.mak

vpath %.c $(SRC_PATH)/modules/amr_dec

#note: __MSDOS__ defined is the same cfg as linux. There may be some pbs on other platforms, to check...
CFLAGS= $(OPTFLAGS) -w -I"$(SRC_PATH)/include" -D__MSDOS__ -DMMS_IO -DWMOPS=0 -DVAD1


ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g -DDEBUG
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

#common obj
OBJS=amr_in.o
NAME=amr_in
AMRLIBS=

ifeq ($(CONFIG_AMR_NB), yes)
NAME=amr_dec
OBJS+= amr_dec.o
AMRLIBS=-L../../extra_lib/lib/gcc -lopencore-amrnb -lopencore-amrwb

else

CFLAGS+=-DGPAC_AMR_IN_STANDALONE

endif


SRCS := $(OBJS:.o=.c)

LIB=gm_$(NAME)$(DYN_LIB_SUFFIX)

all: $(LIB)


$(LIB): $(OBJS)
	$(CC) $(SHFLAGS) $(CFLAGS) -o ../../bin/gcc/$@ $(OBJS) $(EXTRALIBS) -L../../bin/gcc $(AMRLIBS) -lgpac $(LDFLAGS)


clean:
	rm -f $(OBJS) ../../bin/gcc/$(LIB)

dep: depend

depend:
	rm -f .depend
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

distclean: clean
	rm -f Makefile.bak .depend

-include .depend
