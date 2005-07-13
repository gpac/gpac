include ../../config.mak

VPATH=$(SRC_PATH)/modules/render3d

CFLAGS= $(OPTFLAGS) -Wall -I$(SRC_PATH)/include

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif


#common obj
OBJS= background.o bindable.o bitmap.o camera.o form.o geometry_stacks.o geometry_x3d.o gradients.o grouping.o grouping_stacks.o hardcoded_protos.o \
  layers.o layout.o lighting.o mesh.o mesh_collide.o navigate.o path_layout.o render3d.o render3d_nodes.o sensor_stacks.o sound.o text.o \
  texture_stacks.o viewport.o visual_surface.o texturing_gl.o vs_gl_draw.o tesselate.o 

LINKFLAGS=-L../../bin/gcc -lgpac

SRCS := $(OBJS:.o=.c) 

LIB=render3d.$(DYN_LIB_SUFFIX)
ifeq ($(CONFIG_WIN32),yes)
LDFLAGS+=-export-symbols render3d.def 
else
endif

all: $(LIB)


$(LIB): $(OBJS)
	$(CC) $(SHFLAGS) $(LDFLAGS) -o ../../bin/gcc/$@ $(OBJS) $(LINKFLAGS) $(EXTRALIBS) $(OGL_LIBS)


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
