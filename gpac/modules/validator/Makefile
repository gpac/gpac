include ../../config.mak

vpath %.c $(SRC_PATH)/modules/validator

CFLAGS=-I"$(SRC_PATH)/include" $(OPTFLAGS)

ifeq ($(DEBUGBUILD),yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD),yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

#common obj
OBJS=validator.o

SRCS := $(OBJS:.o=.c) 

LIB=gm_validator$(DYN_LIB_SUFFIX)
ifeq ($(CONFIG_WIN32),yes)
endif


all: $(LIB)


$(LIB): $(OBJS)
	$(CC) $(SHFLAGS) -o ../../bin/gcc/$@ $(OBJS) $(EXTRALIBS) -L../../bin/gcc -lgpac $(LDFLAGS)
ifeq ($(STATIC_BUILD),yes)
	$(CC) $(SHFLAGS) -o ../../bin/gcc/gm_validator-static$(DYN_LIB_SUFFIX) $(OBJS) -L../../bin/gcc -lgpac_static $(LDFLAGS)
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
