# Microsoft Developer Studio Project File - Name="GPAX" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=GPAX - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "GPAX.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "GPAX.mak" CFG="GPAX - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "GPAX - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "GPAX - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "GPAX - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "obj/w32_deb"
# PROP Intermediate_Dir "obj/w32_deb"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /Yu"stdafx.h" /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /ZI /Od /I "../../include" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /FR /FD /GZ /c
# SUBTRACT CPP /YX /Yc /Yu
# ADD BASE RSC /l 0x804 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 js32.lib zlib.lib winmm.lib ws2_32.lib /nologo /subsystem:windows /dll /debug /machine:I386 /out:"../../bin/w32_deb/GPAX.dll" /pdbtype:sept /libpath:"../../extra_lib/lib/w32_deb"
# Begin Custom Build - Performing registration
OutDir=.\obj/w32_deb
TargetPath=\CVS\gpac\bin\w32_deb\GPAX.dll
InputPath=\CVS\gpac\bin\w32_deb\GPAX.dll
SOURCE="$(InputPath)"

"$(OutDir)\regsvr32.trg" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	regsvr32 /s /c "$(TargetPath)" 
	echo regsvr32 exec. time > "$(OutDir)\regsvr32.trg" 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "GPAX - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "obj/w32_rel"
# PROP BASE Intermediate_Dir "obj/w32_rel"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "obj/w32_rel"
# PROP Intermediate_Dir "obj/w32_rel"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /Gm /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /FR /Yu"stdafx.h" /FD /GZ /c
# ADD CPP /nologo /MD /W3 /Gm /ZI /I "../../include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /FR /FD /GZ /c
# SUBTRACT CPP /YX /Yc /Yu
# ADD BASE RSC /l 0x804 /d "_DEBUG"
# ADD RSC /l 0x804 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 js32.lib zlib.lib winmm.lib ws2_32.lib /nologo /subsystem:windows /dll /debug /machine:I386 /pdbtype:sept /libpath:"../gpac/extra_lib/lib/w32_deb"
# ADD LINK32 /nologo /subsystem:windows /dll /debug /machine:I386 /out:"../../bin/w32_rel/GPAX.dll" /pdbtype:sept /libpath:"../../extra_lib/lib/w32_rel"
# Begin Custom Build - Performing registration
OutDir=.\obj/w32_rel
TargetPath=\CVS\gpac\bin\w32_rel\GPAX.dll
InputPath=\CVS\gpac\bin\w32_rel\GPAX.dll
SOURCE="$(InputPath)"

"$(OutDir)\regsvr32.trg" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	regsvr32 /s /c "$(TargetPath)" 
	echo regsvr32 exec. time > "$(OutDir)\regsvr32.trg" 
	
# End Custom Build
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Cmds=copy ..\..\bin\w32_rel\GPAX.dll "C:\Program Files\GPAC"
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "GPAX - Win32 Debug"
# Name "GPAX - Win32 Release"
# Begin Source File

SOURCE=..\..\applications\GPAX\gpax.bmp
# End Source File
# Begin Source File

SOURCE=..\..\applications\GPAX\GPAX.cpp
# End Source File
# Begin Source File

SOURCE=..\..\applications\GPAX\GPAX.def
# End Source File
# Begin Source File

SOURCE=..\..\applications\GPAX\GPAX.h
# End Source File
# Begin Source File

SOURCE=..\..\applications\GPAX\GPAX.idl
# ADD MTL /tlb ".\GPAX.tlb"
# End Source File
# Begin Source File

SOURCE=..\..\applications\GPAX\GPAX.rc
# End Source File
# Begin Source File

SOURCE=..\..\applications\GPAX\GPAX.rgs
# End Source File
# Begin Source File

SOURCE=..\..\applications\GPAX\GPAXPlugin.cpp
# End Source File
# Begin Source File

SOURCE=..\..\applications\GPAX\GPAXPlugin.h
# End Source File
# Begin Source File

SOURCE=..\..\applications\GPAX\resource.h
# End Source File
# Begin Source File

SOURCE=..\..\applications\GPAX\StdAfx.cpp
# End Source File
# Begin Source File

SOURCE=..\..\applications\GPAX\StdAfx.h
# End Source File
# End Target
# End Project
# Section GPAX : {00000000-0000-0000-0000-800000800000}
# 	1:21:IDS_DOCSTRINGGPAXProp:105
# 	1:12:IDD_GPAXPROP:107
# 	1:12:IDR_GPAXPROP:106
# 	1:20:IDS_HELPFILEGPAXProp:104
# 	1:17:IDS_TITLEGPAXProp:103
# End Section
