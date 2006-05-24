include ../../config.mak

vpath %.c $(SRC_PATH)/src/odf

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
OBJS=desc_private.o descriptors.o ipmpx_code.o ipmpx_dump.o ipmpx_parse.o oci_codec.o odf_code.o \
	odf_codec.o odf_command.o 	odf_dump.o odf_parse.o qos.o slc.o 
	
SRCS := $(OBJS:.o=.c) 


LIB=../../bin/gcc/temp/libgpac_odf.a

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
