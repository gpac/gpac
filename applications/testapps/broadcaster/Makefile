include ../../../config.mak

vpath %.c $(SRC_PATH)/applications/testapps/broadcaster

CFLAGS= $(OPTFLAGS) -Wall -I$(SRC_PATH)/include -DUSE_TCP_STANDARD

SOURCES=
APPNAME=broadcaster


ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

#common obj
OBJS=RTP_serv_generator.o RTP_serv_packetizer.o RTP_serv_sender.o broadcaster.o sdp_generator.o
LIBS=-lgpac

all: broadcaster

broadcaster: $(OBJS) 
	@echo "edition de liens"
	@$(CC) -o $(APPNAME) $(OBJS) $(LIBS)

.c.o:
	@echo "compiling $*.c"
	@$(CC) -g -c $(CFLAGS) $*.c


clean:
	@echo "cleaning"
	-rm -f $(OBJS) $(APPNAME) *~
