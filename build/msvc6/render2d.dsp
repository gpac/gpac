# Microsoft Developer Studio Project File - Name="render2d" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=render2d - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "render2d.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "render2d.mak" CFG="render2d - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "render2d - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "render2d - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "render2d - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "obj/render2d_rel"
# PROP Intermediate_Dir "obj/render2d_rel"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "RENDER2D_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "../../include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x40c /d "NDEBUG"
# ADD RSC /l 0x40c /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 /nologo /dll /machine:I386 /out:"../../bin/w32_rel/render2d.dll"

!ELSEIF  "$(CFG)" == "render2d - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "obj/render2d_deb"
# PROP Intermediate_Dir "obj/render2d_deb"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "RENDER2D_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "../../include" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /FD /GZ /c
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
# ADD LINK32 /nologo /dll /debug /machine:I386 /out:"../../bin/w32_deb/render2d.dll" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "render2d - Win32 Release"
# Name "render2d - Win32 Debug"
# Begin Group "svg"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\modules\render2d\svg\render_svg_nodes.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render2d\svg\render_svg_text.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render2d\svg\svg_animation.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render2d\svg\svg_media.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render2d\svg\svg_stacks.h
# End Source File
# End Group
# Begin Group "smil"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\modules\render2d\smil\smil_animation.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render2d\smil\smil_stacks.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\modules\render2d\background2d.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render2d\copy_pixels.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render2d\drawable.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render2d\drawable.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\render2d\form.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render2d\geometry_stacks.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render2d\grouping.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render2d\grouping.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\render2d\grouping_stacks.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render2d\ifs2d.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render2d\ils2d.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render2d\layout.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render2d\path_layout.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render2d\render2d.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render2d\render2d.def
# End Source File
# Begin Source File

SOURCE=..\..\modules\render2d\render2d.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\render2d\render2d_nodes.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render2d\sensor_stacks.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render2d\sound.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render2d\stacks2d.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\render2d\text.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render2d\texture_stacks.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render2d\viewport.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render2d\visualsurface2d.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render2d\visualsurface2d.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\render2d\visualsurface2d_draw.c
# End Source File
# End Target
# End Project
