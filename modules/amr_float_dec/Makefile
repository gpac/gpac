include ../../config.mak

vpath %.c $(SRC_PATH)/modules/amr_float_dec

#note: __MSDOS__ defined is the same cfg as linux. There may be some pbs on other platforms, to check...
CFLAGS= $(OPTFLAGS) -w -I"$(SRC_PATH)/include" -D__MSDOS__

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g -DDEBUG
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

#common obj
OBJS=../amr_dec/amr_in.o amr_float_dec.o

#AMR NB obj
CFLAGS+=-DGPAC_HAS_AMR_FT
ifeq ($(CONFIG_AMR_NB_FT), yes)
OBJS+=./amr_nb_ft/interf_dec.o ./amr_nb_ft/interf_enc.o ./amr_nb_ft/sp_dec.o ./amr_nb_ft/sp_enc.o
endif


#AMR WB obj
CFLAGS+=-DGPAC_HAS_AMR_FT_WB
ifeq ($(CONFIG_AMR_WB_FT), yes)
OBJS+=./amr_wb_ft/dec_acelp.o ./amr_wb_ft/dec_dtx.o ./amr_wb_ft/dec_gain.o ./amr_wb_ft/dec_if.o ./amr_wb_ft/dec_lpc.o ./amr_wb_ft/dec_main.o \
	./amr_wb_ft/dec_rom.o ./amr_wb_ft/dec_util.o ./amr_wb_ft/enc_acelp.o ./amr_wb_ft/enc_dtx.o ./amr_wb_ft/enc_gain.o ./amr_wb_ft/enc_if.o \
	./amr_wb_ft/enc_lpc.o ./amr_wb_ft/enc_main.o ./amr_wb_ft/enc_rom.o ./amr_wb_ft/enc_util.o ./amr_wb_ft/if_rom.o 
endif


SRCS := $(OBJS:.o=.c) 

LIB=gm_amr_float_dec.$(DYN_LIB_SUFFIX)
ifeq ($(CONFIG_WIN32),yes)
#LDFLAGS+=-export-symbols amr_float_dec.def 
endif

all: $(LIB)


$(LIB): $(OBJS)
	$(CC) $(SHFLAGS) $(LDFLAGS) -o ../../bin/gcc/$@ $(OBJS) $(EXTRALIBS) -L../../bin/gcc -lgpac

clean: 
	rm -f $(OBJS) ../../bin/gcc/$(LIB)

dep: depend

depend:
	rm -f .depend	
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

distclean: clean
	rm -f Makefile.bak .depend

-include .depend
