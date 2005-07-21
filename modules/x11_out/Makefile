include ../../config.mak

VPATH=$(SRC_PATH)/modules/x11_out

CFLAGS= $(OPTFLAGS) -Wall -I$(SRC_PATH)/include -I/usr/X11R6/include

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

#common obj
OBJS=x11_out.o x11_softstretch.o

SRCS := $(OBJS:.o=.c) 

LIB=x11_out.$(DYN_LIB_SUFFIX)
ifeq ($(CONFIG_WIN32),yes)
LDFLAGS+=-export-symbols  
endif

all: $(LIB)


$(LIB): $(OBJS)
	$(CC) $(SHFLAGS) $(LDFLAGS) -o ../../bin/gcc/$@ $(OBJS) -L/usr/X11R6/lib -lX11 -lXext -L../../bin/gcc -lgpac

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
