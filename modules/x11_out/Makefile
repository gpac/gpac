include ../../config.mak

vpath %.c $(SRC_PATH)/modules/x11_out

CFLAGS=-I"$(SRC_PATH)/include" $(OPTFLAGS) -Wno-deprecated-declarations

ifeq ($(DEBUGBUILD),yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD),yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

CFLAGS+=-I"$(SRC_PATH)/include"


ifeq ($(X11_INC_PATH),)
else
CFLAGS+=-I$(X11_INC_PATH)
endif

ifeq ($(X11_LIB_PATH),)
else
EXTRALIBS+=-L$(X11_LIB_PATH)
endif

ifeq ($(USE_X11_XV),yes)
CFLAGS+=-DGPAC_HAS_X11_XV
EXTRALIBS+=-lXv
endif

ifeq ($(USE_X11_SHM),yes)
CFLAGS+=-DGPAC_HAS_X11_SHM
EXTRALIBS+=-lXext
endif

ifeq ($(HAS_OPENGL),yes)
ifeq ($(GPAC_USE_TINYGL),yes)
else
CFLAGS+=$(OGL_INCLS)
EXTRALIBS+=$(OGL_LIBS)
ifeq ($(CONFIG_DARWIN),yes)
EXTRALIBS+=-framework OpenGL
endif

ifeq ($(USE_X11_GLX),yes)
CFLAGS+=-DGPAC_HAS_X11_GLX
endif

endif
endif

#common obj
OBJS=x11_out.o

SRCS := $(OBJS:.o=.c) 

LIB=gm_x11_out$(DYN_LIB_SUFFIX)
ifeq ($(CONFIG_WIN32),yes)
#LDFLAGS+=-export-symbols  
endif


all: $(LIB)


$(LIB): $(OBJS)
	$(CC) $(SHFLAGS) -o ../../bin/gcc/$@ $(OBJS) -lX11 -L../../bin/gcc -lgpac $(EXTRALIBS) $(LDFLAGS)
ifeq ($(STATICBUILD),yes)
	$(CC) $(SHFLAGS) -o ../../bin/gcc/gm_x11_out-static$(DYN_LIB_SUFFIX) $(OBJS) -lX11 -L../../bin/gcc -lgpac_static $(EXTRALIBS) $(LDFLAGS)
endif


clean: 
	rm -f $(OBJS) ../../bin/gcc/$(LIB)

dep: depend

depend:
	rm -f .depend	
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

distclean: clean
	rm -f Makefile.bak .depend

-include .depend
