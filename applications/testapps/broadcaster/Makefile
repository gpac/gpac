include ../../../config.mak

vpath %.c $(SRC_PATH)/applications/testapps/broadcaster

CFLAGS= $(OPTFLAGS) -I"$(SRC_PATH)/include"

SOURCES=
APPNAME=broadcaster


ifeq ($(DEBUGBUILD),yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD),yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

#common obj
OBJS=RTP_serv_generator.o RTP_serv_packetizer.o RTP_serv_sender.o broadcaster.o sdp_generator.o debug.o

LINKFLAGS=-L../../../bin/gcc -lgpac

all: broadcaster

broadcaster: $(OBJS) *.h
	$(CC) -o $(APPNAME) $(OBJS) $(LINKFLAGS) $(LDFLAGS)

clean:
	-rm -f $(OBJS) $(APPNAME) *~


