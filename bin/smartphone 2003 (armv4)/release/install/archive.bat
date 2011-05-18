for /f "delims=" %%a in ('svnversion') do set gpac_revision=%%a
set OLDDIR=%CD%
cd /d %~dp0
zip "GPAC_0.4.6-r%gpac_revision%_WindowsMobile.zip" ../*.dll ../*.exe ../*.plg
cd /d %OLDDIR%
