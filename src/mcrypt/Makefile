include ../../config.mak

vpath %.c $(SRC_PATH)/src/mcrypt

#there's a bunch of warnings in there, get rid of them
CFLAGS= $(OPTFLAGS) -w -I$(SRC_PATH)/include

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

#common obj
OBJS= cbc.o \
	cfb.o \
	ctr.o \
	des.o \
	ecb.o \
	g_crypt.o \
	ncfb.o \
	nofb.o \
	ofb.o \
	rijndael-128.o \
	rijndael-192.o \
	rijndael-256.o \
	stream.o \
	tripledes.o 


SRCS := $(OBJS:.o=.c) 


LIB=../../bin/gcc/temp/libgpac_mcrypt.a

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
