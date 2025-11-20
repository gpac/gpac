include ../../config.mak

vpath %.c $(SRC_PATH)/modules/caca_out

CFLAGS=-I"$(SRC_PATH)/include" -I/opt/local/include $(OPTFLAGS) $(libcaca_cflags)

LINKFLAGS=-L../../bin/gcc -lgpac -L/opt/local/lib $(libcaca_ldflags)

include ../common.mak

#common obj
OBJS=caca_out.o
SRCS := $(OBJS:.o=.c)

LIB=gm_caca_out$(DYN_LIB_SUFFIX)


all: $(LIB)


$(LIB): $(OBJS)
	$(CC) $(SHFLAGS) -o ../../bin/gcc/$@ $(OBJS) $(LINKFLAGS) $(LDFLAGS)

clean:
	rm -f $(OBJS) ../../bin/gcc/$(LIB)

dep: depend

depend:
	rm -f .depend
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

distclean: clean
	rm -f Makefile.bak .depend

-include .depend
