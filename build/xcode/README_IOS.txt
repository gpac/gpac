How to build GPAC for iOS
=========================

Tested revision: 3239
Tested XCode (SDK): 3.2.6 (4.3)

To compile GPAC in command-line mode, please execute the following steps:

1) Check out the GPAC git repository:
   "git clone https://github.com/gpac/gpac"

2) Get the "gpac_extra_libs.zip" file from:
https://sourceforge.net/p/gpac/code/HEAD/tree/trunk/gpac_extra_libs/

Unzip it in its current directory (ie not a subfolder) so that you have the following directory tree:
   trunk/
      gpac/
         ...
      gpac_extra_libs/
         a52dec/
         AMR_NB/
         ...

3) Compile gpac dependencies (zlib, faad, libpng etc):
   execute "gpac_extra_libs/build/xcode_ios/generate_extra_libs.sh" script.
   execute "gpac_extra_libs/SDL_iOS/generate_SDL.sh	
  
4) Check all gpac dependencies have been successfully compiled:
   $> ls gpac_extra_libs/lib/iOS
      libSDL2.a libglue.a libfaad.a libfreetype.a libjpeg.a libjs.a libmad.a libpng.a

5) Copy the extra_libs to the right place:
   if you checked out as mentionned in 1), execute the "gpac_extra_libs/CopyLibs2Public4iOS.sh" script.
   Otherwise copy manually the libs to the gpac/extra_lib/lib/ios directory.

6) Compile GPAC:
   execute /gpac/configure to generate include/gpac/revision.h
   execute the "gpac/build/xcode/generate_ios.sh" script.
   The script checks that all files have been compiled, print an error message if needed.
   Otherwise it generats a .tar.gz archive into the gpac/bin/iOS directory.
