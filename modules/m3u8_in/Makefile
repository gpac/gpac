include ../../config.mak

vpath %.c $(SRC_PATH)/modules/m3u8_in

CFLAGS= $(OPTFLAGS) -I$(SRC_PATH)/include -g -Wall -pedantic

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

#common obj
OBJS= m3u8_in.o m3u8_in_load.o playlist.o m3u8_parser.o

SRCS := $(OBJS:.o=.c) 

NEED_LOCAL_LIB="no"
LOCAL_LIB=../../bin/gcc
LINKLIBS= -lgpac 

ifeq ($(CONFIG_OGG), local)
NEED_LOCAL_LIB="yes"
endif


#add local lib path
ifeq ($(NEED_LOCAL_LIB), "yes")
LOCAL_LIB+=-L../../extra_lib/lib/gcc
endif


LIB=gm_m3u8_in.$(DYN_LIB_SUFFIX)
ifeq ($(CONFIG_WIN32),yes)
#LDFLAGS+=-export-symbols m3u8_in.def 
endif


all: $(LIB)


$(LIB): $(OBJS)
	$(CC) $(SHFLAGS) $(LDFLAGS) -o ../../bin/gcc/$@ $(OBJS) $(EXTRALIBS) -L$(LOCAL_LIB) $(LINKLIBS)


%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $< 

tests: all test.o
	$(CC) $(LDFLAGS) -o test $(OBJS) test.o $(EXTRALIBS) -L$(LOCAL_LIB) $(LINKLIBS) && ./test
clean: 
	rm -f $(OBJS) test test.o ../../bin/gcc/$(LIB)

dep: depend

depend:
	rm -f .depend	
	GS)

distclean: clean
	rm -f Makefile.bak .depend



# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
