@echo off
set OLDDIR=%CD%
cd /d %~dp0

IF NOT EXIST .\.git\NUL GOTO not_git

for /f "delims=" %%a in ('git describe --tags --long') do @set VERSION=%%a
for /f "delims=" %%a in ('git describe --tags --abbrev^=0') do @set TAG=%%a-
for /f "delims=" %%a in ('git rev-parse --abbrev-ref HEAD') do @set BRANCH=%%a
REM remove anotated tag from VERSION
setlocal enabledelayedexpansion
call set VERSION=%%VERSION:!TAG!=%%
setlocal disabledelayedexpansion
echo #define GPAC_GIT_REVISION "%VERSION%-%BRANCH%" > test.h

:write_file
if not exist include\gpac\revision.h goto create
ECHO n|COMP test.h include\gpac\revision.h > nul
if errorlevel 1 goto create
DEL test.h
goto done

:create
MOVE /Y test.h include\gpac\revision.h
goto done

:not_git
echo "not a git dir"
find /c "-DEV" include\gpac\version.h >nul 
if %errorlevel% equ 1 goto rel_tag
echo "unknwon tag"
@echo off
echo #define GPAC_GIT_REVISION "UNKNOWN_REV" > test.h
goto write_file

:rel_tag
echo "release tag"
@echo off
echo #define GPAC_GIT_REVISION "release" > test.h
goto write_file


:done
cd /d %OLDDIR%
exit/b

