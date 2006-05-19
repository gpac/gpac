include ../config.mak

VPATH=$(SRC_PATH)/src

outlibs=libgpac_bifs.a libgpac_terminal.a libgpac_ietf.a libgpac_isom.a libgpac_odf.a libgpac_scenegraph.a libgpac_scene_manager.a libgpac_media.a libgpac_utils.a libgpac_render.a libgpac_mcrypt.a

#all OS and lib independent
GPACLIB_DIRS=utils odf ietf isomedia mcrypt media_tools bifs terminal renderer scenegraph scene_manager

ifeq ($(DISABLE_SVG), no)
outlibs+= libgpac_laser.a
GPACLIB_DIRS+= laser
endif


ifeq ($(CONFIG_JS),no)
else
ifeq ($(CONFIG_JS),local)
EXTRALIBS+= -L../extra_lib/lib/gcc
endif
EXTRALIBS+= -l$(JS_LIBS)
endif

ifeq ($(CONFIG_ZLIB), local)
CFLAGS+= -I../extra_lib/include/zlib
EXTRALIBS+=-L../extra_lib/lib/gcc
endif
EXTRALIBS+=-lz

ifeq ($(HAS_OPENSSL), yes)
EXTRALIBS+=$(SSL_LIBS)
endif


LIB=libgpac.$(DYN_LIB_SUFFIX)
ifeq ($(CONFIG_WIN32),yes)
#LDFLAGS+=-export-symbols libgpac.def
else
ifeq ($(CONFIG_DARWIN),yes)
LDFLAGS+=-install_name $(prefix)/lib/$(LIB)
endif
EXTRALIBS+=$(GPAC_SH_FLAGS)
endif

all: lib

lib: $(LIB)

$(LIB):
	rm -f ../bin/gcc/libgpac_static.a; 
	rm -f ../bin/gcc/$(LIB)
	for thelib in $(outlibs);	do rm -f ../bin/gcc/temp/$$thelib;	done
	set -e; for i in $(GPACLIB_DIRS);	do $(MAKE) -C $$i all; done
	for thelib in $(outlibs);	do ar x ../bin/gcc/temp/$$thelib;	done
	ar cr ../bin/gcc/libgpac_static.a *.o
	ranlib ../bin/gcc/libgpac_static.a
	$(CC) $(SHFLAGS) $(LDFLAGS) -o ../bin/gcc/$@ *.o $(EXTRALIBS)
	rm -f *.o

dep:
	for i in $(GPACLIB_DIRS) ; do $(MAKE) -C $$i dep; done

clean: 
	set -e; for i in $(GPACLIB_DIRS) ; do $(MAKE) -C $$i clean; done
	rm -rf ../bin/gcc/libgpac_static.a
	rm -rf ../bin/gcc/$(LIB)

distclean: 
	set -e; for i in $(GPACLIB_DIRS) ; do $(MAKE) -C $$i distclean; done
