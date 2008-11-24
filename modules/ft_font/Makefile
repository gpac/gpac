include ../../config.mak

vpath %.c $(SRC_PATH)/modules/ft_font

CFLAGS= $(OPTFLAGS) -I$(SRC_PATH)/include $(FT_CFLAGS)

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

#common obj
OBJS= ft_font.o 

SRCS := $(OBJS:.o=.c) 

LIB=gm_ft_font.$(DYN_LIB_SUFFIX)
ifeq ($(CONFIG_WIN32),yes)
LDFLAGS+=-export-symbols ft_font.def 
endif


LINKVAR=-L../../bin/gcc -lgpac $(FT_LIBS)


ifeq ($(WANT_PIC),yes)
OBJSPIC=$(OBJS:.o=.opic)
else
OBJSPIC=$(OBJS)
endif

all: $(LIB)


$(LIB): $(OBJSPIC)
	$(CC) $(SHFLAGS) $(LDFLAGS) -o ../../bin/gcc/$@ $(OBJSPIC) $(LINKVAR) $(EXTRALIBS)


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
