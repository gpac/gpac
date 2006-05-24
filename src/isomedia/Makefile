include ../../config.mak

vpath %.c $(SRC_PATH)/src/isomedia

CFLAGS= $(OPTFLAGS) -Wall -I$(SRC_PATH)/include

#movie fragments enabled
ifeq ($(ISOFRAGMENTS), no)
CFLAGS+= -DGF_ISOM_NO_FRAGMENTS
endif

#file format is read-only
ifeq ($(GPACREADONLY), yes)
CFLAGS+= -DGPAC_READ_ONLY
endif

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

#common obj
OBJS=avc_ext.o box_code_3gpp.o box_code_base.o box_code_isma.o box_code_meta.o box_dump.o box_funcs.o data_map.o hint_track.o hinting.o \
  isma_sample.o isom_intern.o isom_read.o isom_store.o isom_write.o media.o media_odf.o meta.o movie_fragments.o sample_descs.o stbl_read.o \
  stbl_write.o track.o tx3g.o 
  
  
SRCS := $(OBJS:.o=.c) 


LIB=../../bin/gcc/temp/libgpac_isom.a

all: $(LIB)


$(LIB): $(OBJS)
	$(AR) rc $@ $(OBJS)
	$(RANLIB) $@


%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $< 



dep: depend

depend:
	rm -f .depend
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend


clean: 
	rm -f *.o $(LIB)

distclean: clean
	rm -f Makefile.bak .depend



# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
