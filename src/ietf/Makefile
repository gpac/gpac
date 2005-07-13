include ../../config.mak

VPATH=$(SRC_PATH)/src/ietf

CFLAGS= $(OPTFLAGS) -Wall -I$(SRC_PATH)/include

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif


#common obj
OBJS=rtcp.o rtp.o rtp_packetizer.o rtp_pck_3gpp.o rtp_pck_mpeg12.o rtp_pck_mpeg4.o rtsp_command.o rtsp_common.o rtsp_response.o rtsp_session.o sdp.o


SRCS := $(OBJS:.o=.c) 

LIB=../../bin/gcc/temp/libgpac_ietf.a

all: $(LIB)


$(LIB): $(OBJS)
	$(AR) rc $@ $(OBJS)
	$(RANLIB) $@


%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $< 



dep: depend

depend:
	rm -f .depend
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend


clean: 
	rm -f *.o $(LIB)



distclean: clean
	rm -f Makefile.bak .depend



# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
