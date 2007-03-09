@ECHO OFF

IF "%1" == "" GOTO BTTOMP4
IF "%1" == "mp4" GOTO BTTOMP4
IF "%1" == "xmt" GOTO BTTOXMT
IF "%1" == "xmp4" GOTO XMTTOMP4
IF "%1" == "x3d" GOTO X3DVTOX3D
IF "%1" == "clean" GOTO REGCLEAN
IF "%1" == "help" GOTO GETHELP

:BTTOMP4
for %%a in (*.bt) DO (
echo %%a
MP4Box -mp4 -quiet %%a
)
GOTO DONE

:BTTOXMT
for %%a in (*.bt) DO (
echo %%a
MP4Box -xmt -quiet %%a
)
GOTO DONE

:XMTTOMP4
for %%a in (*.xmt) DO (
MP4Box -mp4 -quiet %%a
echo %a
)
GOTO DONE

:X3DVTOX3D
for %%a in (*.x3dv) DO MP4Box -x3d %%a
GOTO DONE

:REGCLEAN
DEL /S *.mp4
DEL /S *.xmt
DEL /S bifs*.html
DEL /S x3d*.html
GOTO DONE

:GETHELP
echo GPAC Regression Tests Build Script usage: 00-do-w32 [opt]
echo 	no op / opt = "mp4": builds MP4 file sfrom BT scripts
echo 	opt = "xmt": builds XMT-A files from BT scripts
echo 	opt = "xmp4": builds MP4 files from XMT-A scripts
echo 	opt = "clean": deletes all XMT-A and MP4 files
echo 	opt = "help": prints this screen.
GOTO DONE


:DONE

