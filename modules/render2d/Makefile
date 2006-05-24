include ../../config.mak

vpath %.c $(SRC_PATH)/modules/render2d

CFLAGS= $(OPTFLAGS) -Wall -I$(SRC_PATH)/include

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

#common obj
OBJS= background2d.o drawable.o form.o geometry_stacks.o grouping.o grouping_stacks.o ifs2d.o ils2d.o layout.o path_layout.o render2d.o \
  render2d_nodes.o sensor_stacks.o sound.o text.o texture_stacks.o viewport.o visualsurface2d.o visualsurface2d_draw.o \
  svg_base.o svg_text.o svg_media.o

LINKFLAGS=-L../../bin/gcc -lgpac

ifeq ($(DISABLE_SVG), yes)
CFLAGS+=-DGPAC_DISABLE_SVG
endif


SRCS := $(OBJS:.o=.c) 

LIB=gm_render2d.$(DYN_LIB_SUFFIX)
ifeq ($(CONFIG_WIN32),yes)
LDFLAGS+=-export-symbols render2d.def 
endif

all: $(LIB)


$(LIB): $(OBJS)
	$(CC) $(SHFLAGS) $(LDFLAGS) -o ../../bin/gcc/$@ $(OBJS) $(LINKFLAGS) $(EXTRALIBS)


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
