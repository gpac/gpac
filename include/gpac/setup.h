/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / general OS configuration file
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

#ifndef _GF_SETUP_H_
#define _GF_SETUP_H_

#ifdef __cplusplus
extern "C" {
#endif


/*WIN32 and WinCE config*/
#if defined(WIN32) || defined(_WIN32_WCE)

/*common win32 parts*/
#include <stdio.h>
#include <stdlib.h>


typedef unsigned __int64 u64;
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;
typedef __int64 s64;
typedef int s32;
typedef short s16;
typedef char s8;

#if defined(__GNUC__)
#define GFINLINE inline
#else
#define GFINLINE __inline
#endif

#define GF_PATH_SEPARATOR	'\\'
#define GF_MAX_PATH	260


/*WINCE config*/
#if defined(_WIN32_WCE)

/*winCE read-only (smaller)*/
#ifndef GPAC_READ_ONLY
#define GPAC_READ_ONLY
#endif

/*winCE always fixed-point*/
#ifndef GPAC_FIXED_POINT
#define GPAC_FIXED_POINT
#endif

/*win32 assert*/
void CE_Assert(u32 valid);
#ifndef NDEBUG
#define assert( t )	CE_Assert((unsigned int) (t) )
#else
#define assert(t)
#endif

/*performs wide->char and char->wide conversion on a buffer GF_MAX_PATH long*/
void CE_WideToChar(unsigned short *w_str, char *str);
void CE_CharToWide(char *str, unsigned short *w_str);


#define strdup _strdup
#define stricmp _stricmp
#define strnicmp _strnicmp
#define strupr _strupr

#ifndef _PTRDIFF_T_DEFINED
typedef int ptrdiff_t;
#define PTRDIFF(p1, p2, type)	((p1) - (p2))
#define _PTRDIFF_T_DEFINED
#endif

#ifndef _SIZE_T_DEFINED
typedef unsigned int size_t;
#define _SIZE_T_DEFINED
#endif

#ifndef offsetof
#define offsetof(s,m) ((size_t)&(((s*)0)->m))
#endif

#ifndef getenv
#define getenv(a) 0L
#endif

#define strupr _strupr
#define strlwr _strlwr



#else	/*END WINCE*/

/*WIN32 not-WinCE*/
#include <ctype.h>
#include <string.h>
#include <float.h>
#include <limits.h>
#include <stdarg.h>
#include <assert.h>

#endif	/*END WIN32 non win-ce*/

#else	/*end WIN32 config*/

/*UNIX likes*/

/*force large file support*/
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif
#ifndef _LARGEFILE_SOURCE
#define _LARGEFILE_SOURCE
#endif
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <limits.h>
#include <float.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>


typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef int64_t s64;
typedef int32_t s32;
typedef int16_t s16;
typedef int8_t s8;

#define GFINLINE	inline

/*sorry this was developed under w32 :)*/
#define stricmp		strcasecmp
#define strnicmp	strncasecmp

#ifndef strupr
char * my_str_upr(char *str);
#define strupr my_str_upr
#endif

#ifndef strlwr
char * my_str_lwr(char *str);
#define strlwr my_str_lwr
#endif

#define GF_PATH_SEPARATOR	'/'

#ifdef PATH_MAX
#define GF_MAX_PATH	PATH_MAX
#else
/*PATH_MAX not defined*/
#define GF_MAX_PATH	1023
#endif


#endif

/*define what's missing*/
#ifndef NULL
#define NULL 0
#endif


typedef double Double;
typedef float Float;
/* 128 bit IDs */
typedef u8 bin128[16];

#define GF_MAX_DOUBLE		DBL_MAX
#define GF_MIN_DOUBLE		-GF_MAX_DOUBLE
#define GF_MAX_FLOAT		FLT_MAX
#define GF_MIN_FLOAT		-GF_MAX_FLOAT
#define GF_EPSILON_FLOAT	FLT_EPSILON
#define GF_SHORT_MAX		SHRT_MAX
#define GF_SHORT_MIN		SHRT_MIN

#ifndef MIN
#define MIN(X, Y) ((X)<(Y)?(X):(Y))
#endif
#ifndef MAX
#define MAX(X, Y) ((X)>(Y)?(X):(Y))
#endif

#define ABSDIFF(a, b)	( ( (a) > (b) ) ? ((a) - (b)) : ((b) - (a)) )

#ifndef ABS
#define ABS(a)	( ( (a) > 0 ) ? (a) : - (a) )
#endif

#ifndef Bool
typedef u32 Bool;
#endif

#ifdef __cplusplus
}
#endif

#endif	/*_GF_SETUP_H_*/
