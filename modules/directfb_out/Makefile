include ../../config.mak

vpath %.c $(SRC_PATH)/modules/directfb_out

CFLAGS=-I"$(SRC_PATH)/include" -I$(DIRECTFB_INC_PATH) $(OPTFLAGS)
LDFLAGS+=$(DIRECTFB_LIB)

ifeq ($(DEBUGBUILD),yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD),yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif




#common obj
OBJS=directfb_out.o directfb_wrapper.o

SRCS := $(OBJS:.o=.c) 

LIB=gm_directfb_out$(DYN_LIB_SUFFIX)


all: $(LIB)


$(LIB): $(OBJS)
	$(CC) $(SHFLAGS) -o ../../bin/gcc/$@ $(OBJS) -L../../bin/gcc -lgpac $(LDFLAGS)

clean: 
	rm -f $(OBJS) ../../bin/gcc/$(LIB)

dep: depend

depend:
	rm -f .depend	
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

distclean: clean
	rm -f Makefile.bak .depend

-include .depend
