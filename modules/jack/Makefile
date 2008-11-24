include ../../config.mak

vpath %.c $(SRC_PATH)/modules/jack

CFLAGS= $(OPTFLAGS) -I$(SRC_PATH)/include $(OSS_CFLAGS)

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

#common obj
OBJS= jack.o

SRCS := $(OBJS:.o=.c) 


ifeq ($(WANT_PIC),yes)
OBJSPIC=$(OBJS:.o=.opic)
else
OBJSPIC=$(OBJS)
endif


LIB=gm_jack.$(DYN_LIB_SUFFIX)

all: $(LIB)

$(LIB): $(OBJSPIC)
	$(CC) $(SHFLAGS) -o ../../bin/gcc/$@ $(OBJSPIC) $(EXTRALIBS) -L../../bin/gcc -lgpac -L/usr/lib  -ljack


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

