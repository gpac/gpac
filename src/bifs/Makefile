include ../../config.mak

vpath %.c $(SRC_PATH)/src/bifs

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
OBJS=arith_decoder.o bifs_codec.o bifs_node_tables.o com_dec.o com_enc.o conditional.o field_decode.o \
	field_encode.o memory_decoder.o predictive_mffield.o quantize.o script_dec.o script_enc.o unquantize.o


SRCS := $(OBJS:.o=.c) 


LIB=../../bin/gcc/temp/libgpac_bifs.a

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
