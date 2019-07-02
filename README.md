[![Build Status](https://tests.gpac.io/testres/badge/build/ubuntu64_bb?branch=filters)](https://buildbot.gpac.io/#/grid?branch=filters)
[![Tests](https://tests.gpac.io/testres/badge/tests/linux64?branch=filters)](https://tests.gpac.io/testres/filters)

[![Build Status](https://tests.gpac.io/testres/badge/build/ubuntu32_bb?branch=filters)](https://buildbot.gpac.io/#/grid?branch=filters)
[![Tests](https://tests.gpac.io/testres/badge/tests/linux32?branch=filters)](https://tests.gpac.io/testres/filters)

[![Build Status](https://tests.gpac.io/testres/badge/build/windows64_bb?branch=filters)](https://buildbot.gpac.io/#/grid?branch=filters)
[![Tests](https://tests.gpac.io/testres/badge/tests/win64?branch=filters)](https://tests.gpac.io/testres/filters)

[![Build Status](https://tests.gpac.io/testres/badge/build/windows32_bb?branch=filters)](https://buildbot.gpac.io/#/grid?branch=filters)
[![Tests](https://tests.gpac.io/testres/badge/tests/win32?branch=filters)](https://tests.gpac.io/testres/filters)

[![Build Status](https://tests.gpac.io/testres/badge/build/macos_bb?branch=filters)](https://buildbot.gpac.io/#/grid?branch=filters)
[![Tests](https://tests.gpac.io/testres/badge/tests/macos?branch=filters)](https://tests.gpac.io/testres/filters)

[![Build Status](https://tests.gpac.io/testres/badge/build/ios_bb?branch=filters)](https://buildbot.gpac.io/#/grid?branch=filters)

[![Build Status](https://tests.gpac.io/testres/badge/build/android_bb?branch=filters)](https://buildbot.gpac.io/#/grid?branch=filters)

[![Coverage](https://tests.gpac.io/testres/badge/cov/linux64?branch=filters)](https://tests.gpac.io/testres/filters)
[![Coverage](https://tests.gpac.io/testres/badge/covfn/linux64?branch=filters)](https://tests.gpac.io/testres/filters)

![License](https://img.shields.io/badge/license-LGPL-blue.svg)

README for GPAC version 0.9.0-DEV

# Introduction

GPAC is a multimedia framework oriented towards rich media and distributed under the LGPL license (see COPYING).

GPAC supports many multimedia formats, from simple audiovisual containers (avi, mov, mpg) to complex  presentation formats (MPEG-4 Systems, SVG Tiny 1.2, VRML/X3D) and 360 videos. GPAC supports presentation scripting for MPEG4/VRML/X3D through mozilla SpiderMonkey javascript engine.

GPAC currently supports local file playback, HTTP progressive download, Adaptive HTTP Streaming (MPEG-DASH, HLS), RTP/RTSP streaming over UDP (unicast or multicast) or TCP,  TS demuxing (from file, IP or DVB4Linux), ATSC 3.0 ROUTE sessions, desktop grabbing, camera/microphone inputs and any input format supported by FFmpeg.

For more information, visit the [GPAC website](http://gpac.io)

GPAC is being developped at [Telecom Paris](https://www.telecom-paris.fr/), in the [MultiMedia group](http://www.tsi.telecom-paristech.fr/mm/), by many [great contributors](https://github.com/gpac/gpac/graphs/contributors)

GPAC includes the following applications built by default:
## MP4Box
MP4Box is a multi-purpose MP4 file manipulation for the prompt, featuring media importing and extracting, file inspection, DASH segmentation, RTP hinting, ... See `MP4Box -h`, `man MP4Box` or [our wiki](https://github.com/gpac/gpac/wiki/MP4Box-Introduction).


## MP4Client
MP4Client is a media player built upon libgpac, featuring a rich media interactive composition engine with MPEG-4 BIFS, SVG, VRML/X3D support.
For GPAC configuration instruction, check `MP4Client -h` ,  `man MP4Client` or [our wiki](https://github.com/gpac/gpac/wiki/mp4client).

## gpac 
As of version 0.9.0, GPAC includes a filter engine in charge of stream management and used by most applications in GPAC - [check this](https://github.com/gpac/gpac/wiki/Rearchitecture) for more dicussion on how this impacts MP4Box and MP4Client.
The gpac application is a direct interface to the filter engine of GPAC, allowing any combinaison of filers not enabled by other applications. See `gpac -h`, `man gpac`, `man gpac-filters` or [our wiki](https://github.com/gpac/gpac/wiki/Filters) for more details.


# Roadmap
## Ongoing tasks and bugs
Please use [github](https://github.com/gpac/gpac/issues) for feature requests and bug reports. When filing a request there, please tag it as feature-request.	

## V0.9.0
Remaining before release
- [x] move encrypter to filter
- [x] move ATSC demux to filter
- [x] move NVDec to filter
- [x] move MediaCodec to filter
- [x] move DekTec output to filter
- [x] move HEVC tile splitter to filter
- [x] move HEVC tile merger to filter
- [x] merge AV1 
- [x] merge VP9 
- [x] move iOS client to filters
- [ ] move Android client to filters
- [x] rewrite MP42TS to filters or drop it
- [ ] fixed features disabled during rearchitecture or drop them (FILTER_FIXME macro)
- [x] add pipe source and sink
- [x] add  tcp source and sink
- [x] add generic serializer/reader for all events/packets in filter arch
- [x] add Remotery support
- [x] filter API documentation
- [x] add segmentation handling in TS muxer
- [ ] unify vout color handling (complete) and compositor GLSL shaders (partial color support)
- [x] pass all test suite

## V1.0.0
Targets:
- [ ] freeze filter API
- [ ] add ffmpeg muxer support
- [ ] add ffmpeg simple avfilter support
- [ ] drop SpiderMonkey and move to duktape or similar
- [ ] filters scriptable through JS
- [x] improve filter graph resolver
- [x] improve filter scheduler ?
- [ ] move input sensors to filter ?
- [ ] filters scriptable through other languages ?

# Testing
GPAC has a test suite exercicing most features of the framework. Check the [tests readme](tests/README.md) for more details. Per-commit [build](https://buildbot.gpac.io/) and [tests results](https://tests.gpac.io) are available.

# Compilation and Installation

Please visit the [build section](https://github.com/gpac/gpac/wiki/Build-Introduction) of our wiki.

# Source code tree
This is a short overview of the gpac source repository. 

- *gpac/applications/* various apps of GPAC, including MP4Box, MP4Client, gpac and other players for iOS and Android
- *gpac/bin/* output path of build system
- *gpac/build/* various build systems (MSVC, Android, XCode, ...)
- *gpac/debian/* files for debian packaging
- *gpac/extra_lib/* external lib directory used by different build systems
- *gpac/include/gpac/* all exported files of the lib (high level APIs). Development headers are <gpac/file>
- *gpac/include/gpac/internal/* all development files of the lib (low level access).
- *gpac/include/gpac/modules/* all module APIs defined in GPAC.
- *gpac/packagers/* installer scripts for Windows and OSX
- *gpac/modules/* various modules of GPAC (video and audio output, font engine,  sensors, ...)
- *gpac/share/doc* doxygen for GPAC
- *gpac/share/doc/man* man pages for GPAC
- *gpac/share/gui/* BIFS+SVG based GUI used by client.
- *gpac/share/lang/* Language files (under development) for GPAC options and help
- *gpac/share/scripts/* various JS scripts used by GPAC.
- *gpac/share/shaders/* GLSL shaders used by the compositor.
- *gpac/share/vis/* Remotery web client.
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


For more information, visit:
- the GPAC website: http://gpac.io
- the GPAC wiki: https://github.com/gpac/gpac/wiki

