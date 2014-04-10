/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre 
 *			Copyright (c) Telecom ParisTech 2008-2012
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

#define GPAC_CONFIGURATION "(static configuration file)"

/*this file defines all common macros for libgpac compilation
  except for symbian32 which uses .mmp directives ... */


/*visual studio and xcode*/
#if defined(WIN32) || defined(_WIN32_WCE) || defined(GPAC_CONFIG_DARWIN) || defined(GPAC_CONFIG_ANDROID) 

/*enables GPAC memory tracking in debug mode only*/
#if defined(DEBUG) || defined(_DEBUG)
#define GPAC_MEMORY_TRACKING
#endif

/*SSL enabled - no 64 bit support yet*/
#if defined(WIN32) && !defined(_WIN64)
#define GPAC_HAS_SSL
#endif

/*spidermonkey enabled*/
#define GPAC_HAS_SPIDERMONKEY
#ifdef GPAC_CONFIG_DARWIN
#define MOZILLA_1_8_BRANCH
#define XP_UNIX
#endif

/*libjpeg enabled*/
#define GPAC_HAS_JPEG
/*pnj enabled*/
#define GPAC_HAS_PNG

/*IPv6 enabled - for win32, this is evaluated at compile time, !! do not uncomment !!*/


//iOS compilation
#if defined(GPAC_CONFIG_DARWIN) && defined(GPAC_IPHONE)

#define GPAC_USE_OGL_ES
#define GPAC_FIXED_POINT
#define GPAC_HAS_GLU

/*lazy definition of extra libs for iOS*/
#define GPAC_HAS_FAAD
//#define GPAC_HAS_MAD
#define GPAC_HAS_SDL
#define GPAC_HAS_FREETYPE

#endif //end iOS flags


//OSX compilation
#if defined(GPAC_CONFIG_DARWIN) && !defined(GPAC_IPHONE)

#define GPAC_HAS_IPV6
#define GPAC_HAS_SSL

#ifdef __LP64__
#define GPAC_64_BITS
#endif

#endif  //end OSX flags


//Android test compilation without any extra lib
#if defined(GPAC_CONFIG_ANDROID) 
#define GPAC_ANDROID

#define GPAC_HAS_IPV6
#define GPAC_USE_OGL_ES
#define GPAC_FIXED_POINT

#undef GPAC_HAS_SPIDERMONKEY
#undef GPAC_HAS_PNG
#undef GPAC_HAS_JPEG

#endif  //end OSX flags



//WinCE flags
#if defined(_WIN32_WCE)

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

#if !defined(GPAC_DISABLE_3D) && !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_OGL_ES)
#define GPAC_USE_OGL_ES
#endif

#endif //WinCE flags



#endif /*defined(WIN32) || defined(_WIN32_WCE) || defined(GPAC_CONFIG_DARWIN)*/


#if defined(__SYMBIAN32__)

#ifndef GPAC_FIXED_POINT
#define GPAC_FIXED_POINT
#endif

#endif

#if defined(_WIN32_WCE)
/*comment this line if you don't have a GLU32 version for Windows Mobile*/
//#define GPAC_HAS_GLU
#elif defined(WIN32)
#define GPAC_HAS_GLU
#endif

#if defined(_WIN64)
#define GPAC_X64
#endif

/*disables SVG scene graph*/
//#define GPAC_DISABLE_SVG

/*disables VRML/BIFS scene graphs*/
//#define GPAC_DISABLE_VRML

/*disables X3D scene graphs*/
//#define GPAC_DISABLE_X3D

/*disables MPEG-4 OD Framework - this only minimalize the set of OD features used, however all cannot be removed*/
//#define GPAC_MINIMAL_ODF

/*disables BIFS coding*/
//#define GPAC_DISABLE_BIFS

/*disables LASeR coder*/
//#define GPAC_DISABLE_LASER

/*disables BIFS Engine support - TODO - merge DIMS and LASeR into BENG and rename it*/
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

/*disables mcrypt*/
//#define GPAC_DISABLE_MCRYPT

/*disables all ISO FF*/
//#define GPAC_DISABLE_ISOM

/*disables ISO FF hint tracks*/
//#define GPAC_DISABLE_ISOM_HINTING

/*disables ISO FF writing*/
//#define GPAC_DISABLE_ISOM_WRITE

/*disables ISO FF fragments*/
//#define GPAC_DISABLE_ISOM_FRAGMENTS

/*disables scene graph textual dump*/
//#define GPAC_DISABLE_SCENE_DUMP

/*disables OD graph textual dump*/
//#define GPAC_DISABLE_OD_DUMP

/*disables OD graph textual dump*/
//#define GPAC_DISABLE_ISOM_DUMP

/*disables IETF RTP/SDP/RTSP*/
//#define GPAC_DISABLE_STREAMING

/*disables dashclient */
//#define GPAC_DISABLE_DASH_CLIENT

/*disables Timed Text support */
//#define GPAC_DISABLE_TTXT

#endif		/*_GF_CONFIG_H_*/

