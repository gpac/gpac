include ../../config.mak

VPATH=$(SRC_PATH)/src/utils

CFLAGS= $(OPTFLAGS) -Wall -I$(SRC_PATH)/include

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif


#common obj
OBJS=os_divers.o os_net.o os_module.o os_thread.o list.o base_encoding.o bitstream.o color.o configfile.o downloader.o error.o math.o path2d.o path2d_stroker.o module.o token.o url.o utf.o xml_parser.o


ifeq ($(HAS_OPENSSL), yes)
CFLAGS+=-DGPAC_HAS_SSL
endif

ifeq ($(CONFIG_IPV6), yes)
CFLAGS+=-DGPAC_IPV6
endif

SRCS := $(OBJS:.o=.c) 

LIB=../../bin/gcc/temp/libgpac_utils.a

all: $(LIB)


$(LIB): $(OBJS)
	$(AR) rc $@ $(OBJS)
	$(RANLIB) $@

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $< 


clean: 
	rm -f $(OBJS) $(LIB)

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
