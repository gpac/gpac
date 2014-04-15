@echo off
set OLDDIR=%CD%
cd /d %~dp0
for /f "delims=" %%a in ('svnversion') do echo #define GPAC_SVN_REVISION "%%a" > test.h 
if not exist include\gpac\revision.h goto create 
ECHO n|COMP test.h include\gpac\revision.h > nul 
if errorlevel 1 goto create
DEL test.h
goto done

:create
MOVE /Y test.h include\gpac\revision.h

:done
cd /d %OLDDIR%
exit/b
