[![Build Status](http://gpacvm-ext.enst.fr/django/testres/badge/build/ubuntu64)](http://gpacvm-ext.enst.fr/buildbot/#/grid?branch=master)
[![Tests](http://gpacvm-ext.enst.fr/django/testres/badge/tests/linux64)](http://gpacvm-ext.enst.fr/django/testres/)

[![Build Status](http://gpacvm-ext.enst.fr/django/testres/badge/build/ubuntu32)](http://gpacvm-ext.enst.fr/buildbot/#/grid?branch=master)
[![Tests](http://gpacvm-ext.enst.fr/django/testres/badge/tests/linux32)](http://gpacvm-ext.enst.fr/django/testres/)

[![Build Status](http://gpacvm-ext.enst.fr/django/testres/badge/build/windows64)](http://gpacvm-ext.enst.fr/buildbot/#/grid?branch=master)
[![Tests](http://gpacvm-ext.enst.fr/django/testres/badge/tests/win64)](http://gpacvm-ext.enst.fr/django/testres/)

[![Build Status](http://gpacvm-ext.enst.fr/django/testres/badge/build/windows32)](http://gpacvm-ext.enst.fr/buildbot/#/grid?branch=master)
[![Tests](http://gpacvm-ext.enst.fr/django/testres/badge/tests/win32)](http://gpacvm-ext.enst.fr/django/testres/)

[![Build Status](http://gpacvm-ext.enst.fr/django/testres/badge/build/macos)](http://gpacvm-ext.enst.fr/buildbot/#/grid?branch=master)
[![Tests](http://gpacvm-ext.enst.fr/django/testres/badge/tests/macos)](http://gpacvm-ext.enst.fr/django/testres/)

[![Build Status](http://gpacvm-ext.enst.fr/django/testres/badge/build/ios)](http://gpacvm-ext.enst.fr/buildbot/#/grid?branch=master)

[![Build Status](http://gpacvm-ext.enst.fr/django/testres/badge/build/android)](http://gpacvm-ext.enst.fr/buildbot/#/grid?branch=master)

[![Coverage](http://gpacvm-ext.enst.fr/django/testres/badge/cov/linux64)](http://gpacvm-ext.enst.fr/django/testres/)

![License](https://img.shields.io/badge/license-LGPL-blue.svg)

README for GPAC version 0.9.0-ALPHA

# Introduction

GPAC is a multimedia framework oriented towards rich media and distributed under the LGPL license (see COPYING).

GPAC supports many multimedia formats, from simple audiovisual containers (avi, mov, mpg) to complex  presentation formats (MPEG-4 Systems, SVG Tiny 1.2, VRML/X3D) and 360 videos. GPAC supports presentation scripting for MPEG4/VRML/X3D through mozilla SpiderMonkey javascript engine.

GPAC currently supports local file playback, HTTP progressive download, Adaptive HTTP Streaming (MPEG-DASH, HLS), RTP/RTSP streaming over UDP (unicast or multicast) or TCP,  TS demuxing (from file, IP or DVB4Linux), ATSC 3.0 ROUTE sessions, desktop grabbing, camera/microphone inputs and any input format supported by FFmpeg.

For more information, visit the GPAC website:
	http://gpac.io

GPAC includes the following applications built by default:
## MP4Box
MP4Box is a multi-purpose MP4 file manipulation for the prompt, featuring media importing and extracting, file inspection, DASH segmentation, RTP hinting, ... See MP4Box -h for more info on the tool
MP4Box documentation is available online at https://gpac.wp.imt.fr/mp4box/


## MP4Client
MP4Client is a media player built upon libgpac, featuring a rich media interactive composition engine with MPEG-4 BIFS, SVG, VRML/X3D support.
For GPAC configuration instruction, check gpac/doc/configuration.html or gpac/doc/man/gpac.1 (man gpac when installed)

## MP42TS (deprecated)
MP42TS is a TS multiplexer from MP4 and RTP sources.

## gpac 
As of version 0.9.0, GPAC features a filter engine in charge of stream management and used by most applications in GPAC. For API backward compatibility, old apps (MP4Box, MP4Client, ...) have been kept but rewritten to match the new filter architecture, but do not expose all possible filter connections. The gpac application is a direct interface to the filter engine of GPAC, allowing any combinaison of filers not enabled by other apps. See gpac -h for more details.


# Compilation and Installation

## Quick Instructions
Installation instructions for latest GPAC GIT version - June 2018:

For Windows: https://gpac.wp.imt.fr/2011/04/18/command-line-gpac-compiling-on-windows-x86-using-free-microsoft-visual-c/
For Linux Ubuntu: https://gpac.wp.imt.fr/2011/04/20/compiling-gpac-on-ubuntu/
For Mac OS X: https://gpac.wp.imt.fr/2011/05/02/compiling-gpac-for-macos-x/
For Android: https://gpac.wp.imt.fr/compiling-gpac-for-android/


## Generic Instructions

GPAC may be compiled without any third party libraries, but in this case its functionalities are very limited (no still image, no audio, no video, no text, no scripting). It is therefore recommended to download the extra lib package available at http://sourceforge.net/projects/gpac. Compilation instructions for these libraries are provided per library in the package. 

The current extra_lib package to use with gpac is gpac_extra_libs available here: http://download.tsi.telecom-paristech.fr/gpac/gpac_extra_libs.zip

In case you have some of these libs already installed on your system, the detailed list of dependencies is
* freetype2 from version 2.1.4 on.
* SpiderMonkey v1.7 or 1.85 (libjs from mozilla) .
* libjpg version 6b
* Libpng version 1.2.33 (older versions should work)
* MAD version 0.15.1b (older versions should work)
* xvid version 1.0 (0.9.0 / .1 / .2 should also work)
* ffmpeg (latest stable API version checked was 17 February 2016 snapshot, you may need to change a few things with current versions)
* libogg 1.1, libvorbis 1.1 and libtheora 1.0 from Xiph.org (newer versions work)
* faad2, version 2.0 or above (2.6.1 working)
* liba52, version 0.7.4
* OpenJPEG, version 1.3
* OpenSVCDecoder, version 1.3


/!\ GPAC won't compile if the 'git' command is not in your path /!\

!! WARNING !!
The following instructions may not be completely up to data
You should use online instructions which may be more up-to-date:
http://gpac.io/2011/04/18/command-line-gpac-compiling-on-windows-x86-using-free-microsoft-visual-c/

Detailed instruction for Win32 MSVC Compilation are available in gpac/doc/INSTALL.w32

Detailed instruction for WinCE eVC Compilation are available in gpac/doc/INSTALL.wCE

Detailed instruction for GCC Compilation are available in gpac/doc/INSTALL.gcc

Detailed instruction for GCC cross-compilation for familiar+GPE systems are available in gpac/doc/INSTALL.gpe
	
Detailed instruction for GCCE/Symbian cross-compilation for Symbian v9.1 systems are available in gpac/doc/INSTALL.symbian

Detailed instruction for iOS Compilation are available in gpac/build/xcode/README_IOS.txt


# Source code tree
This is a short overview of the gpac source repository. 

- *gpac/applications/* various apps of GPAC, including MP4Box, MP4Client and other players for iOS and Android
- *gpac/bin/* output path of build system
- *gpac/build/* various build systems (MSVC, Android, XCode, ...)
- *gpac/debian/* files for debian packaging
- *gpac/doc/* doxygen for GPAC
- *gpac/extra_lib/* external lib directory used by different build systems
- *gpac/gui/* BIFS+SVG based GUI used by client.
- *gpac/include/gpac/* all exported files of the lib (high level APIs). Development headers are <gpac/file>
- *gpac/include/gpac/internal/* all development files of the lib (low level access).
- *gpac/include/gpac/modules/* all module APIs defined in GPAC.
- *gpac/packagers/* installer scripts for Windows and OSX
- *gpac/modules/* various modules of GPAC (video and audio output, font engine, rasterizer,  sensors, ...)
- *gpac/shaders/* GLSL shaders used by the compositor.
- *gpac/src/bifs/* BInary Format for Scene coding (decoder and encoder) (BIFS tables are with MPEG4Gen application in gpac/applications/generators/MPEG4)
- *gpac/src/compositor/* interactive composition engine  for 2D & 3D drawing - handles MPEG-4, X3D/VRML and SVG.
- *gpac/src/crypto/* cryptographic tools (AES 128 CBC and CTR only)
- *gpac/src/filter_core/* filter engine of GPAC, in charge of filter graph resolution, filter scheduling, packets handling.
- *gpac/src/filters/* filters defined in GPAC. This include encoders, decoders, av output, wrapper for GPAC's compositor, ISOBMF, RTP, M2TS muxers and demuxers, etc ...
- *gpac/src/ietf/* small RTP/RTSP/SDP library, plus media packetizers.
- *gpac/src/isomedia/* ISOBMFF (Iso Base Media File Format), features file reading/writing/editing, precise interleaving, hint track creation and movie fragments (read/write). Includes 3GPP/3GPP2 ,  AVC/SVC, HEVC/L-HEVC and JPEG2000 support.
- *gpac/src/laser/* MPEG-4 LAsER (Lightweight Application Scene Representation)
- *gpac/src/media_tools/* media tools for authoring: ISMA & 3GPP tools, AV parsers, media importing and exporting, hinting ...
- *gpac/src/odf/* MPEG-4 Object Descriptor Framework: encoding/decoding of all descriptors, OD codec and OCI codec
- *gpac/src/scene_manager/* memory representation of the scene, importers (BT/XMT/SWF/QT), dumpers and encoding
- *gpac/src/scenegraph/* scene Graph API (MPEG4/VRML/X3D/SVG) - BIFS/VRML/X3D nodes are generated using gpac/applications/generators/*
- *gpac/src/terminal/* client application engine. This is a simple wrapper around the filter engine of GPAC.
- *gpac/src/utils/* all generic objects used throughout the lib (list, bitstream, thread, mutex...). The OS specific files are prefixed os_* . Porting libgpac to new platforms usually means porting only these files and updating the makefile
- *gpac/tests/* tests suite for GPAC. See gpac/tests/README.md


