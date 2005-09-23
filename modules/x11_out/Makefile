include ../../config.mak

VPATH=$(SRC_PATH)/modules/x11_out

CFLAGS= $(OPTFLAGS) -Wall -I$(SRC_PATH)/include
# -I/usr/local/arm/3.3.2/include


ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(X11_LIB_PATH), )
else
LDFLAGS+=-L$(X11_LIB_PATH)
endif

ifeq ($(USE_X11_SHM), yes)
CFLAGS+=-DGPAC_HAS_X11_SHM
LDFLAGS+=-lXext
endif


ifeq ($(HAS_OPENGL), yes)
CFLAGS +=-DGPAC_HAS_OPENGL
LDFLAGS+=-lGL
endif

#common obj
OBJS=x11_out.o

SRCS := $(OBJS:.o=.c) 

LIB=gm_x11_out.$(DYN_LIB_SUFFIX)
ifeq ($(CONFIG_WIN32),yes)
LDFLAGS+=-export-symbols  
endif

all: $(LIB)


$(LIB): $(OBJS)
	$(CC) $(SHFLAGS) $(LDFLAGS) -o ../../bin/gcc/$@ $(OBJS) -lX11 -L../../bin/gcc -lgpac

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
