@echo off
REM ============================================
echo *** Generating a Windows GPAC installer ***
REM ============================================


echo:
REM ============================================
echo Check NSIS is in your PATH
REM ============================================

set NSIS_EXEC="%PROGRAMFILES%\NSIS\makensis.exe"
echo   Assuming NSIS at default location %NSIS_EXEC%


echo:
REM ============================================
echo Retrieving version/revision information
REM ============================================
if not exist include/gpac/version.h echo   version couldn't be found - check include/gpac/version.h exists
if not exist include/gpac/version.h goto abort

REM execute svnversion and check if the result if found within version.h
for /f "delims=" %%i in ('svnversion.exe') do Set VarRevisionSVN=%%i
REM 'M', 'S', 'P', ':' are special 'svnversion' results
for /f "delims=" %%i in ('echo %VarRevisionSVN% ^| findstr /i /r M^"') do goto RevisionAbort
for /f "delims=" %%i in ('echo %VarRevisionSVN% ^| findstr /i /r S^"') do goto RevisionAbort
for /f "delims=" %%i in ('echo %VarRevisionSVN% ^| findstr /i /r P^"') do goto RevisionAbort
for /f "delims=" %%i in ('echo %VarRevisionSVN% ^| findstr /i /r :^"') do goto RevisionAbort
for /f "delims=" %%i in ('type include\gpac\version.h ^| findstr /i /r "%VarRevisionSVN%"') do Set VarRevisionBuild=%%i
if !"%VarRevisionBuild%"==!"" echo   local revision and last build revision are not congruent - please consider rebuilding before generating an installer
if !"%VarRevisionBuild%"==!"" goto abort
echo   version found: %VarRevisionSVN%

move bin\win32\release\nsis_install\default.out bin\win32\release\nsis_install\default.out_
echo Name "GPAC Framework ${GPAC_VERSION} revision %VarRevisionSVN%" > bin\win32\release\nsis_install\default.out
echo OutFile "GPAC.Framework.Setup-${GPAC_VERSION}-rev%VarRevisionSVN%.exe" >> bin\win32\release\nsis_install\default.out


echo:
REM ============================================
echo Executing NSIS
REM ============================================
call %NSIS_EXEC% bin\win32\release\nsis_install\gpac_installer.nsi


echo:
REM ============================================
echo Removing temporary files
REM ============================================
move bin\win32\release\nsis_install\default.out_ bin\win32\release\nsis_install\default.out


echo:
REM ============================================
echo Windows GPAC installer generated - goodbye!
REM ============================================
exit/b

:RevisionAbort
echo   SVN revision "%VarRevisionSVN%" is not a simple number, you may have local modification (please check 'svnrevision' flags meaning or execute the NSIS script manually)

:abort
echo:
echo  *** ABORTING: CHECK ERROR MESSAGES ABOVE ***
set VarRevisionSVN=
set VarRevisionBuild=
exit/b
