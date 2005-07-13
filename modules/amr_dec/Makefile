include ../../config.mak

VPATH=$(SRC_PATH)/modules/amr_dec

#note: __MSDOS__ defined is the same cfg as linux. There may be some pbs on other platforms, to check...
CFLAGS= $(OPTFLAGS) -w -I$(SRC_PATH)/include -D__MSDOS__ -DMMS_IO -DWMOPS=0 -DVAD1


ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g -DDEBUG
LDFLAGS+=-g
endif


#common obj
OBJS=amr_in.o amr_dec.o \
	./AMR_NB/agc.o ./AMR_NB/autocorr.o ./AMR_NB/az_lsp.o ./AMR_NB/bits2prm.o ./AMR_NB/cl_ltp.o \
	./AMR_NB/convolve.o ./AMR_NB/c1035pf.o ./AMR_NB/d_plsf.o ./AMR_NB/d_plsf_5.o ./AMR_NB/d_gain_c.o \
	./AMR_NB/d_gain_p.o ./AMR_NB/dec_lag6.o ./AMR_NB/d1035pf.o ./AMR_NB/cor_h.o ./AMR_NB/enc_lag3.o \
	./AMR_NB/enc_lag6.o ./AMR_NB/g_code.o ./AMR_NB/g_pitch.o ./AMR_NB/int_lpc.o ./AMR_NB/inter_36.o \
	./AMR_NB/inv_sqrt.o ./AMR_NB/lag_wind.o ./AMR_NB/levinson.o ./AMR_NB/lsp_az.o ./AMR_NB/lsp_lsf.o \
	./AMR_NB/ol_ltp.o ./AMR_NB/pitch_fr.o ./AMR_NB/pitch_ol.o ./AMR_NB/pow2.o ./AMR_NB/pre_big.o \
	./AMR_NB/pre_proc.o ./AMR_NB/pred_lt.o ./AMR_NB/preemph.o ./AMR_NB/prm2bits.o ./AMR_NB/pstfilt.o \
	./AMR_NB/q_gain_c.o ./AMR_NB/q_gain_p.o ./AMR_NB/q_plsf.o ./AMR_NB/q_plsf_5.o ./AMR_NB/lsfwt.o \
	./AMR_NB/reorder.o ./AMR_NB/residu.o ./AMR_NB/lsp.o ./AMR_NB/lpc.o ./AMR_NB/ec_gains.o \
	./AMR_NB/spreproc.o ./AMR_NB/syn_filt.o ./AMR_NB/weight_a.o ./AMR_NB/qua_gain.o \
	./AMR_NB/gc_pred.o ./AMR_NB/q_plsf_3.o ./AMR_NB/post_pro.o ./AMR_NB/dec_lag3.o ./AMR_NB/dec_gain.o \
	./AMR_NB/d_plsf_3.o ./AMR_NB/d4_17pf.o ./AMR_NB/c4_17pf.o ./AMR_NB/d3_14pf.o ./AMR_NB/c3_14pf.o \
	./AMR_NB/d2_11pf.o ./AMR_NB/c2_11pf.o ./AMR_NB/d2_9pf.o ./AMR_NB/c2_9pf.o ./AMR_NB/cbsearch.o \
	./AMR_NB/spstproc.o ./AMR_NB/gain_q.o ./AMR_NB/cod_amr.o ./AMR_NB/dec_amr.o ./AMR_NB/sp_enc.o \
	./AMR_NB/sp_dec.o ./AMR_NB/ph_disp.o ./AMR_NB/g_adapt.o ./AMR_NB/calc_en.o ./AMR_NB/qgain795.o \
	./AMR_NB/qgain475.o ./AMR_NB/sqrt_l.o ./AMR_NB/set_sign.o ./AMR_NB/s10_8pf.o ./AMR_NB/bgnscd.o \
	./AMR_NB/gmed_n.o ./AMR_NB/mac_32.o ./AMR_NB/ex_ctrl.o ./AMR_NB/c_g_aver.o ./AMR_NB/lsp_avg.o \
	./AMR_NB/int_lsf.o ./AMR_NB/c8_31pf.o ./AMR_NB/d8_31pf.o ./AMR_NB/p_ol_wgh.o ./AMR_NB/ton_stab.o \
	./AMR_NB/vad1.o ./AMR_NB/dtx_enc.o ./AMR_NB/dtx_dec.o ./AMR_NB/a_refl.o ./AMR_NB/b_cn_cod.o \
	./AMR_NB/calc_cor.o ./AMR_NB/hp_max.o ./AMR_NB/vadname.o ./AMR_NB/vad2.o ./AMR_NB/r_fft.o \
	./AMR_NB/lflg_upd.o ./AMR_NB/e_homing.o ./AMR_NB/d_homing.o ./AMR_NB/basicop2.o \
	./AMR_NB/count.o ./AMR_NB/oper_32b.o ./AMR_NB/copy.o ./AMR_NB/log2.o \
	./AMR_NB/set_zero.o ./AMR_NB/strfunc.o ./AMR_NB/n_proc.o ./AMR_NB/sid_sync.o


SRCS := $(OBJS:.o=.c) 

LIB=amr_dec.$(DYN_LIB_SUFFIX)
ifeq ($(CONFIG_WIN32),yes)
LDFLAGS+=-export-symbols amr_dec.def 
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
