include ../../config.mak

vpath %.c $(SRC_PATH)/modules/xvid_dec

CFLAGS= $(OPTFLAGS) -I$(SRC_PATH)/include

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

#common obj
OBJS=xvid_dec.o

#local xvid lib
ifeq ($(CONFIG_XVID), local)
CFLAGS+= -I$(LOCAL_INC_PATH)/xvid
EXTRALIBS+= -L../../extra_lib/lib/gcc
endif
EXTRALIBS+= -lxvidcore

SRCS := $(OBJS:.o=.c) 

LIB=gm_xvid_dec.$(DYN_LIB_SUFFIX)
ifeq ($(CONFIG_WIN32),yes)
LDFLAGS+=-export-symbols xvid_dec.def 
endif

ifeq ($(WANT_PIC),yes)
OBJSPIC=$(OBJS:.o=.opic)
else
OBJSPIC=$(OBJS)
endif

all: $(LIB)


$(LIB): $(OBJSPIC)
	$(CC) $(SHFLAGS) $(LDFLAGS) -o ../../bin/gcc/$@ $(OBJSPIC) -L../../bin/gcc -lgpac $(EXTRALIBS)


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
