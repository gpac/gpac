# Microsoft Developer Studio Project File - Name="render_full" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=render_full - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "render_full.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "render_full.mak" CFG="render_full - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "render_full - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "render_full - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "render_full - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "render_full___Win32_Release"
# PROP BASE Intermediate_Dir "render_full___Win32_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "obj/render_full_rel"
# PROP Intermediate_Dir "obj/render_full_rel"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "RENDERH_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "../../include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "RENDERH_EXPORTS" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x40c /d "NDEBUG"
# ADD RSC /l 0x40c /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 opengl32.lib glu32.lib /nologo /dll /machine:I386 /out:"../../bin/w32_rel/gm_render_full.dll"

!ELSEIF  "$(CFG)" == "render_full - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "render_full___Win32_Debug"
# PROP BASE Intermediate_Dir "render_full___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "obj/render_full_deb"
# PROP Intermediate_Dir "obj/render_full_deb"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "RENDERH_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "../../include" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "RENDERH_EXPORTS" /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x40c /d "_DEBUG"
# ADD RSC /l 0x40c /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 opengl32.lib glu32.lib /nologo /dll /debug /machine:I386 /out:"../../bin/w32_deb/gm_render_full.dll" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "render_full - Win32 Release"
# Name "render_full - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\modules\render_full\background.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\background2d.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\bindable.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\bitmap.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\camera.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\composite_texture.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\drawable.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\events.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\form.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\geometry_2d.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\geometry_3d.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\geometry_ifs2d.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\geometry_ils2d.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\geometry_x3d.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\gradients.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\grouping.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\grouping_stacks_2d.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\grouping_stacks_3d.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\hardcoded_protos.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\layer_2d.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\layer_3d.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\layout.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\lighting.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\mesh.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\mesh_collide.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\mesh_tesselate.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\navigate.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\path_layout.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\render.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\render.def
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\render_2d.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\render_3d.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\render_nodes.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\sensor_stacks.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\sound.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\svg_base.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\svg_media.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\svg_paint_servers.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\svg_text.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\text.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\texturing_gl.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\viewport.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\visual.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\visual_2d.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\visual_2d_draw.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\visual_3d.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\visual_3d_gl.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\modules\render_full\camera.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\drawable.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\gl_inc.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\grouping.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\mesh.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\node_stacks.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\render.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\texturing.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\visual_manager.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\visual_manager_2d.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\render_full\visual_manager_3d.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
