/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2008-2023
 *					All rights reserved
 *
 *  This file is part of GPAC
 *
 *  GPAC is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  GPAC is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */


#ifndef _GF_CONFIG_H_
#define _GF_CONFIG_H_

/*!
\addtogroup setup_grp
\brief Base data types

This section documents the base data types of GPAC.

@{
*/

#define GPAC_CONFIGURATION "(static configuration file)"



/*this file defines all common macros for libgpac compilation
  except for symbian32 which uses .mmp directives ... */

/*Configuration for visual studio, 32/64 bits */
#if defined(_WIN32) && !defined(_WIN32_WCE)

#ifndef GPAC_MP4BOX_MINI

#define GPAC_HAS_SSL

#define GPAC_HAS_QJS
//codecs
#define GPAC_HAS_JPEG
#define GPAC_HAS_PNG
#define GPAC_HAS_LIBA52
#define GPAC_HAS_FAAD
#define GPAC_HAS_JP2
#define GPAC_HAS_MAD
#define GPAC_HAS_OPENHEVC
#define GPAC_HAS_OPENSVC
#define GPAC_HAS_THEORA
#define GPAC_HAS_VORBIS
#define GPAC_HAS_XVID
#define GPAC_HAS_FFMPEG
#define GPAC_HAS_DTAPI
#define GPAC_HAS_HTTP2
#define GPAC_HAS_LIBCAPTION
#define GPAC_HAS_MPEGHDECODER
#define GPAC_HAS_LIBCACA

/*IPv6 enabled - for win32, this is evaluated at compile time, !! do not uncomment !!*/

#define GPAC_MEMORY_TRACKING

/*Win32 IPv6 is evaluated at compile time, !! do not uncomment !!*/
//#define GPAC_HAS_IPV6

/*undefined at compil time if no poll support*/
#define GPAC_HAS_POLL

#define GPAC_HAS_GLU

#ifndef GPAC_CONFIG_WIN32
#define GPAC_CONFIG_WIN32
#endif //GPAC_CONFIG_WIN32


#endif /*GPAC_MP4BOX_MINI*/

/*Configuration for WindowsCE 32 bits */
#elif defined(_WIN32_WCE)

#ifndef GPAC_FIXED_POINT
#define GPAC_FIXED_POINT
#endif

/*use intel fixed-point*/
//#define GPAC_USE_IGPP
/*use intel fixed-point with high precision*/
//#define GPAC_USE_IGPP_HP

#if defined(GPAC_USE_IGPP) && defined(GPAC_USE_IGPP_HP)
#error "Only one of GPAC_USE_IGPP and GPAC_USE_IGPP_HP can be defined"
#endif

#if !defined(GPAC_DISABLE_3D) && !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES1X)
#define GPAC_USE_GLES1X
#endif


#define GPAC_HAS_QJS
#define GPAC_HAS_JPEG
#define GPAC_HAS_PNG

/*comment this line if you don't have a GLU32 version for Windows Mobile*/
//#define GPAC_HAS_GLU


/*Configuration for Android */
#elif defined(GPAC_CONFIG_ANDROID)

#define GPAC_HAS_IPV6
#define GPAC_USE_GLES1X
/*don't use fixed-point version on Android, not needed*/
//#define GPAC_FIXED_POINT

#define GPAC_HAS_QJS
#define GPAC_HAS_JPEG
#define GPAC_HAS_PNG
#define GPAC_HAS_HTTP2
#define GPAC_HAS_IFADDRS
#define GPAC_HAS_LIBCAPTION
#define GPAC_HAS_MPEGHDECODER

/*Configuration for XCode OSX (not iOS) */
#elif defined(GPAC_CONFIG_DARWIN) && !defined(GPAC_CONFIG_IOS)

#define GPAC_HAS_IPV6
//#if !defined(__arm64__)
#define GPAC_HAS_SSL
//#endif
#define GPAC_HAS_SOCK_UN

#define GPAC_HAS_FD
#define GPAC_HAS_IFADDRS

//64-bits OSX
#ifdef __LP64__
/*! macro defined for 64-bits platforms*/
#define GPAC_64_BITS
#endif

#define GPAC_HAS_QJS
//#define GPAC_DISABLE_QJS_LIBC
#define GPAC_HAS_JPEG
#define GPAC_HAS_PNG
#define GPAC_HAS_GLU
#define GPAC_HAS_VTB
#define GPAC_HAS_HTTP2
#define GPAC_HAS_POLL
#define GPAC_HAS_LIBCACA

#define GPAC_MEMORY_TRACKING

#define GPAC_HAS_LIBCAPTION
#define GPAC_HAS_MPEGHDECODER

/*Configuration for XCode iOS*/
#elif defined(GPAC_CONFIG_DARWIN) && defined(GPAC_CONFIG_IOS)

//64-bits iOS
#ifdef __LP64__
/*! macro defined for 64-bits platforms*/
#define GPAC_64_BITS
#endif

#define GPAC_HAS_QJS
#define GPAC_HAS_JPEG
#define GPAC_HAS_PNG
#define GPAC_HAS_SOCK_UN
#define GPAC_HAS_IFADDRS

/*don't use fixed-point version on iOS, not needed*/
//#define GPAC_FIXED_POINT

