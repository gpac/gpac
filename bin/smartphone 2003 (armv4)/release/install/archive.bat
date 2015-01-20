set OLDDIR=%CD%
cd /d %~dp0

for /f "delims=" %%a in ('git describe --tags --long') do set VERSION=%%a
for /f "delims=" %%a in ('git rev-parse --abbrev-ref HEAD') do set BRANCH=%%a
REM remove anotated tag "v0.5.2-" from VERSION
set revision="%VERSION:~7%-%BRANCH%"
set gpac_version="0.5.2-DEV-r%gpac_revision%"

zip "GPAC_%gpac_version%_WindowsMobile.zip" ../*.dll ../*.exe ../*.plg

cd /d %OLDDIR%
