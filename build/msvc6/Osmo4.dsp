# Microsoft Developer Studio Project File - Name="Osmo4" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=Osmo4 - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "Osmo4.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Osmo4.mak" CFG="Osmo4 - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Osmo4 - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "Osmo4 - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Osmo4 - Win32 Release"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 6
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "obj/osmo4_w32_rel"
# PROP Intermediate_Dir "obj/osmo4_w32_rel"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "../../include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /FD /c
# SUBTRACT CPP /YX /Yc /Yu
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x40c /d "NDEBUG" /d "_AFXDLL"
# ADD RSC /l 0x40c /d "NDEBUG" /d "_AFXDLL"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /machine:I386
# ADD LINK32 /nologo /stack:0x800000 /subsystem:windows /machine:I386 /out:"../../bin/w32_rel/Osmo4.exe"

!ELSEIF  "$(CFG)" == "Osmo4 - Win32 Debug"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 6
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "obj/osmo4_w32_deb"
# PROP Intermediate_Dir "obj/osmo4_w32_deb"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_AFXDLL" /Yu"stdafx.h" /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "../../include" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /FR /FD /GZ /c
# SUBTRACT CPP /YX /Yc /Yu
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x40c /d "_DEBUG" /d "_AFXDLL"
# ADD RSC /l 0x40c /d "_DEBUG" /d "_AFXDLL"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 /nologo /subsystem:windows /debug /machine:I386 /out:"../../bin/w32_deb/Osmo4.exe" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "Osmo4 - Win32 Release"
# Name "Osmo4 - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\applications\osmo4_w32\AddressBar.cpp
# End Source File
# Begin Source File

SOURCE=..\..\applications\osmo4_w32\FileProps.cpp
# End Source File
# Begin Source File

SOURCE=..\..\applications\osmo4_w32\MainFrm.cpp
# End Source File
# Begin Source File

SOURCE=..\..\applications\osmo4_w32\OpenUrl.cpp
# End Source File
# Begin Source File

SOURCE=..\..\applications\osmo4_w32\Options.cpp
# End Source File
# Begin Source File

SOURCE=..\..\applications\osmo4_w32\Osmo4.cpp
# End Source File
# Begin Source File

SOURCE=..\..\applications\osmo4_w32\Osmo4.rc
# End Source File
# Begin Source File

SOURCE=..\..\applications\osmo4_w32\Playlist.cpp
# End Source File
# Begin Source File

SOURCE=..\..\applications\osmo4_w32\Sliders.cpp
# End Source File
# Begin Source File

SOURCE=..\..\applications\osmo4_w32\StdAfx.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\applications\osmo4_w32\AddressBar.h
# End Source File
# Begin Source File

SOURCE=..\..\applications\osmo4_w32\FileProps.h
# End Source File
# Begin Source File

SOURCE=..\..\applications\osmo4_w32\MainFrm.h
# End Source File
# Begin Source File

SOURCE=..\..\applications\osmo4_w32\OpenUrl.h
# End Source File
# Begin Source File

SOURCE=..\..\applications\osmo4_w32\Options.h
# End Source File
# Begin Source File

SOURCE=..\..\applications\osmo4_w32\Osmo4.h
# End Source File
# Begin Source File

SOURCE=..\..\applications\osmo4_w32\Playlist.h
# End Source File
# Begin Source File

SOURCE=..\..\applications\osmo4_w32\Resource.h
# End Source File
# Begin Source File

SOURCE=..\..\applications\osmo4_w32\Sliders.h
# End Source File
# Begin Source File

SOURCE=..\..\applications\osmo4_w32\StdAfx.h
# End Source File
# End Group
# Begin Group "res"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\applications\osmo4_w32\res\error.ico
# End Source File
# Begin Source File

SOURCE=..\..\applications\osmo4_w32\res\maintool.bmp
# End Source File
# Begin Source File

SOURCE=..\..\applications\osmo4_w32\res\message.ico
# End Source File
# Begin Source File

SOURCE=..\..\applications\osmo4_w32\res\osmo4.ico
# End Source File
# Begin Source File

SOURCE=..\..\applications\osmo4_w32\res\pause.ico
# End Source File
# Begin Source File

SOURCE=..\..\applications\osmo4_w32\res\play.ico
# End Source File
# Begin Source File

SOURCE=..\..\applications\osmo4_w32\res\playlist.bmp
# End Source File
# Begin Source File

SOURCE=..\..\applications\osmo4_w32\res\stop.ico
# End Source File
# End Group
# End Target
# End Project
