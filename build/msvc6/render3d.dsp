# Microsoft Developer Studio Project File - Name="render3d" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=render3d - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "render3d.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "render3d.mak" CFG="render3d - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "render3d - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "render3d - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "render3d - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "obj/render3d_rel"
# PROP Intermediate_Dir "obj/render3d_rel"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "RENDER3D_EXPORTS" /YX /FD /c
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
# ADD LINK32 opengl32.lib glu32.lib /nologo /dll /machine:I386 /out:"../../bin/w32_rel/render3d.dll"

!ELSEIF  "$(CFG)" == "render3d - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "obj/render3d_deb"
# PROP Intermediate_Dir "obj/render3d_deb"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "RENDER3D_EXPORTS" /YX /FD /GZ /c
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
# ADD LINK32 opengl32.lib glu32.lib /nologo /dll /debug /machine:I386 /out:"../../bin/w32_deb/render3d.dll" /pdbtype:sept /libpath:"../../extra_lib/lib/w32_deb"

!ENDIF 

# Begin Target

# Name "render3d - Win32 Release"
# Name "render3d - Win32 Debug"
# Begin Source File

SOURCE=..\..\modules\render3d\background.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render3d\bindable.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render3d\bitmap.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render3d\camera.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render3d\camera.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\render3d\form.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render3d\geometry_stacks.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render3d\geometry_x3d.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render3d\gl_inc.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\render3d\gradients.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render3d\grouping.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render3d\grouping.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\render3d\grouping_stacks.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render3d\hardcoded_protos.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render3d\layers.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render3d\layout.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render3d\lighting.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render3d\mesh.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render3d\mesh.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\render3d\mesh_collide.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render3d\navigate.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render3d\path_layout.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render3d\render3d.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render3d\render3d.def
# End Source File
# Begin Source File

SOURCE=..\..\modules\render3d\render3d.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\render3d\render3d_nodes.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render3d\render3d_nodes.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\render3d\sensor_stacks.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render3d\sound.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render3d\tesselate.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render3d\text.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render3d\texture_stacks.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render3d\texturing_gl.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render3d\viewport.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render3d\visual_surface.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render3d\visual_surface.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\render3d\vs_gl_draw.c
# End Source File
# End Target
# End Project
