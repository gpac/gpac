include ../../config.mak

vpath %.c $(SRC_PATH)/modules/ft_font

CFLAGS=-I"$(SRC_PATH)/include" $(OPTFLAGS) $(freetype_cflags)

ifeq ($(DEBUGBUILD),yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD),yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

#common obj
OBJS= ft_font.o 

SRCS := $(OBJS:.o=.c) 

LIB=gm_ft_font$(DYN_LIB_SUFFIX)
ifeq ($(CONFIG_WIN32),yes)
#LDFLAGS+=-export-symbols ft_font.def 
endif


ifneq ($(STATIC_BUILD),yes)
LINKVAR=-L../../bin/gcc -lgpac $(freetype_ldflags)
else
LINKVAR=-L../../bin/gcc -lgpac_static $(freetype_ldflags)
endif


all: $(LIB)


$(LIB): $(OBJS)
	$(CC) $(SHFLAGS) -o ../../bin/gcc/$@ $(OBJS) $(LINKVAR) $(EXTRALIBS) $(LDFLAGS)
ifeq ($(STATIC_BUILD),yes)
	$(CC) $(SHFLAGS) -o ../../bin/gcc/gm_ft_font-static$(DYN_LIB_SUFFIX) $(OBJS) $(LINKVAR) $(EXTRALIBS) $(LDFLAGS)
endif


clean: 
	rm -f $(OBJS) ../../bin/gcc/$(LIB)

dep: depend

depend:
	rm -f .depend	
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

distclean: clean
	rm -f Makefile.bak .depend

-include .depend
