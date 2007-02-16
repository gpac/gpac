CC=gcc
CFLAGS=-Wall -DUSE_TCP_STANDARD
# -pedantic -ansi
SOURCES=
INCLUDES=-Iinclude -I.
APPNAME=broadcaster
OBJECTS=RTP_serv_generator.o RTP_serv_packetizer.o RTP_serv_sender.o broadcaster.o sdp_generator.o
LIBS=-lgpac

all: broadcaster

broadcaster: $(OBJECTS) 
	@echo "edition de liens"
	@$(CC) -o $(APPNAME) $(OBJECTS) $(LIBS)

.c.o:
	@echo "compiling $*.c"
	@$(CC) -g -c $(INCLUDES) $(CFLAGS) $*.c


clean:
	@echo "cleaning"
	-rm -f $(OBJECTS) $(APPNAME) *~
