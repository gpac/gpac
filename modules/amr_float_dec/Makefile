include ../../config.mak

VPATH=$(SRC_PATH)/modules/amr_float_dec

#note: __MSDOS__ defined is the same cfg as linux. There may be some pbs on other platforms, to check...
CFLAGS= $(OPTFLAGS) -w -I$(SRC_PATH)/include -D__MSDOS__

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g -DDEBUG
LDFLAGS+=-g
endif


#common obj
OBJS=../amr_dec/amr_in.o amr_float_dec.o

#AMR NB obj
CFLAGS+=-DGPAC_HAS_AMR_FT
ifeq ($(CONFIG_AMR_NB_FT), yes)
OBJS+=./AMR_NB_FT/interf_dec.o ./AMR_NB_FT/interf_enc.o ./AMR_NB_FT/sp_dec.o ./AMR_NB_FT/sp_enc.o
endif


#AMR WB obj
CFLAGS+=-DGPAC_HAS_AMR_FT_WB
ifeq ($(CONFIG_AMR_WB_FT), yes)
OBJS+=./AMR_WB_FT/dec_acelp.o ./AMR_WB_FT/dec_dtx.o ./AMR_WB_FT/dec_gain.o ./AMR_WB_FT/dec_if.o ./AMR_WB_FT/dec_lpc.o ./AMR_WB_FT/dec_main.o \
	./AMR_WB_FT/dec_rom.o ./AMR_WB_FT/dec_util.o ./AMR_WB_FT/enc_acelp.o ./AMR_WB_FT/enc_dtx.o ./AMR_WB_FT/enc_gain.o ./AMR_WB_FT/enc_if.o \
	./AMR_WB_FT/enc_lpc.o ./AMR_WB_FT/enc_main.o ./AMR_WB_FT/enc_rom.o ./AMR_WB_FT/enc_util.o ./AMR_WB_FT/if_rom.o 
endif


SRCS := $(OBJS:.o=.c) 

LIB=amr_float_dec.$(DYN_LIB_SUFFIX)
ifeq ($(CONFIG_WIN32),yes)
LDFLAGS+=-export-symbols amr_float_dec.def 
endif

all: $(LIB)


$(LIB): $(OBJS)
	$(CC) $(SHFLAGS) $(LDFLAGS) -o ../../bin/gcc/$@ $(OBJS) $(EXTRALIBS) -L../../bin/gcc -lgpac


%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $< 


clean: 
	rm -f $(OBJS) ../../bin/gcc/$(LIB)

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
