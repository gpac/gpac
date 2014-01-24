include ../../config.mak

vpath %.c $(SRC_PATH)/modules/ft_font

CFLAGS= $(OPTFLAGS) -I"$(SRC_PATH)/include" $(FT_CFLAGS)

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
#LDFLAGS+=-export-symbols ft_font.def 
endif


ifneq ($(STATICBUILD),yes)
LINKVAR=-L../../bin/gcc -lgpac $(FT_LIBS)
else
LINKVAR=-L../../bin/gcc -lgpac_static $(FT_LIBS)
endif


all: $(LIB)


$(LIB): $(OBJS)
	$(CC) $(SHFLAGS) $(LDFLAGS) -o ../../bin/gcc/$@ $(OBJS) $(LINKVAR) $(EXTRALIBS)
ifeq ($(STATICBUILD),yes)
	$(CC) $(SHFLAGS) $(LDFLAGS) -o ../../bin/gcc/gm_ft_font-static.$(DYN_LIB_SUFFIX) $(OBJS) $(LINKVAR) $(EXTRALIBS)
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
