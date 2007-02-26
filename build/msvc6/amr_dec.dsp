# Microsoft Developer Studio Project File - Name="amr_dec" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=amr_dec - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "amr_dec.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "amr_dec.mak" CFG="amr_dec - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "amr_dec - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "amr_dec - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "amr_dec - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "obj/amr_dec_rel"
# PROP Intermediate_Dir "obj/amr_dec_rel"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "amr_dec_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "../../include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "MMS_IO" /FD /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x40c /d "NDEBUG"
# ADD RSC /l 0x40c /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 /nologo /stack:0x800000 /dll /machine:I386 /out:"../../bin/w32_rel/gm_amr_dec.dll"
# SUBTRACT LINK32 /verbose /profile /incremental:yes /debug /nodefaultlib

!ELSEIF  "$(CFG)" == "amr_dec - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "obj/amr_dec_deb"
# PROP Intermediate_Dir "obj/amr_dec_deb"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "amr_dec_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /I "../../include" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "MMS_IO" /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x40c /d "_DEBUG"
# ADD RSC /l 0x40c /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 /nologo /dll /debug /machine:I386 /out:"../../bin/w32_deb/gm_amr_dec.dll"
# SUBTRACT LINK32 /verbose /profile /pdb:none /incremental:no

!ENDIF 

# Begin Target

# Name "amr_dec - Win32 Release"
# Name "amr_dec - Win32 Debug"
# Begin Group "amr_nb_dec"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\a_refl.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\a_refl.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\agc.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\agc.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\autocorr.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\autocorr.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\az_lsp.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\az_lsp.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\b_cn_cod.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\b_cn_cod.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\basic_op.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\basicop2.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\bgnscd.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\bgnscd.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\bits2prm.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\bits2prm.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\c1035pf.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\c1035pf.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\c2_11pf.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\c2_11pf.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\c2_9pf.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\c2_9pf.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\c3_14pf.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\c3_14pf.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\c4_17pf.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\c4_17pf.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\c8_31pf.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\c8_31pf.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\c_g_aver.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\c_g_aver.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\calc_cor.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\calc_cor.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\calc_en.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\calc_en.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\cbsearch.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\cbsearch.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\cl_ltp.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\cl_ltp.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\cnst.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\cnst_vad.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\cod_amr.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\cod_amr.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\convolve.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\convolve.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\copy.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\copy.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\cor_h.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\cor_h.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\count.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\count.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\d1035pf.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\d1035pf.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\d2_11pf.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\d2_11pf.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\d2_9pf.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\d2_9pf.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\d3_14pf.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\d3_14pf.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\d4_17pf.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\d4_17pf.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\d8_31pf.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\d8_31pf.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\d_gain_c.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\d_gain_c.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\d_gain_p.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\d_gain_p.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\d_homing.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\d_homing.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\d_plsf.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\d_plsf.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\d_plsf_3.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\d_plsf_5.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\dec_amr.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\dec_amr.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\dec_gain.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\dec_gain.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\dec_lag3.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\dec_lag3.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\dec_lag6.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\dec_lag6.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\dtx_dec.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\dtx_dec.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\dtx_enc.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\dtx_enc.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\ec_gains.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\enc_lag6.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\ex_ctrl.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\ex_ctrl.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\frame.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\g_adapt.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\g_adapt.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\g_code.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\g_code.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\g_pitch.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\g_pitch.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\gain_q.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\gain_q.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\gc_pred.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\gc_pred.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\gmed_n.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\gmed_n.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\hp_max.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\hp_max.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\int_lpc.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\int_lpc.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\int_lsf.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\int_lsf.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\inter_36.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\inter_36.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\inv_sqrt.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\inv_sqrt.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\lag_wind.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\lag_wind.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\levinson.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\levinson.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\lflg_upd.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\log2.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\log2.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\lpc.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\lpc.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\lsfwt.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\lsfwt.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\lsp.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\lsp.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\lsp_avg.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\lsp_avg.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\lsp_az.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\lsp_az.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\lsp_lsf.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\lsp_lsf.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\mac_32.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\mac_32.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\mode.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\n_proc.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\n_proc.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\ol_ltp.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\ol_ltp.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\oper_32b.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\oper_32b.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\p_ol_wgh.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\p_ol_wgh.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\ph_disp.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\ph_disp.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\pitch_fr.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\pitch_fr.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\pitch_ol.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\pitch_ol.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\post_pro.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\post_pro.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\pow2.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\pow2.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\pre_big.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\pre_big.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\pre_proc.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\pre_proc.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\pred_lt.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\pred_lt.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\preemph.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\preemph.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\prm2bits.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\prm2bits.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\pstfilt.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\pstfilt.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\q_gain_c.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\q_gain_c.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\q_gain_p.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\q_gain_p.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\q_plsf.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\q_plsf.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\q_plsf_3.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\q_plsf_5.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\qgain475.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\qgain475.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\qgain795.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\qgain795.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\qua_gain.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\qua_gain.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\r_fft.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\reorder.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\reorder.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\residu.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\residu.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\s10_8pf.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\s10_8pf.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\set_sign.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\set_sign.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\set_zero.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\set_zero.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\sid_sync.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\sid_sync.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\sp_dec.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\sp_dec.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\sp_enc.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\sp_enc.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\spreproc.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\spreproc.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\spstproc.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\spstproc.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\sqrt_l.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\sqrt_l.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\strfunc.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\strfunc.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\syn_filt.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\syn_filt.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\ton_stab.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\ton_stab.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\typedef.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\typedefs.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\vad.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\vad1.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\vad1.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\vad2.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\vad2.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\vadname.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\vadname.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\weight_a.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\weight_a.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_dec.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_dec.def
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_in.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_nb\enc_lag3.c
# End Source File
# End Target
# End Project
