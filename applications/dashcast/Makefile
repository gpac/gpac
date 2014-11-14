include ../../config.mak

vpath %.c $(SRC_PATH)/applications/dashcast

CFLAGS= $(OPTFLAGS) -D_GNU_SOURCE -I"$(SRC_PATH)/include" -I../../ $(ffmpeg_cflags)

LINKLIBS=$(GPAC_SH_FLAGS)

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

ifeq ($(GPACREADONLY), yes)
CFLAGS+=-DGPAC_READ_ONLY
endif

ifneq ($(CONFIG_LIBAV),no)
CFLAGS+=-DGPAC_USE_LIBAV
endif

ifneq ($(CONFIG_LIBAVRESAMPLE),no)
CFLAGS+=-DDC_AUDIO_RESAMPLER
LINKLIBS+=-lavresample
endif


#common obj
OBJS= dashcast.o audio_data.o audio_decoder.o audio_encoder.o audio_muxer.o circular_buffer.o cmd_data.o controler.o message_queue.o register.o video_data.o video_decoder.o video_encoder.o video_muxer.o video_scaler.o task.o

ifeq ($(CONFIG_WIN32),yes)
EXE=.exe
PROG=DashCast$(EXE)
else
EXT=
PROG=DashCast
endif

LINKLIBS+= $(ffmpeg_lflags)

ifeq ($(CONFIG_DARWIN), yes)
#fixme - use proper detection of libavdevice dependencies
LINKLIBS+=-lavfilter -lswresample -lbz2 
endif


SRCS := $(OBJS:.o=.c) 

all: $(PROG)

DashCast$(EXE): $(OBJS)
	$(CC) $(LDFLAGS) -o ../../bin/gcc/$@ $(OBJS) -L../../bin/gcc  -lgpac $(LINKLIBS)

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
