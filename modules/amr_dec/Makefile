include ../../config.mak

vpath %.c $(SRC_PATH)/modules/amr_dec

#note: __MSDOS__ defined is the same cfg as linux. There may be some pbs on other platforms, to check...
CFLAGS= $(OPTFLAGS) -w -I"$(SRC_PATH)/include" -D__MSDOS__ -DMMS_IO -DWMOPS=0 -DVAD1


ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g -DDEBUG
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

#common obj
OBJS=amr_in.o amr_dec.o \
	./amr_nb/agc.o ./amr_nb/autocorr.o ./amr_nb/az_lsp.o ./amr_nb/bits2prm.o ./amr_nb/cl_ltp.o \
	./amr_nb/convolve.o ./amr_nb/c1035pf.o ./amr_nb/d_plsf.o ./amr_nb/d_plsf_5.o ./amr_nb/d_gain_c.o \
	./amr_nb/d_gain_p.o ./amr_nb/dec_lag6.o ./amr_nb/d1035pf.o ./amr_nb/cor_h.o ./amr_nb/enc_lag3.o \
	./amr_nb/enc_lag6.o ./amr_nb/g_code.o ./amr_nb/g_pitch.o ./amr_nb/int_lpc.o ./amr_nb/inter_36.o \
	./amr_nb/inv_sqrt.o ./amr_nb/lag_wind.o ./amr_nb/levinson.o ./amr_nb/lsp_az.o ./amr_nb/lsp_lsf.o \
	./amr_nb/ol_ltp.o ./amr_nb/pitch_fr.o ./amr_nb/pitch_ol.o ./amr_nb/pow2.o ./amr_nb/pre_big.o \
	./amr_nb/pre_proc.o ./amr_nb/pred_lt.o ./amr_nb/preemph.o ./amr_nb/prm2bits.o ./amr_nb/pstfilt.o \
	./amr_nb/q_gain_c.o ./amr_nb/q_gain_p.o ./amr_nb/q_plsf.o ./amr_nb/q_plsf_5.o ./amr_nb/lsfwt.o \
	./amr_nb/reorder.o ./amr_nb/residu.o ./amr_nb/lsp.o ./amr_nb/lpc.o ./amr_nb/ec_gains.o \
	./amr_nb/spreproc.o ./amr_nb/syn_filt.o ./amr_nb/weight_a.o ./amr_nb/qua_gain.o \
	./amr_nb/gc_pred.o ./amr_nb/q_plsf_3.o ./amr_nb/post_pro.o ./amr_nb/dec_lag3.o ./amr_nb/dec_gain.o \
	./amr_nb/d_plsf_3.o ./amr_nb/d4_17pf.o ./amr_nb/c4_17pf.o ./amr_nb/d3_14pf.o ./amr_nb/c3_14pf.o \
	./amr_nb/d2_11pf.o ./amr_nb/c2_11pf.o ./amr_nb/d2_9pf.o ./amr_nb/c2_9pf.o ./amr_nb/cbsearch.o \
	./amr_nb/spstproc.o ./amr_nb/gain_q.o ./amr_nb/cod_amr.o ./amr_nb/dec_amr.o ./amr_nb/sp_enc.o \
	./amr_nb/sp_dec.o ./amr_nb/ph_disp.o ./amr_nb/g_adapt.o ./amr_nb/calc_en.o ./amr_nb/qgain795.o \
	./amr_nb/qgain475.o ./amr_nb/sqrt_l.o ./amr_nb/set_sign.o ./amr_nb/s10_8pf.o ./amr_nb/bgnscd.o \
	./amr_nb/gmed_n.o ./amr_nb/mac_32.o ./amr_nb/ex_ctrl.o ./amr_nb/c_g_aver.o ./amr_nb/lsp_avg.o \
	./amr_nb/int_lsf.o ./amr_nb/c8_31pf.o ./amr_nb/d8_31pf.o ./amr_nb/p_ol_wgh.o ./amr_nb/ton_stab.o \
	./amr_nb/vad1.o ./amr_nb/dtx_enc.o ./amr_nb/dtx_dec.o ./amr_nb/a_refl.o ./amr_nb/b_cn_cod.o \
	./amr_nb/calc_cor.o ./amr_nb/hp_max.o ./amr_nb/vadname.o ./amr_nb/vad2.o ./amr_nb/r_fft.o \
	./amr_nb/lflg_upd.o ./amr_nb/e_homing.o ./amr_nb/d_homing.o ./amr_nb/basicop2.o \
	./amr_nb/count.o ./amr_nb/oper_32b.o ./amr_nb/copy.o ./amr_nb/log2.o \
	./amr_nb/set_zero.o ./amr_nb/strfunc.o ./amr_nb/n_proc.o ./amr_nb/sid_sync.o


SRCS := $(OBJS:.o=.c) 

LIB=gm_amr_dec.$(DYN_LIB_SUFFIX)
ifeq ($(CONFIG_WIN32),yes)
#LDFLAGS+=-export-symbols amr_dec.def 
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
