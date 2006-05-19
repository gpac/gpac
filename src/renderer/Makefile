include ../../config.mak

VPATH=$(SRC_PATH)/src/renderer

CFLAGS= $(OPTFLAGS) -Wall -I$(SRC_PATH)/include

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(DISABLE_SVG), yes)
CFLAGS+=-DGPAC_DISABLE_SVG
endif

#common obj
OBJS=audio_input.o audio_mixer.o audio_render.o audio_stacks.o base_textures.o common_stacks.o renderer.o texturing.o 

SRCS := $(OBJS:.o=.c) 


LIB=../../bin/gcc/temp/libgpac_render.a

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
