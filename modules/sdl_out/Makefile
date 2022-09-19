include ../../config.mak

vpath %.c $(SRC_PATH)/modules/sdl_out

CFLAGS=-I"$(SRC_PATH)/include" $(OPTFLAGS) $(sdl_cflags)

ifeq ($(DEBUGBUILD),yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD),yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

LINKFLAGS=-L../../bin/gcc -lgpac $(sdl_ldflags)

ifeq ($(CONFIG_DARWIN),yes)
LINKFLAGS+=-framework OpenGL
endif

#common obj
OBJS=sdl_out.o audio.o video.o

SRCS := $(OBJS:.o=.c) 

LIB=gm_sdl_out$(DYN_LIB_SUFFIX)
ifeq ($(CONFIG_WIN32),yes)
#LDFLAGS+=-export-symbols sdl_out.def 
endif


all: $(LIB)


$(LIB): $(OBJS)
	$(CC) $(SHFLAGS) -o ../../bin/gcc/$@ $(OBJS) $(LINKFLAGS) $(LDFLAGS)

clean: 
	rm -f $(OBJS) ../../bin/gcc/$(LIB)

dep: depend

depend:
	rm -f .depend	
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

distclean: clean
	rm -f Makefile.bak .depend

-include .depend
