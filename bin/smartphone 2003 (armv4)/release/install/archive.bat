for /f "delims=" %%a in ('svnversion') do set gpac_revision=%%a
zip "GPAC_0.4.6-r%gpac_revision%_WindowsMobile.zip" ../*.dll ../*.exe ../*.plg
