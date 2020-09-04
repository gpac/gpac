include ../../config.mak

vpath %.c $(SRC_PATH)/modules/wav_out

CFLAGS=-I"$(SRC_PATH)/include" $(OPTFLAGS) -DDISABLE_WAVE_EX

ifeq ($(DEBUGBUILD),yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD),yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

#common obj
OBJS=wav_out.o


SRCS := $(OBJS:.o=.c) 

LIB=gm_wav_audio$(DYN_LIB_SUFFIX)
#LDFLAGS+=-export-symbols wav_out.def 

all: $(LIB)


$(LIB): $(OBJS)
	$(CC) $(SHFLAGS) -o ../../bin/gcc/$@ $(OBJS) -L../../bin/gcc -lgpac $(EXTRALIBS)  $(LDFLAGS)

clean: 
	rm -f $(OBJS) ../../bin/gcc/$(LIB)

dep: depend

depend:
	rm -f .depend	
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

distclean: clean
	rm -f Makefile.bak .depend

-include .depend
