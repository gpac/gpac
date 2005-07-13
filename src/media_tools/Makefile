include ../../config.mak

VPATH=$(SRC_PATH)/src/media_tools

CFLAGS= $(OPTFLAGS) -Wall -I$(SRC_PATH)/include

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

#file format is read-only
ifeq ($(GPACREADONLY), yes)
CFLAGS+= -DGPAC_READ_ONLY
endif


#big-endian config
ifeq ($(IS_BIGENDIAN), yes)
CFLAGS+=-DGPAC_BIG_ENDIAN
endif

#common obj
OBJS=av_parsers.o avilib.o gpac_ogg.o ismacryp.o isom_hinter.o isom_tools.o media_export.o media_import.o mpeg2_ps.o text_import.o


SRCS := $(OBJS:.o=.c) 

LIB=../../bin/gcc/temp/libgpac_media.a

all: $(LIB)


$(LIB): $(OBJS)
	$(AR) rc $@ $(OBJS)
	$(RANLIB) $@


%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $< 


clean: 
	rm -f *.o $(LIB)

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
