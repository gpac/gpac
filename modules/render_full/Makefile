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

LINKLIBS= $(EXTRALIBS)
ifeq ($(HAS_OPENGL),yes)
LINKLIBS+= $(OGL_LIBS)
else
CFLAGS+=-DGPAC_DISABLE_3D
endif

#common obj
OBJS=background.o background2d.o bindable.o bitmap.o camera.o composite_texture.o drawable.o events.o form.o geometry_2d.o \
      geometry_3d.o geometry_ifs2d.o geometry_ils2d.o geometry_x3d.o gradients.o grouping.o grouping_stacks_2d.o grouping_stacks_3d.o \
      hardcoded_protos.o layer_2d.o layer_3d.o layout.o lighting.o mesh.o mesh_collide.o mesh_tesselate.o navigate.o path_layout.o \
      render.o render_2d.o render_3d.o render_nodes.o sensor_stacks.o sound.o svg_base.o svg_media.o svg_paint_servers.o svg_text.o \
      text.o texturing_gl.o viewport.o visual.o visual_2d.o visual_2d_draw.o visual_3d.o visual_3d_gl.o

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

LIB=gm_render.$(DYN_LIB_SUFFIX)
ifeq ($(CONFIG_WIN32),yes)
LDFLAGS+=-export-symbols render.def 
else
endif

ifeq ($(WANT_PIC),yes)
OBJSPIC=$(OBJS:.o=.opic)
else
OBJSPIC=$(OBJS)
endif

all: $(LIB)


$(LIB): $(OBJSPIC)
	$(CC) $(SHFLAGS) $(LDFLAGS) -o ../../bin/gcc/$@ $(OBJSPIC) $(LINKFLAGS) $(LINKLIBS)


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
