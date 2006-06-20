# Microsoft Developer Studio Project File - Name="standalone2drender" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=standalone2drender - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "standalone2drender.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "standalone2drender.mak" CFG="standalone2drender - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "standalone2drender - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "standalone2drender - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "standalone2drender - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "obj/sar2d_rel"
# PROP Intermediate_Dir "obj/sar2d_rel"
# PROP Target_Dir ""
F90=df.exe
MTL=midl.exe
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "../../include" /I "../../extra_lib/include/freetype" /I "../../modules/m4_rend" /I "../../modules/render2d" /I "../../modules/ft_font" /I "../../modules/raw_out" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "GPAC_STANDALONE_RENDER_2D" /D "DANAE" /FR /YX /FD /c
# ADD BASE RSC /l 0x40c /d "NDEBUG"
# ADD RSC /l 0x40c /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"../../bin/w32_rel/gpac_sar2d.lib"

!ELSEIF  "$(CFG)" == "standalone2drender - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "obj/sar2d_deb"
# PROP Intermediate_Dir "obj/sar2d_deb"
# PROP Target_Dir ""
F90=df.exe
MTL=midl.exe
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "../../include" /I "../../extra_lib/include/freetype" /I "../../modules/m4_rend" /I "../../modules/render2d" /I "../../modules/ft_font" /I "../../modules/raw_out" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "GPAC_STANDALONE_RENDER_2D" /D "DANAE" /FR /YX /FD /GZ /c
# ADD BASE RSC /l 0x40c /d "_DEBUG"
# ADD RSC /l 0x40c /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"../../bin/w32_deb/gpac_sar2d.lib"

!ENDIF 

# Begin Target

# Name "standalone2drender - Win32 Release"
# Name "standalone2drender - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\modules\render2d\background2d.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render2d\drawable.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render2d\form.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\ft_font\ft_font.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\soft_raster\ftgrays.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render2d\geometry_stacks.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render2d\grouping.c
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

SOURCE=..\..\modules\soft_raster\raster_565.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\soft_raster\raster_argb.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\soft_raster\raster_load.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\soft_raster\raster_rgb.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\raw_out\raw_video.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render2d\render2d.c

!IF  "$(CFG)" == "standalone2drender - Win32 Release"

# ADD CPP /FAcs

!ELSEIF  "$(CFG)" == "standalone2drender - Win32 Debug"

!ENDIF 

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

SOURCE=..\..\applications\standalone2drender\standalone2drender.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\soft_raster\stencil.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\soft_raster\surface.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render2d\svg_base.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render2d\svg_media.c
# End Source File
# Begin Source File

SOURCE=..\..\modules\render2d\svg_stacks.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\render2d\svg_text.c
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

SOURCE=..\..\modules\render2d\visualsurface2d_draw.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\modules\render2d\drawable.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\render2d\grouping.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\soft_raster\rast_soft.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\render2d\render2d.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\render2d\stacks2d.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\render2d\svg\svg_stacks.h
# End Source File
# Begin Source File

SOURCE=..\..\modules\render2d\visualsurface2d.h
# End Source File
# End Group
# End Target
# End Project
