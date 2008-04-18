# Microsoft Developer Studio Project File - Name="dx_hw" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=dx_hw - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "dx_hw.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "dx_hw.mak" CFG="dx_hw - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "dx_hw - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "dx_hw - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "dx_hw - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "obj/dx_hw_rel"
# PROP Intermediate_Dir "obj/dx_hw_rel"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "DX_HW_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "../../include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "GPAC_HAVE_CONFIG_H" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x40c /d "NDEBUG"
# ADD RSC /l 0x40c /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 opengl32.lib glu32.lib dsound.lib dxguid.lib ddraw.lib ole32.lib user32.lib gdi32.lib /nologo /dll /machine:I386 /out:"../../bin/w32_rel/gm_dx_hw.dll"

!ELSEIF  "$(CFG)" == "dx_hw - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "obj/dx_hw_deb"
# PROP Intermediate_Dir "obj/dx_hw_deb"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "DX_HW_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "../../include" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "GPAC_HAVE_CONFIG_H" /FR /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x40c /d "_DEBUG"
# ADD RSC /l 0x40c /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 msimg32.lib opengl32.lib dsound.lib dxguid.lib ddraw.lib ole32.lib user32.lib gdi32.lib /nologo /dll /debug /machine:I386 /out:"../../bin/w32_deb/gm_dx_hw.dll" /pdbtype:sept /libpath:"../../extra_lib/lib/w32_deb"

!ENDIF 

# Begin Target

# Name "dx_hw - Win32 Release"
# Name "dx_hw - Win32 Debug"
# Begin Source File

SOURCE=..\..\modules\dx_hw\collide.cur
# End Source File
# Begin Source File

SOURCE=..\..\modules\dx_hw\res\collide.cur
# End Source File
# Begin Source File

SOURCE=..\..\modules\dx_hw\copy_pixels.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\dx_hw\dx_2d.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\dx_hw\dx_audio.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\dx_hw\dx_hw.def
# End Source File
# Begin Source File

SOURCE=..\..\modules\dx_hw\dx_hw.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\dx_hw\dx_hw.rc
# End Source File
# Begin Source File

SOURCE=..\..\modules\dx_hw\dx_video.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\dx_hw\dx_window.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\dx_hw\hand.cur
# End Source File
# Begin Source File

SOURCE=..\..\modules\dx_hw\res\hand.cur
# End Source File
# Begin Source File

SOURCE=..\..\modules\dx_hw\resource.h
# End Source File
# End Target
# End Project
