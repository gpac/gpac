/*
 * Copyright (c) TELECOM ParisTech 2019
 do not include in gpac, only here to create doxygen group for doc ordering
 */


//define doxygen groups for their order
/*!
\defgroup utils_grp Core Tools
 \defgroup setup_grp Base data types
 \ingroup utils_grp
 \defgroup libsys_grp Library configuration
 \ingroup utils_grp
 \defgroup mem_grp Memory Management
 \ingroup utils_grp
 \defgroup errors_grp Error codes
 \ingroup utils_grp
 \defgroup cst_grp Constants
 \ingroup utils_grp
 \defgroup log_grp Logging tools
 \ingroup utils_grp
 \defgroup bs_grp Bitstream IO
 \ingroup utils_grp
 \defgroup list_grp Generic List object
 \ingroup utils_grp
 \defgroup time_grp Local and Network time
 \ingroup utils_grp

 \defgroup net_grp Network
 \ingroup utils_grp
 \defgroup thr_grp Process and Threads
 \ingroup utils_grp
 \defgroup math_grp Math
 \ingroup utils_grp
 \defgroup download_grp Downloader
 \ingroup net_grp
 \defgroup cache_grp DownloaderCache
 \ingroup download_grp
 \defgroup osfile_grp File System
 \ingroup utils_grp
 \defgroup bascod_grp base64 encoding
 \ingroup utils_grp
 \defgroup color_grp Color
 \ingroup utils_grp
 \defgroup cfg_grp Configuration File
 \ingroup utils_grp
 \defgroup cst_grp Constants
 \ingroup utils_grp
 \defgroup lang_grp Languages
 \ingroup utils_grp
 \defgroup tok_grp Tokenizer
 \ingroup utils_grp
 \defgroup hash_grp Hash and Compression
 \ingroup utils_grp
 \defgroup utf_grp Unicode and UTF
 \ingroup utils_grp
 \addtogroup xml_grp XML
 \ingroup utils_grp
 \defgroup miscsys_grp Misc tools
 \ingroup utils_grp
 \defgroup sysmain_grp Main tools
 \ingroup utils_grp



\defgroup media_grp Media Tools

\defgroup iso_grp ISO Base Media File

\defgroup filters_grp Filter Management

\defgroup evg_grp Vector Graphics Rendering

\defgroup scene_grp Scene Graph

\defgroup mpeg4sys_grp MPEG-4 Systems

\defgroup playback_grp Media Player

\defgroup jsapi_grp JavaScript APIs
\brief JavaScript API available in GPAC

Parts of the GPAC code can be scriptable using JavaScript. This part of the documentation describes the various APIs used in GPAC.

For SVG and DOM scenegraph API, see https://www.w3.org/TR/SVGTiny12/svgudom.html.

For BIFS and VRML scenegraph, see https://www.web3d.org/documents/specifications/14772/V2.0/part1/javascript.html

SVG and BIFS APIs are automatically loaded when loading an SVG or BIFS scene with script nodes. They cannot be loaded in another way.


The JSFilter API is loaded through a dedicated filter, see `gpac -h jsf`.

All other APIs shall be loaded by the parent script (scene or JSFilter) as ECMAScript modules using the `import` directive.

GPAC uses the amazing QuickJS engine for JavaScript support: https://bellard.org/quickjs/quickjs.html. Although not JIT-enabled, it is small, fast and cross-platform which ensures the same behaviour on all devices.

The module loader is the same as the QuickJS libc module loader (https://bellard.org/quickjs/quickjs.html#Modules), except that it checks for .so, .dll or .dylib extensions for C modules.

This means that JS C modules as defined in QuickJS could also be used (https://bellard.org/quickjs/quickjs.html#C-Modules)
\warning support for C modules is still not fully tested

There is currently no support for non-local modules (http, https ...).

GPAC constants used in the API (error code, property types, specific flags for functions) are exported:
- using the same name as native code, e.g. GF_STATS_LOCAL, GF_FILTER_SAP_1, etc...
- in the global object of the javascript context

Unless indicated otherwise, all errors are handled through exceptions. An exception object contains a code (integer) attribute and optionnally a message (string) attribute.


Types and interfaces are described using WebIDL, see https://heycam.github.io/webidl/, with some slight modifications.
\warning These IDL files are only intended to document the APIs, and are likely useless for other purposes.

*/

