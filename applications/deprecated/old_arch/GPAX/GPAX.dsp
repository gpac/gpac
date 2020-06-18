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
# ADD CPP /nologo /MDd /W3 /Gm /ZI /Od /I "../../include" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /FR /Yu"stdafx.h" /FD /GZ /c
# ADD BASE RSC /l 0x804 /d "_DEBUG"
# ADD RSC /l 0x804 /d "_DEBUG"
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
# ADD CPP /nologo /MD /W3 /Gm /ZI /I "../../include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /FR /Yu"stdafx.h" /FD /GZ /c
# SUBTRACT CPP /O<none>
# ADD BASE RSC /l 0x804 /d "_DEBUG"
# ADD RSC /l 0x804 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 js32.lib zlib.lib winmm.lib ws2_32.lib /nologo /subsystem:windows /dll /debug /machine:I386 /pdbtype:sept /libpath:"../gpac/extra_lib/lib/w32_deb"
# ADD LINK32 js32.lib zlib.lib winmm.lib ws2_32.lib /nologo /subsystem:windows /dll /debug /machine:I386 /out:"../../bin/w32_rel/GPAX.dll" /pdbtype:sept /libpath:"../../extra_lib/lib/w32_rel"
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Cmds=copy ..\..\bin\w32_rel\GPAX.dll "C:\Program Files\GPAC"
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "GPAX - Win32 Debug"
# Name "GPAX - Win32 Release"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\GPAX.cpp
# End Source File
# Begin Source File

SOURCE=.\GPAX.def
# End Source File
# Begin Source File

SOURCE=.\GPAX.idl
# ADD MTL /tlb ".\GPAX.tlb" /h "GPAX.h" /iid "GPAX_i.c" /Oicf
# End Source File
# Begin Source File

SOURCE=.\GPAX.rc
# End Source File
# Begin Source File

SOURCE=.\GPAXPlugin.cpp
# End Source File
# Begin Source File

SOURCE=.\StdAfx.cpp
# ADD CPP /Yc"stdafx.h"
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\GPAXPlugin.h
# End Source File
# Begin Source File

SOURCE=.\Resource.h
# End Source File
# Begin Source File

SOURCE=.\StdAfx.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\gpax.bmp
# End Source File
# Begin Source File

SOURCE=.\GPAX.rgs
# End Source File
# Begin Source File

SOURCE=.\GPAXProp.rgs
# End Source File
# End Group
# End Target
# End Project
# Section GPAX : {00000000-0000-0000-0000-800000800000}
# 	1:21:IDS_DOCSTRINGGPAXProp:105
# 	1:12:IDD_GPAXPROP:107
# 	1:12:IDR_GPAXPROP:106
# 	1:20:IDS_HELPFILEGPAXProp:104
# 	1:17:IDS_TITLEGPAXProp:103
# End Section
