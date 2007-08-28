include ../../config.mak

vpath %.c $(SRC_PATH)/modules/sdl_out

CFLAGS= $(OPTFLAGS) -Wall -I$(SRC_PATH)/include $(SDL_CFLAGS)

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

ifeq ($(CONFIG_LINUX),yes)
ifeq ($(X11_INC_PATH), )
else
CFLAGS+=-I$(X11_INC_PATH)
endif
endif

#common obj
OBJS=sdl_out.o audio.o video.o

SRCS := $(OBJS:.o=.c) 

LIB=gm_sdl_out.$(DYN_LIB_SUFFIX)
ifeq ($(CONFIG_WIN32),yes)
LDFLAGS+=-export-symbols sdl_out.def 
endif

ifeq ($(WANT_PIC),yes)
OBJSPIC=$(OBJS:.o=.opic)
else
OBJSPIC=$(OBJS)
endif

all: $(LIB)


$(LIB): $(OBJSPIC)
	$(CC) $(SHFLAGS) $(LDFLAGS) -o ../../bin/gcc/$@ $(OBJSPIC) $(SDL_LIBS) -L../../bin/gcc -lgpac

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