//#define GPAC_USE_GLES1X
#define GPAC_USE_GLES2

// glu port available in gpac extra libs
#define GPAC_HAS_GLU

/*extra libs supported on iOS*/
#define GPAC_HAS_FAAD
#define GPAC_HAS_MAD
#define GPAC_HAS_FFMPEG
#define GPAC_HAS_SDL
#define GPAC_HAS_FREETYPE

#define GPAC_HAS_IPV6
#define GPAC_HAS_SSL
#define GPAC_DISABLE_OGG
#define GPAC_HAS_STRLCPY
#define GPAC_HAS_VTB
#define GPAC_HAS_HTTP2
#define GPAC_HAS_POLL
#define GPAC_HAS_LIBCAPTION
#define GPAC_HAS_MPEGHDECODER

/*Configuration for Symbian*/
#elif defined(__SYMBIAN32__)

#ifndef GPAC_FIXED_POINT
#define GPAC_FIXED_POINT
#endif

#define GPAC_HAS_QJS
#define GPAC_HAS_JPEG
#define GPAC_HAS_PNG

#else
#error "Unknown target platform used with static configuration file"
#endif


/*disables player */
//#define GPAC_DISABLE_COMPOSITOR

/*disables scene manager */
//#define GPAC_DISABLE_SMGR

/*disables zlib */
#ifndef GPAC_MP4BOX_MINI
//#define GPAC_DISABLE_ZLIB
#else
#define GPAC_DISABLE_ZLIB
#endif

/*disables QuickJS libc*/
//#define GPAC_DISABLE_QJS_LIBC

/*disables SVG scene graph*/
//#define GPAC_DISABLE_SVG

/*disables VRML/BIFS scene graphs*/
//#define GPAC_DISABLE_VRML

/*disables X3D scene graphs*/
//#define GPAC_DISABLE_X3D

/*disables MPEG-4 OD Framework - this only minimalize the set of OD features used, however all cannot be removed
this macro is currently defined in setup.h */
//#define GPAC_MINIMAL_ODF

/*disables BIFS coding*/
//#define GPAC_DISABLE_BIFS

/*disables LASeR coder*/
//#define GPAC_DISABLE_LASER
//#define GPAC_DISABLE_SAF

/*disables BIFS Engine support*/
//#define GPAC_DISABLE_SENG

/*disables Cubic QTVR importing*/
//#define GPAC_DISABLE_QTVR

/*disables AVILib support*/
//#define GPAC_DISABLE_AVILIB

/*disables OGG support*/
//#define GPAC_DISABLE_OGG

/*disables MPEG2 PS support*/
//#define GPAC_DISABLE_MPEG2PS

/*disables MPEG2 TS demux support*/
//#define GPAC_DISABLE_MPEG2TS

/*disables MPEG2 TS Mux support*/
//#define GPAC_DISABLE_MPEG2TS_MUX

/*disables all media import functions*/
//#define GPAC_DISABLE_MEDIA_IMPORT

/*disable all AV parsing functions*/
//#define GPAC_DISABLE_AV_PARSERS

/*disables all media export functions*/
//#define GPAC_DISABLE_MEDIA_EXPORT

/*disables SWF importer*/
//#define GPAC_DISABLE_SWF_IMPORT

/*disables all media export functions*/
//#define GPAC_DISABLE_SCENE_STATS

/*disables scene -> MP4 encoder*/
//#define GPAC_DISABLE_SCENE_ENCODER

/*disables ISOM -> scene decoder*/
//#define GPAC_DISABLE_LOADER_ISOM

/*disables BT/WRL/X3DV -> scene decoder*/
//#define GPAC_DISABLE_LOADER_BT

/*disables XMTA/X3D -> scene decoder*/
//#define GPAC_DISABLE_LOADER_XMT

/*disables crypto tools*/
//#define GPAC_DISABLE_CRYPTO

/*disables all ISO FF*/
//#define GPAC_DISABLE_ISOM

/*disables ISO FF hint tracks*/
//#define GPAC_DISABLE_ISOM_HINTING

/*disables ISO FF writing*/
//#define GPAC_DISABLE_ISOM_WRITE

/*disables ISO FF fragments*/
//#define GPAC_DISABLE_ISOM_FRAGMENTS

/*disables scene graph */
//#define GPAC_DISABLE_SCENEGRAPH

/*disables scene graph textual dump*/
//#define GPAC_DISABLE_SCENE_DUMP

/*disables OD graph textual dump*/
//#define GPAC_DISABLE_OD_DUMP

/*disables OD graph textual dump*/
//#define GPAC_DISABLE_ISOM_DUMP

/*disables IETF RTP/SDP/RTSP*/
//#define GPAC_DISABLE_STREAMING

/*disables Timed Text support */
//#define GPAC_DISABLE_TTXT

/*disables TTML */
//#define GPAC_DISABLE_TTML

/*disables WebVTT */
//#define GPAC_DISABLE_VTT

/*disables DASH MPD */
//#define GPAC_DISABLE_MPD

/*disables VOBSUB */
//#define GPAC_DISABLE_VOBSUB

/*! @} */

#endif		/*_GF_CONFIG_H_*/
