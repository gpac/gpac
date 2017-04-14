set OLDDIR=%CD%
cd /d %~dp0

for /f "delims=" %%a in ('git describe --tags --long') do @set VERSION=%%a
for /f "delims=" %%a in ('git describe --tags --abbrev=0') do @set TAG=%%a-
for /f "delims=" %%a in ('git rev-parse --abbrev-ref HEAD') do @set BRANCH=%%a
REM remove anotated tag from VERSION
setlocal enabledelayedexpansion
call set VERSION=%%VERSION:!TAG!=%%
setlocal disabledelayedexpansion
set revision="%VERSION%-%BRANCH%"
set gpac_version="0.7.0-r%gpac_revision%

zip "GPAC_%gpac_version%_WindowsMobile.zip" ../*.dll ../*.exe ../*.plg

cd /d %OLDDIR%
