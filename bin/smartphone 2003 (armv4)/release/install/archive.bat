set OLDDIR=%CD%
cd /d %~dp0

for /f "delims=" %%a in ('svnversion ') do set gpac_revision=%%a
set gpac_version="0.5.1-DEV-r%gpac_revision%"

zip "GPAC_%gpac_version%_WindowsMobile.zip" ../*.dll ../*.exe ../*.plg

cd /d %OLDDIR%
