# Microsoft Developer Studio Project File - Name="V4Studio" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=V4Studio - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "V4Studio.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "V4Studio.mak" CFG="V4Studio - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "V4Studio - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "V4Studio - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "V4Studio - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "obj/v4studio_rel"
# PROP Intermediate_Dir "obj/v4studio_rel"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "../../include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "__WINDOWS__" /D "__WXMSW__" /D "__WXDEBUG__" /D WXDEBUG=1 /D "__WIN95__" /D "__WIN32__" /D WINVER=0x0400 /D "STRICT" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x40c /d "NDEBUG"
# ADD RSC /l 0x40c /i "E:\MesProjets\wxWindows\include" /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 wxbase26.lib wxmsw26_core.lib wxmsw26_adv.lib ws2_32.lib shell32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib ole32.lib uuid.lib odbc32.lib odbccp32.lib comctl32.lib rpcrt4.lib winmm.lib /nologo /subsystem:windows /machine:I386 /out:"../../bin/w32_rel/V4Studio.exe"
# SUBTRACT LINK32 /map

!ELSEIF  "$(CFG)" == "V4Studio - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "obj/v4studio_deb"
# PROP Intermediate_Dir "obj/v4studio_deb"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "../../include" /I "../../M4Systems/SceneGraph" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "__WINDOWS__" /D "__WXMSW__" /D "__WXDEBUG__" /D WXDEBUG=1 /D "__WIN95__" /D "__WIN32__" /D WINVER=0x0400 /D "STRICT" /FR /FD /GZ /GZ /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x40c /d "_DEBUG"
# ADD RSC /l 0x40c /fo"rc\V4Studio.res" /i "C:\Projets\wxWindows\include" /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /machine:IX86
# ADD LINK32 wxbase26d.lib wxmsw26d_core.lib wxmsw26d_adv.lib comctl32.lib ws2_32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib rpcrt4.lib /nologo /subsystem:windows /debug /machine:I386 /out:"../../bin/w32_deb/V4Studio.exe" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "V4Studio - Win32 Release"
# Name "V4Studio - Win32 Debug"
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\appearance.bmp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\wx\msw\blank.cur
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\wx\msw\bullseye.cur
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\wx\msw\cdrom.ico
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\circle.bmp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\close.bmp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\colortransform.bmp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\wx\msw\computer.ico
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\copy.bmp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\wx\msw\cross.cur
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\cut.bmp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\delete.bmp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\wx\msw\drive.ico
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\wx\msw\file1.ico
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\wx\msw\floppy.ico
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\wx\msw\folder1.ico
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\wx\msw\folder2.ico
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\fs.bmp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\group.bmp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\wx\msw\hand.cur
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\ifs2d.bmp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\ils2d.bmp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\image.bmp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\layer2d.bmp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\lg.bmp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\lineproperties.bmp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\wx\msw\magnif1.cur
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\material2d.bmp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\movie.bmp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\new.bmp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\wx\msw\noentry.cur
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\open.bmp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\orderedgroup.bmp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\paste.bmp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\paste_use.bmp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\wx\msw\pbrush.cur
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\wx\msw\pencil.cur
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\wx\msw\pntleft.cur
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\wx\msw\pntright.cur
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\preview.bmp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\print.bmp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\wx\msw\query.cur
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\RCa01312
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\rect.bmp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\redo.bmp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\wx\msw\removble.ico
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\resource_orig.xrc
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\rg.bmp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\wx\msw\rightarr.cur
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\wx\msw\roller.cur
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\save.bmp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\shape.bmp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\sound.bmp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\t2d.bmp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\text.bmp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\tm2d.bmp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\undo.bmp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\v4.bmp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\v4.ico
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\V4Studio.rc

!IF  "$(CFG)" == "V4Studio - Win32 Release"

!ELSEIF  "$(CFG)" == "V4Studio - Win32 Debug"

# ADD BASE RSC /l 0x40c /i "\CVS\gpac\applications\v4studio\rc" /i "\gpac2\applications\v4studio\rc"
# SUBTRACT BASE RSC /i "C:\Projets\wxWindows\include"
# ADD RSC /l 0x40c /i "\CVS\gpac\applications\v4studio\rc" /i "\gpac2\applications\v4studio\rc"
# SUBTRACT RSC /i "C:\Projets\wxWindows\include"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\wx\msw\watch1.cur
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\rc\xlineproperties.bmp
# End Source File
# End Group
# Begin Group "UI"

# PROP Default_Filter ""
# Begin Group "TimeLine"

# PROP Default_Filter "*.h"
# Begin Source File

SOURCE=..\..\applications\v4studio\safe_include.h
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\V4TimeLine\V4TimeLine.cpp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\V4TimeLine\V4TimeLine.h
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\V4TimeLine\V4TimeLineCase.cpp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\V4TimeLine\V4TimeLineCase.h
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\V4TimeLine\V4TimeLineElt.cpp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\V4TimeLine\V4TimeLineElt.h
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\V4TimeLine\V4TimeLineHdr.cpp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\V4TimeLine\V4TimeLineHdr.h
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\V4TimeLine\V4TimeLineLine.cpp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\V4TimeLine\V4TimeLineLine.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\applications\v4studio\V4CommandPanel.cpp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\V4CommandPanel.h
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\V4FieldList.cpp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\V4FieldList.h
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\V4StudioFrame.cpp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\V4StudioFrame.h
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\V4StudioTree.cpp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\V4StudioTree.h
# End Source File
# End Group
# Begin Group "SceneManagment"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\applications\v4studio\V4Node.cpp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\V4Node.h
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\V4NodePool.cpp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\V4NodePool.h
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\V4NodePools.cpp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\V4NodePools.h
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\V4SceneManager.cpp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\V4SceneManager.h
# End Source File
# End Group
# Begin Group "GPACTerminal"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\applications\v4studio\V4Service.cpp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\V4Service.h
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\wxGPACPanel.cpp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\wxGPACPanel.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\applications\v4studio\V4StudioApp.cpp
# End Source File
# Begin Source File

SOURCE=..\..\applications\v4studio\V4StudioApp.h
# End Source File
# End Target
# End Project
