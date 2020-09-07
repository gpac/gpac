include ../../config.mak

vpath %.c $(SRC_PATH)/applications/dashcast

CFLAGS= $(OPTFLAGS) -D_GNU_SOURCE -Wno-deprecated-declarations -I"$(SRC_PATH)/include" -I../../ $(ffmpeg_cflags)

LINKFLAGS=$(GPAC_SH_FLAGS)

ifeq ($(DEBUGBUILD),yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD),yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

ifeq ($(GPACREADONLY),yes)
CFLAGS+=-DGPAC_READ_ONLY
endif

ifneq ($(CONFIG_LIBAV),no)
CFLAGS+=-DGPAC_USE_LIBAV
endif

ifeq ($(CONFIG_WIN32),yes)
EXE=.exe
PROG=DashCast$(EXE)
else
EXT=
PROG=DashCast
endif


ifeq ($(STATICBUILD),yes)
##include static modules and other deps for libgpac
include ../../static.mak
LINKFLAGS+=-lgpac_static
LINKFLAGS+= $(GPAC_SH_FLAGS)
LINKFLAGS+=$(EXTRALIBS)
else
LINKFLAGS+=-lgpac
endif

#common obj
OBJS= dashcast.o audio_data.o audio_decoder.o audio_encoder.o audio_muxer.o circular_buffer.o cmd_data.o controler.o message_queue.o register.o video_data.o video_decoder.o video_encoder.o video_muxer.o video_scaler.o task.o

LINKFLAGS+= $(ffmpeg_lflags_dashcast)

ifneq ($(CONFIG_LIBSWRESAMPLE),no)
CFLAGS+=-DDC_AUDIO_RESAMPLER
LINKFLAGS+=-lswresample
endif

ifeq ($(CONFIG_DARWIN),yes)
#fixme - use proper detection of libavdevice dependencies
#LINKFLAGS+=-lavfilter -lswresample -lbz2
endif

SRCS := $(OBJS:.o=.c)

all: $(PROG)

DashCast$(EXE): $(OBJS)
	$(CC) -o ../../bin/gcc/$@ $(OBJS) -L../../bin/gcc $(LDFLAGS) $(LINKFLAGS)

clean:
	rm -f $(OBJS) ../../bin/gcc/$(PROG)

install: clean
	install -m 755 $(INSTFLAGS) ../../bin/gcc/DashCast "$(DESTDIR)$(prefix)/bin"

uninstall:
	rm -rf $(DESTDIR)$(prefix)/bin/DashCast

dep: depend

depend:
	rm -f .depend
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

distclean: clean
	rm -f Makefile.bak .depend

-include .depend
