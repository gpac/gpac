include ../../config.mak

VPATH=$(SRC_PATH)/modules/svg_loader

CFLAGS= $(OPTFLAGS) -Wall -I$(SRC_PATH)/include -I$(VPATH)/svg_parser

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

#common obj
OBJS= svg_loader.o svg_parser.o lsr_parser.o

SRCS := $(OBJS:.o=.c) 

CFLAGS+=$(XML2_CFLAGS)

LOCAL_LIB=
ifeq ($(CONFIG_ZLIB), local)
CFLAGS+= -I../../extra_lib/include/zlib
LOCAL_LIB+=-L../../extra_lib/lib/gcc
endif

LIB=svg_loader.$(DYN_LIB_SUFFIX)
ifeq ($(CONFIG_WIN32),yes)
LDFLAGS+=-export-symbols svg_loader.def 
endif

all: $(LIB)


$(LIB): $(OBJS)
	$(CC) $(SHFLAGS) $(LDFLAGS) -o ../../bin/gcc/$@ $(OBJS) -L../../bin/gcc -lgpac $(XML2_LFLAGS) $(LOCAL_LIB)


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
