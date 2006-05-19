include ../../config.mak

VPATH=$(SRC_PATH)/src/scene_manager

CFLAGS= $(OPTFLAGS) -Wall -I$(SRC_PATH)/include

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

#file format is read-only
ifeq ($(GPACREADONLY), yes)
CFLAGS+= -DGPAC_READ_ONLY
endif

#common obj
OBJS=loader_bt.o loader_isom.o loader_qt.o loader_xmt.o scene_dump.o scene_manager.o scene_stats.o swf_parse.o swf_shape.o text_to_bifs.o encode_cbk.o encode_isom.o 

ifeq ($(DISABLE_SVG), yes)
CFLAGS+=-DGPAC_DISABLE_SVG
else
OBJS+=loader_svg.o
endif

SRCS := $(OBJS:.o=.c) 

LIB=../../bin/gcc/temp/libgpac_scene_manager.a

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
