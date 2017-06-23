include ../../config.mak

vpath %.c $(SRC_PATH)/modules/nvdec

CFLAGS= $(OPTFLAGS) -I"$(SRC_PATH)/include"

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

#common obj
OBJS=nvdec.o cuda_sdk.o

SRCS := $(OBJS:.o=.c) 


ifeq ($(CONFIG_WIN32), yes)
else
EXTRALIBS+=-ldl
endif

ifeq ($(HAS_OPENGL), yes)
CFLAGS+=$(OGL_INCLS)
EXTRALIBS+=$(OGL_LIBS)

ifeq ($(CONFIG_DARWIN),yes)
EXTRALIBS+=-lGL -lGLU
endif

endif


LIB=gm_nvdec$(DYN_LIB_SUFFIX)

all: $(LIB)


$(LIB): $(OBJS)
	$(CC) $(SHFLAGS) -o ../../bin/gcc/$@ $(OBJS) -L../../bin/gcc -lgpac $(EXTRALIBS) $(LDFLAGS)
ifeq ($(STATICBUILD),yes)
	$(CC) $(SHFLAGS) -o ../../bin/gcc/gm_nvdec-static$(DYN_LIB_SUFFIX) $(OBJS) -L../../bin/gcc -lgpac_static $(EXTRALIBS) $(LDFLAGS)
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
