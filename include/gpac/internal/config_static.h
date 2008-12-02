/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005 
 *			Copyright (c) ENST 2008 - 
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

/*this file defines all common macros for libgpac compilation*/

/*platform is big endian*/
//#define GPAC_BIG_ENDIAN

/*SSL enabled*/
//#define GPAC_HAS_SSL

/*spidermonkey enabled*/
#define GPAC_HAS_SPIDERMONKEY

/*libjpeg enabled*/
#define GPAC_HAS_JPEG

/*pnj enabled*/
#define GPAC_HAS_PNG

/*IPv6 enabled - for win32, this is evaluated at compile time, !! do not uncomment !!*/
//#define GPAC_HAS_IPV6

/*SVG disabled*/
//#define GPAC_DISABLE_SVG

/*3D compositor disabled*/
//#define GPAC_DISABLE_3D

/*use TinyGL instead of OpenGL*/
//#define GPAC_USE_TINYGL

/*use OpenGL ES instead of OpenGL*/
//#define GPAC_USE_OGL_ES


#if defined(_WIN32_WCE)

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

#endif


#if !defined(GPAC_USE_OGL_ES) && !defined(GPAC_USE_TINYGL)
#define GPAC_HAS_GLU
#endif



#endif		/*_GF_CONFIG_H_*/

