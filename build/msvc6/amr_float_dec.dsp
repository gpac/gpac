# Microsoft Developer Studio Project File - Name="amr_float_dec" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=amr_float_dec - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "amr_float_dec.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "amr_float_dec.mak" CFG="amr_float_dec - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "amr_float_dec - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "amr_float_dec - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "amr_float_dec - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "obj/amr_float_dec_rel"
# PROP Intermediate_Dir "obj/amr_float_dec_rel"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "AMR_FLOAT_DEC_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "../../include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "GPAC_HAS_AMR_FT" /D "GPAC_HAS_AMR_FT_WB" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x40c /d "NDEBUG"
# ADD RSC /l 0x40c /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 /nologo /dll /machine:I386 /out:"../../bin/w32_rel/gm_amr_float_dec.dll"

!ELSEIF  "$(CFG)" == "amr_float_dec - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "obj/amr_float_dec_deb"
# PROP Intermediate_Dir "obj/amr_float_dec_deb"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "AMR_FLOAT_DEC_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "../../include" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "GPAC_HAS_AMR_FT" /D "GPAC_HAS_AMR_FT_WB" /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x40c /d "_DEBUG"
# ADD RSC /l 0x40c /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 /nologo /dll /debug /machine:I386 /out:"../../bin/w32_deb/gm_amr_float_dec.dll" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "amr_float_dec - Win32 Release"
# Name "amr_float_dec - Win32 Debug"
# Begin Group "amr_nb_ft"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_nb_ft\interf_dec.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_nb_ft\interf_dec.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_nb_ft\interf_enc.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_nb_ft\interf_enc.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_nb_ft\interf_rom.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_nb_ft\rom_dec.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_nb_ft\rom_enc.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_nb_ft\sp_dec.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_nb_ft\sp_dec.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_nb_ft\sp_enc.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_nb_ft\sp_enc.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_nb_ft\typedef.h
# End Source File
# End Group
# Begin Group "amr_wb_ft"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_wb_ft\dec.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_wb_ft\dec_acelp.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_wb_ft\dec_acelp.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_wb_ft\dec_dtx.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_wb_ft\dec_dtx.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_wb_ft\dec_gain.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_wb_ft\dec_gain.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_wb_ft\dec_if.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_wb_ft\dec_if.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_wb_ft\dec_lpc.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_wb_ft\dec_lpc.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_wb_ft\dec_main.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_wb_ft\dec_main.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_wb_ft\dec_rom.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_wb_ft\dec_util.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_wb_ft\dec_util.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_wb_ft\enc.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_wb_ft\enc_acelp.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_wb_ft\enc_acelp.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_wb_ft\enc_dtx.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_wb_ft\enc_dtx.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_wb_ft\enc_gain.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_wb_ft\enc_gain.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_wb_ft\enc_if.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_wb_ft\enc_if.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_wb_ft\enc_lpc.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_wb_ft\enc_lpc.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_wb_ft\enc_main.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_wb_ft\enc_main.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_wb_ft\enc_rom.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_wb_ft\enc_util.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_wb_ft\enc_util.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_wb_ft\if_rom.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_wb_ft\if_rom.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_wb_ft\typedef.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_wb_ft\typedefs.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_float_dec.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_float_dec\amr_float_dec.def
# End Source File
# Begin Source File

SOURCE=..\..\modules\amr_dec\amr_in.c
# End Source File
# End Target
# End Project
