@echo off
set OLDDIR=%CD%
cd /d %~dp0
for /f "delims=" %%a in ('git describe --tags --long') do @set VERSION=%%a
for /f "delims=" %%a in ('git describe --tags --abbrev^=0') do @set TAG=%%a-
for /f "delims=" %%a in ('git rev-parse --abbrev-ref HEAD') do @set BRANCH=%%a
REM remove anotated tag from VERSION
setlocal enabledelayedexpansion
call set VERSION=%%VERSION:!TAG!=%%
setlocal disabledelayedexpansion
echo #define GPAC_GIT_REVISION "%VERSION%-%BRANCH%" > test.h
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

