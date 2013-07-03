include ../../config.mak

vpath %.c $(SRC_PATH)/applications/dashcast

CFLAGS= $(OPTFLAGS) -I"$(SRC_PATH)/include" -I../../

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

#common obj
OBJS= dashcast.o audio_data.o audio_decoder.o audio_encoder.o audio_muxer.o circular_buffer.o cmd_data.o controler.o message_queue.o register.o video_data.o video_decoder.o video_encoder.o video_muxer.o video_scaler.o task.o

ifeq ($(CONFIG_WIN32),yes)
EXE=.exe
PROG=DashCast$(EXE)
else
EXT=
PROG=DashCast
endif

SRCS := $(OBJS:.o=.c) 

all: $(PROG)

DashCast$(EXE): $(OBJS)
	$(CC) $(LDFLAGS) -o ../../bin/gcc/$@ $(OBJS) -L../../bin/gcc -lz -lpthread -lavcodec -lavformat -lswscale -lavutil -lavdevice -lavfilter -lm -lbz2 -lz -lswresample -lgpac $(LINKLIBS)

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



# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
