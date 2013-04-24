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
    SET "PRGROOT=%programfiles(x86)%" 
) 
if "%PROCESSOR_ARCHITECTURE%" == "x86" ( 
    SET PRGROOT=%programfiles% 
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

REM execute svnversion and check if the result if found within revision.h
for /f "delims=" %%i in ('svnversion.exe') do Set VarRevisionSVN=%%i
REM 'M', 'S', 'P', ':' are special 'svnversion' results
for /f "delims=" %%i in ('echo %VarRevisionSVN% ^| findstr /i /r M^"') do goto RevisionAbort
for /f "delims=" %%i in ('echo %VarRevisionSVN% ^| findstr /i /r S^"') do goto RevisionAbort
for /f "delims=" %%i in ('echo %VarRevisionSVN% ^| findstr /i /r P^"') do goto RevisionAbort
for /f "delims=" %%i in ('echo %VarRevisionSVN% ^| findstr /i /r :^"') do goto RevisionAbort
for /f "delims=" %%i in ('type include\gpac\revision.h ^| findstr /i /r "%VarRevisionSVN%"') do Set VarRevisionBuild=%%i
echo VarRevisionBuild = %VarRevisionBuild%
echo VarRevisionSVN   = %VarRevisionSVN%
if !"%VarRevisionBuild%"==!"%VarRevisionSVN%" echo   local revision and last build revision are not congruent - please consider rebuilding before generating an installer
if !"%VarRevisionBuild%"==!"%VarRevisionSVN%" goto Abort
REM echo   version found: %VarRevisionSVN%

move packagers\win32_64\nsis\default.out packagers\win32_64\nsis\default.out_
echo Name "GPAC Framework ${GPAC_VERSION} for %1 revision %VarRevisionSVN%" > packagers\win32_64\nsis\default.out
echo OutFile "GPAC.Framework.Setup-${GPAC_VERSION}-rev%VarRevisionSVN%-%1.exe" >> packagers\win32_64\nsis\default.out
IF "%1"=="x64" echo !define IS_WIN64 >> packagers\win32_64\nsis\default.out

echo:
REM ============================================
echo Executing NSIS
REM ============================================
call %NSIS_EXEC% packagers\win32_64\nsis\gpac_installer.nsi


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
set VarRevisionSVN=
set VarRevisionBuild=
cd /d %OLDDIR%
exit/b 0

:RevisionAbort
echo   SVN revision "%VarRevisionSVN%" is not a simple number, you may have local modification (please check 'svnrevision' flags meaning or execute the NSIS script manually)

:Abort
echo:
echo  *** ABORTING: CHECK ERROR MESSAGES ABOVE ***

REM LeaveBatchError 
set VarRevisionSVN=
set VarRevisionBuild=
cd /d %OLDDIR%
exit/b 1
