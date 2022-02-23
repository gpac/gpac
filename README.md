[![Build Status](https://tests.gpac.io/testres/badge/build/ubuntu64)](https://buildbot.gpac.io/#/grid?branch=master)
[![Tests](https://tests.gpac.io/testres/badge/tests/linux64)](https://tests.gpac.io/)

[![Build Status](https://tests.gpac.io/testres/badge/build/ubuntu32)](https://buildbot.gpac.io/#/grid?branch=master)
[![Tests](https://tests.gpac.io/testres/badge/tests/linux32)](https://tests.gpac.io/)

[![Build Status](https://tests.gpac.io/testres/badge/build/windows64)](https://buildbot.gpac.io/#/grid?branch=master)
[![Tests](https://tests.gpac.io/testres/badge/tests/win64)](https://tests.gpac.io/)

[![Build Status](https://tests.gpac.io/testres/badge/build/windows32)](https://buildbot.gpac.io/#/grid?branch=master)
[![Tests](https://tests.gpac.io/testres/badge/tests/win32)](https://tests.gpac.io/)

[![Build Status](https://tests.gpac.io/testres/badge/build/macos)](https://buildbot.gpac.io/#/grid?branch=master)
[![Tests](https://tests.gpac.io/testres/badge/tests/macos)](https://tests.gpac.io/)

[![Build Status](https://tests.gpac.io/testres/badge/build/ios)](https://buildbot.gpac.io/#/grid?branch=master)
[![Build Status](https://tests.gpac.io/testres/badge/build/android)](https://buildbot.gpac.io/#/grid?branch=master)

[![Coverage](https://tests.gpac.io/testres/badge/cov/linux64?branch=master)](https://tests.gpac.io/testres/)
[![Coverage](https://tests.gpac.io/testres/badge/covfn/linux64?branch=master)](https://tests.gpac.io/testres/)

![License](https://img.shields.io/badge/license-LGPL-blue.svg)
[![OpenHub](https://www.openhub.net/p/gpac/widgets/project_thin_badge.gif)](https://www.openhub.net/p/gpac)


# GPAC Introduction
Current version: 2.1-DEV

Latest Release: 2.0

GPAC is an open-source multimedia framework focused on modularity and standards compliance.
GPAC provides tools to process, inspect, package, stream, playback and interact with media content. Such content can be any combination of audio, video, subtitles, metadata, scalable graphics, encrypted media, 2D/3D graphics and ECMAScript.
GPAC is best-known for its wide MP4/ISOBMFF capabilities and is popular among video enthusiasts, academic researchers, standardization bodies, and professional broadcasters.

For more information, visit [GPAC website](http://gpac.io)

GPAC is distributed under the LGPL v2.1 or later, and is also available, for most of it, under a [commercial license](https://www.gpac-licensing.com).

Please ! _cite_ ! our work in your research:
- "GPAC Filters" (https://doi.org/10.1145/3339825.3394929) for recent versions (0.9 or above) 
- "GPAC: open source multimedia framework" (https://doi.org/10.1145/1291233.1291452) for older versions.


# Features

GPAC can process, analyse, package, stream, encode, decode and playback a wide variety of contents. Selected feature list:
- Audio: MPEG audio (mp1/2/3, aac), AC3, E-AC3, Opus, FLAC, …
- Video: MPEG 1 / 2 / 4 (H264/AVC) / H (HEVC), VVC, AV1, VP9, Theora, ...
- Subtitles: WebVTT, TTML (full, EBU-TTD, …), 3GPP/Apple Timed Text, …
- Encryption: CENC, PIFF, ISMA, OMA, ...
- Containers: MP4/fMP4/CMAF/Quicktime MOV/ProRes MOV, AVI, MPG, OGG, MKV, ...
- Streaming: MPEG-2 Transport Stream, RTP, RTSP, HTTP, Apple HLS, MPEG-DASH, ATSC 3.0 ROUTE, ...
- Supported IOs: local files, pipes, UDP/TCP, HTTP(S), custom IO
- Presentation formats: MPEG-4 BIFS, SVG Tiny 1.2, VRML/X3D
- JS scripting through QuickJS for both SVG/BIFS/VRML and extending GPAC framework tools
- 3D support (360 videos, WebGL JS filters…)
- Inputs: microphone, camera, desktop grabbing
- Highly configurable media processing pipeline
- Python and NodeJS bindings

Features are encapsulated in processing modules called filters:
- to get the full list of available features, you can run the command line `gpac -h filters` or check [filters' wiki](https://github.com/gpac/gpac/wiki/Filters).
- to get the full list of playback features, check [the dedicated wiki page](https://github.com/gpac/gpac/wiki/Player-Features).


# Tools

## MP4Box
MP4Box is a multi-purpose MP4 file manipulation for the prompt, featuring media importing and extracting, file inspection, DASH segmentation, RTP hinting, ... See `MP4Box -h`, `man MP4Box` or [our wiki](https://wiki.gpac.io/MP4Box).


## gpac 
GPAC includes a filter engine in charge of stream management and used by most applications in GPAC - [read this post](https://wiki.gpac.io/Rearchitecture) for more discussion on how this impacts MP4Box and MP4Client.
The gpac application is a direct interface to the filter engine of GPAC, allowing any combinaison of filters not enabled by other applications. See `gpac -h`, `man gpac`, `man gpac-filters` or [our wiki](https://wiki.gpac.io/Filters) for more details.

## MP4Client (deprecated)
MP4Client is a media player built upon libgpac, featuring a rich media interactive composition engine with MPEG-4 BIFS, SVG, VRML/X3D support.
For GPAC configuration instruction, check `MP4Client -h` ,  `man MP4Client` or [our wiki](https://wiki.gpac.io/mp4client).

__Warning__
MP4Client is deprecated and will be removed in the next release. Start modifying your scripts:
- replace `MP4Client URL` with `gpac -play URL` (audio/video playback only) or `gpac -mp4c URL` (if compositor is needed)
- replace `MP4Client -gui URL` with `gpac -gui URL`.

# Getting started
## Download
Stable and nightly builds installers for Windows, Linux, OSX, Android, iOS are available on [gpac.io](https://gpac.wp.imt.fr/downloads/).

If you want to compile GPAC yourself, please follow the instructions in the [build section](https://wiki.gpac.io/Build-Introduction) of our wiki.

## Documentation
The general GPAC framework documentation is available on [wiki.gpac.io](https://wiki.gpac.io), including [HowTos](https://github.com/gpac/gpac/wiki/Howtos).

GPAC tools are mostly wrappers around an underlying library called libgpac which can easily be embedded in your projects. The libgpac developer documentation is available at [doxygen.gpac.io](https://doxygen.gpac.io), including documentation of [JS APIs](https://doxygen.gpac.io/group__jsapi__grp.html), [Python APIs](https://doxygen.gpac.io/group__pyapi__grp.html) and [NodeJS APIs](https://doxygen.gpac.io/group__nodejs__grp.html).


## Testing
GPAC has a test suite exercising most features of the framework. The test suite is in a separate repository [https://github.com/gpac/testsuite/](https://github.com/gpac/testsuite/), but is available as a submodule of the GPAC main repository. To initialize the testsuite submodule, do `git submodule update --init`.

For more details on the test suite, read [this page](https://github.com/gpac/gpac/wiki/GPAC_tests) and check the [testsuite readme](https://github.com/gpac/testsuite).

Per-commit [build](https://buildbot.gpac.io/) and [tests results](https://tests.gpac.io) are available.


## Support, ongoing tasks and bugs

Please use [github](https://github.com/gpac/gpac/issues) for feature requests and bug reports. When filing a request there, please tag it as _feature-request_.	

## Contributing
A complex project like GPAC wouldn’t exist and persist without the support of its community. Please contribute: a nice message, supporting us in our communication, reporting issues when you see them… any gesture, even the smallest ones, counts. 

If you want to contribute to GPAC, you can find ideas at [GSoC page](https://gpac.wp.imt.fr/jobs/google-summer-of-code-ideas/) or look for a [good first issue](https://github.com/gpac/gpac/labels/good%20first%20issue). In any doubt please feel free to [contact us](mailto:contact@gpac.io).

# Team
GPAC is brought to you by an experienced team of developers with a wide track-record on media processing. 

The project is mainly developed in the MultiMedia group of [Telecom Paris](https://www.telecom-paris.fr/) with the help of many [great contributors](https://github.com/gpac/gpac/graphs/contributors).

GPAC has a peculiar story: started as a startup in NYC, GPAC gained traction from research and a nascent multimedia community as it was open-sourced in 2003. Since then we have never stopped transforming GPAC into a useful and up-to-date project, with many industrial R&D collaborations and a community of tens of thousands of users. This makes GPAC one of the few open-source multimedia projects that gathers so much diversity.


# Roadmap
Users are encouraged to use the latest tag or the master branch.

The v0.8.X release (the last one using the legacy architecture) is officially deprecated.

## V2.X
Targets:
- [ ] drop MP4Client and GF_Terminal API
- [ ] User authentication for HTTP and RTSP servers
- [ ] DASH event support
- [ ] Web integration (emscripten, Remotery UI)
- [ ] Better TTML2 / SMPTE ST 2052-1 support
- [ ] configure buffer level per filter
- [ ] GUI cleanup ?
- [ ] deprecate `:filemode` in ROUTE
- [ ] support ClearKey Content Protection (CCP)

