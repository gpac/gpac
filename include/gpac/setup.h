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

#ifdef GPAC_HAVE_CONFIG_H

#if defined(__GNUC__)
#include <gpac/internal/config.h>
#else
#include <gpac/internal/config_static.h>
#endif

#endif

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
#define GF_MAX_PATH	1024

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
#ifndef assert

void CE_Assert(u32 valid, char *file, u32 line);

#ifndef NDEBUG
#define assert( t )	CE_Assert((unsigned int) (t), __FILE__, __LINE__ )
#else
#define assert(t)
#endif

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

//#define GPAC_DISABLE_LOG

#else	/*END WINCE*/

/*WIN32 not-WinCE*/
#include <ctype.h>
#include <string.h>
#include <float.h>
#include <limits.h>
#include <stdarg.h>
#include <assert.h>


#endif	/*END WIN32 non win-ce*/
/*end WIN32 config*/

/*start SYMBIAN config*/
#elif defined(__SYMBIAN32__)

#define GFINLINE inline
#define GF_PATH_SEPARATOR	'\\'

/*we must explicitely export our functions...*/
#define GF_EXPORT EXPORT_C

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <limits.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>

#ifdef __SERIES60_3X__

typedef unsigned __int64 u64;
typedef __int64 s64;

#else 

/*FIXME - we don't have 64bit support here we should get rid of all 64bits divisions*/
//typedef unsigned long long u64;
//typedef long long s64;

typedef unsigned int u64;
typedef signed int s64;

#endif	/*symbian 8*/

/*SYMBIAN always fixed-point*/
#ifndef GPAC_FIXED_POINT
#define GPAC_FIXED_POINT
#endif


typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;
typedef int s32;
typedef short s16;
typedef signed char s8;

#pragma mpwc_relax on

#define GF_MAX_PATH	260

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

#ifndef DBL_MAX
#include <libc/ieeefp.h>
#define DBL_MAX	(__IEEE_DBL_MAXPOWTWO)
#endif

#ifndef FLT_MAX
#include <libc/ieeefp.h>
#define FLT_MAX	(__IEEE_FLT_MAXPOWTWO)
#endif

#ifndef FLT_EPSILON
#define FLT_EPSILON 1
#endif

/*end SYMBIAN config*/

#else 

/*UNIX likes*/

#ifdef GPAC_HAVE_CONFIG_H
#include "config.h"
#endif


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


#endif /* end platform specific Win32/WinCE/UNIX*/

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

/*GPAC memory tracking*/
#define GPAC_MEMORY_TRACKING	0

#if GPAC_MEMORY_TRACKING
void *gf_malloc(size_t size);
void *gf_realloc(void *ptr, size_t size);
void gf_free(void *ptr);
char *gf_strdup(const char *str);

#undef malloc
#define malloc gf_malloc
#undef realloc
#define realloc gf_realloc
#undef free
#define free gf_free
#undef strdup
#define strdup gf_strdup

#endif
/*end GPAC memory tracking*/

#if defined (WIN32) && !defined(__GNUC__)
#define LLD "%I64d"
#define LLU "%I64u"
#define LLD_CAST
#define LLU_CAST
#elif defined (__SYMBIAN32__)
#define LLD "%d"
#define LLU "%u"
#define LLD_CAST (u32)
#define LLU_CAST (s32)
#else
#define LLD "%lld"
#define LLU "%llu"
#define LLD_CAST
#define LLU_CAST
#endif


#ifndef GF_EXPORT
/*use def files for windows or let compiler decide*/
#define GF_EXPORT 
#endif


#ifdef __cplusplus
}
#endif

#endif	/*_GF_SETUP_H_*/
