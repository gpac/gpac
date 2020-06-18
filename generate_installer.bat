@echo off
set OLDDIR=%CD%
cd /d %~dp0
REM ============================================
echo *** Generating a Windows GPAC installer ***
REM ============================================


:begin
IF "%1"=="win32" GOTO next
IF "%1"=="x64" GOTO next
echo You must specified target architecture : win32 or x64
GOTO Abort

:next
echo:
REM ============================================
echo Check NSIS is in your PATH
REM ============================================


if "%PROCESSOR_ARCHITECTURE%" == "AMD64" (
    setlocal enabledelayedexpansion
    SET PRGROOT=!programfiles(x86^)!
    setlocal disabledelayedexpansion
)
if "%PROCESSOR_ARCHITECTURE%" == "x86" (
    setlocal enabledelayedexpansion
    SET PRGROOT=!programfiles!
    setlocal disabledelayedexpansion
)

set NSIS_EXEC="%PRGROOT%\NSIS\makensis.exe"
if not exist "%PRGROOT%\NSIS\makensis.exe" echo   NSIS couldn't be found at default location %NSIS_EXEC%
if not exist "%PRGROOT%\NSIS\makensis.exe" goto Abort
echo   Found NSIS at default location %NSIS_EXEC%


echo:
REM ============================================
echo Retrieving version/revision information
REM ============================================
if not exist include/gpac/revision.h echo   version couldn't be found - check include/gpac/revision.h exists
if not exist include/gpac/revision.h goto Abort

REM check if found a local commit which has not been pushed
for /f "delims=" %%a in ('git diff FETCH_HEAD') do @set diff=%%a
echo diff = %diff%
if not "%diff%"=="" echo Local and remote revisions not in sync, consider pushing or pulling changes

REM execute git and check if the result if found within revision.h
for /f "delims=" %%a in ('git describe --tags --long') do @set VERSION=%%a
for /f "delims=" %%a in ('git describe --tags --abbrev^=0') do @set TAG=%%a-
for /f "delims=" %%a in ('git rev-parse --abbrev-ref HEAD') do @set BRANCH=%%a
REM remove anotated tag from VERSION
setlocal enabledelayedexpansion
call set VERSION=%%VERSION:!TAG!=%%
setlocal disabledelayedexpansion
SET VarRevisionGIT=%VERSION%-%BRANCH%
for /f "delims=" %%i in ('type include\gpac\revision.h ^| findstr /i /r "%VarRevisionGIT%"') do Set VarRevisionBuild=%%i
echo VarRevisionBuild = %VarRevisionBuild%
echo VarRevisionGIT  = %VarRevisionGIT%
if !"%VarRevisionBuild%"==!"%VarRevisionGIT%" echo   local revision and last build revision are not congruent - please consider rebuilding before generating an installer
if !"%VarRevisionBuild%"==!"%VarRevisionGIT%" goto Abort

move packagers\win32_64\nsis\default.out packagers\win32_64\nsis\default.out_
echo Name "GPAC Framework ${GPAC_VERSION} for %1 revision %VarRevisionGIT%" > packagers\win32_64\nsis\default.out
echo OutFile "gpac-${GPAC_VERSION}-rev%VarRevisionGIT%-%1.exe" >> packagers\win32_64\nsis\default.out
IF "%1"=="x64" echo !define IS_WIN64 >> packagers\win32_64\nsis\default.out

echo:
REM ============================================
echo Executing NSIS
REM ============================================
call %NSIS_EXEC% packagers\win32_64\nsis\gpac_installer.nsi
IF %ERRORLEVEL% NEQ 0 GOTO Abort


echo:
REM ============================================
echo Removing temporary files
REM ============================================
move packagers\win32_64\nsis\default.out_ packagers\win32_64\nsis\default.out


echo:
REM ============================================
echo Windows GPAC installer generated - goodbye!
REM ============================================
REM LeaveBatchSuccess
set VarRevisionGIT=
set VarRevisionBuild=
cd /d %OLDDIR%
exit/b 0

:RevisionAbort
echo Local revision and remote revision are not congruent; you may have local commit
echo Please consider pushing your commit before generating an installer

:Abort
echo:
echo  *** ABORTING: CHECK ERROR MESSAGES ABOVE ***

REM LeaveBatchError
set VarRevisionBuild=
cd /d %OLDDIR%
exit/b 1
