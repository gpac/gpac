include ../../config.mak

vpath %.c $(SRC_PATH)/src/terminal

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
OBJS=channel.o clock.o decoder.o term_node_init.o inline.o input_sensor.o media_control.o media_manager.o \
	media_memory.o media_object.o media_sensor.o network_service.o object_browser.o object_manager.o terminal.o 


SRCS := $(OBJS:.o=.c) 


LIB=../../bin/gcc/temp/libgpac_terminal.a

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
