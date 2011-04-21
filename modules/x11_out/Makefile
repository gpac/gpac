include ../../config.mak

vpath %.c $(SRC_PATH)/modules/x11_out

CFLAGS= $(OPTFLAGS)

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

CFLAGS+=-I"$(SRC_PATH)/include"
# -I/usr/local/arm/3.3.2/include


ifeq ($(X11_INC_PATH), )
else
CFLAGS+=-I$(X11_INC_PATH)
endif

ifeq ($(X11_LIB_PATH), )
else
LDFLAGS+=-L$(X11_LIB_PATH)
endif

ifeq ($(USE_X11_XV), yes)
CFLAGS+=-DGPAC_HAS_X11_XV
LDFLAGS+=-lXv
endif

ifeq ($(USE_X11_SHM), yes)
CFLAGS+=-DGPAC_HAS_X11_SHM
LDFLAGS+=-lXext
endif

ifeq ($(HAS_OPENGL), yes)
ifeq ($(GPAC_USE_TINYGL), yes)
else
CFLAGS+=$(OGL_INCLS)
LDFLAGS+=$(OGL_LIBS)
ifeq ($(CONFIG_DARWIN),yes)
LDFLAGS+=-lGL -lGLU
endif
endif
endif

#common obj
OBJS=x11_out.o

SRCS := $(OBJS:.o=.c) 

LIB=gm_x11_out.$(DYN_LIB_SUFFIX)
ifeq ($(CONFIG_WIN32),yes)
#LDFLAGS+=-export-symbols  
endif


all: $(LIB)


$(LIB): $(OBJS)
	$(CC) $(SHFLAGS) $(LDFLAGS) -lX11 -L../../bin/gcc -lgpac -o ../../bin/gcc/$@ $(OBJS)
ifeq ($(STATICBUILD),yes)
	$(CC) $(SHFLAGS) $(LDFLAGS) -o ../../bin/gcc/gm_x11_out-static.so $(OBJS) -lX11 -L../../bin/gcc -lgpac_static
endif

#static-bin:
#	$(CC) $(SHFLAGS) $(LDFLAGS) -o ../../bin/gcc/gm_x11_out-static.so $(OBJS) -lX11 -L../../bin/gcc -lgpac_static
                                                                                               
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
