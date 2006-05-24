include ../../config.mak

vpath %.c $(SRC_PATH)/src/laser

CFLAGS= $(OPTFLAGS) -Wall -I$(SRC_PATH)/include

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

#common obj
OBJS=lsr_enc.o lsr_dec.o lsr_tables.o


SRCS := $(OBJS:.o=.c) 


LIB=../../bin/gcc/temp/libgpac_laser.a

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
