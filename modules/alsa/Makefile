include ../../config.mak

vpath %.c $(SRC_PATH)/modules/alsa

CFLAGS= $(OPTFLAGS) -I$(SRC_PATH)/include $(OSS_CFLAGS)
LDFLAGS+=$(OSS_LDFLAGS)

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

#common obj
OBJS= alsa.o

SRCS := $(OBJS:.o=.c) 

LIB=gm_alsa.$(DYN_LIB_SUFFIX)

ifeq ($(WANT_PIC),yes)
OBJSPIC=$(OBJS:.o=.opic)
else
OBJSPIC=$(OBJS)
endif

all: $(LIB)

$(LIB): $(OBJSPIC)
	$(CC) $(SHFLAGS) -o ../../bin/gcc/$@ $(OBJSPIC) $(EXTRALIBS) -L../../bin/gcc -lgpac -lasound


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

