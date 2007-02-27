include ../../config.mak

vpath %.c $(SRC_PATH)/modules/render3d

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
OBJS= background.o bindable.o bitmap.o camera.o form.o geometry_stacks.o geometry_x3d.o gradients.o grouping.o grouping_stacks.o hardcoded_protos.o \
  layers.o layout.o lighting.o mesh.o mesh_collide.o navigate.o path_layout.o render3d.o render3d_nodes.o sensor_stacks.o sound.o text.o \
  texture_stacks.o viewport.o visual_surface.o texturing_gl.o vs_gl_draw.o tesselate.o 

LINKFLAGS=-L../../bin/gcc -lgpac

#update for MacOSX, needs path to X11 
ifeq ($(CONFIG_DARWIN),yes)

ifeq ($(X11_INC_PATH), )
else
CFLAGS+=-I$(X11_INC_PATH)
endif

ifeq ($(X11_LIB_PATH), )
else
LDFLAGS+=-L$(X11_LIB_PATH)
endif

endif

SRCS := $(OBJS:.o=.c) 

LIB=gm_render3d.$(DYN_LIB_SUFFIX)
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
