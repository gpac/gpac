include ../../config.mak

vpath %.c $(SRC_PATH)/modules/directfb_out

CFLAGS= $(OPTFLAGS)
CFLAGS+=-I"$(SRC_PATH)/include" -I$(DIRECTFB_INC_PATH)
LDFLAGS+=$(DIRECTFB_LIB)

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif




#common obj
OBJS=directfb_out.o

SRCS := $(OBJS:.o=.c) 

LIB=gm_directfb_out.$(DYN_LIB_SUFFIX)


all: $(LIB)


$(LIB): $(OBJS)
	$(CC) $(SHFLAGS) $(LDFLAGS) -L../../bin/gcc -lgpac -o ../../bin/gcc/$@ $(OBJS)
                                                                                               
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
