/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2024
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


/*!
\file "gpac/setup.h"
\brief Base data types of GPAC.

This file contains the base data types of GPAC, depending on the platform.


*/


/*!
\addtogroup setup_grp
\brief Base data types of GPAC.

This section documents the base data types of GPAC, as well as some macros wrapping platform-specific functionalities.
For better portability, only use the base data types defined here.

@{
*/

/*This is to handle cases where config.h is generated at the root of the gpac build tree (./configure)
This is only needed when building libgpac and modules when libgpac is not installed*/
#ifdef GPAC_HAVE_CONFIG_H
# include "config.h"
#else
# include <gpac/configuration.h>
#endif


/*WIN32 and WinCE config*/
#if defined(WIN32) || defined(_WIN32_WCE)

/*common win32 parts*/
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#if defined(_WIN64) && !defined(GPAC_64_BITS)
/*! macro defined for 64-bits platforms*/
#define GPAC_64_BITS
#endif

/*! 64 bit unsigned integer*/
typedef unsigned __int64 u64;
/*! 64 bit signed integer*/
typedef __int64 s64;
/*! 32 bit unsigned integer*/
typedef unsigned int u32;
/*! 32 bit signed integer*/
typedef int s32;
/*! 16 bit unsigned integer*/
typedef unsigned short u16;
/*! 16 bit signed integer*/
typedef short s16;
/*! 8 bit unsigned integer*/
typedef unsigned char u8;
/*! 8 bit signed integer*/
typedef char s8;

#if defined(__GNUC__)
/*! macro for cross-platform inlining of functions*/
#define GFINLINE inline
#else
/*! macro for cross-platform inlining of functions*/
#define GFINLINE __inline
#endif

/*! default path separator of the current platform*/
#define GF_PATH_SEPARATOR	'\\'
/*! default max filesystem path size of the current platform*/
#define GF_MAX_PATH	1024

/*WINCE config*/
#if defined(_WIN32_WCE)

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
#define mkdir _mkdir
#define snprintf _snprintf
#define memccpy _memccpy


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

/*
#define GPAC_DISABLE_LOG
*/
#else	/*END WINCE*/

/*WIN32 not-WinCE*/
#include <ctype.h>
#include <string.h>
#include <float.h>
#include <limits.h>
#include <stdarg.h>
#include <assert.h>

#define snprintf _snprintf

/*	the _USING_V110_SDK71_ macro will be defined when using
	msvc toolsets like v140_xp, v141_xp, etc.
*/

#if ( (defined(WINVER) && WINVER <= 0x0502) || _USING_V110_SDK71_)
#define GPAC_BUILD_FOR_WINXP
#endif

/*! minimum api versions to use for windows apis with mingw */
#if (defined(__MINGW32__) || defined(__MINGW64__))
#if (defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0601)
#undef _WIN32_WINNT
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#endif

#endif	/*END WIN32 non win-ce*/
/*end WIN32 config*/

/*start SYMBIAN config*/
#elif defined(__SYMBIAN32__)

/*! macro for cross-platform inlining of functions*/
#define GFINLINE inline
/*! default path separator of the current platform*/
#define GF_PATH_SEPARATOR	'\\'

/*we must explicitly export our functions...*/

/*! macro for cross-platform signaling of exported function of libgpac*/
#define GF_EXPORT EXPORT_C

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <limits.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>

#ifdef __SERIES60_3X__

/*! 64 bit unsigned integer*/
typedef unsigned __int64 u64;
/*! 64 bit signed integer*/
typedef __int64 s64;

#else

/*FIXME - we don't have 64bit support here we should get rid of all 64bits divisions*/
/*
typedef unsigned long long u64;
typedef long long s64;
*/

/*! 64 bit unsigned integer*/
typedef unsigned int u64;
/*! 64 bit signed integer*/
typedef signed int s64;

#endif	/*symbian 8*/


/*! 32 bit unsigned integer*/
typedef unsigned int u32;
/*! 32 bit signed integer*/
typedef int s32;
/*! 16 bit unsigned integer*/
typedef unsigned short u16;
/*! 16 bit signed integer*/
typedef short s16;
/*! 8 bit unsigned integer*/
typedef unsigned char u8;
/*! 8 bit signed integer*/
typedef signed char s8;

#pragma mpwc_relax on

/*! default max filesystem path size of the current platform*/
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

/*! max file offset bits*/
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif
/*! largefile*/
#ifndef _LARGEFILE_SOURCE
#define _LARGEFILE_SOURCE
#endif
/*! largefile64*/
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <limits.h>
#include <float.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>

/*! file descriptor support*/
#define GPAC_HAS_FD

#if __APPLE__ && defined GPAC_CONFIG_IOS
#include <TargetConditionals.h>
#endif

/*! 64 bit unsigned integer*/
typedef uint64_t u64;
/*! 64 bit signed integer*/
typedef int64_t s64;
/*! 32 bit unsigned integer*/
typedef uint32_t u32;
/*! 32 bit signed integer*/
typedef int32_t s32;
/*! 16 bit unsigned integer*/
typedef uint16_t u16;
/*! 16 bit signed integer*/
typedef int16_t s16;
/*! 8 bit unsigned integer*/
typedef uint8_t u8;
/*! 8 bit signed integer*/
typedef int8_t s8;

/*! macro for cross-platform inlining of functions*/
#define GFINLINE	inline

/*! use stricmp */
#define stricmp		strcasecmp
/*! use strnicmp */
#define strnicmp	strncasecmp

#ifndef strupr
/*! gets upper case
\param str input string
\return uper case to free*/
char *my_str_upr(char *str);
/*! use strupr */
#define strupr my_str_upr
#endif

#ifndef strlwr
/*! gets lower case
\param str input string
\return lower case to free*/
char * my_str_lwr(char *str);
/*! use strulwr */
#define strlwr my_str_lwr
#endif

/*! default path separator of the current platform*/
#define GF_PATH_SEPARATOR	'/'

#ifdef PATH_MAX
/*! default max filesystem path size of the current platform*/
#define GF_MAX_PATH	PATH_MAX
#else
/*! default max filesystem path size of the current platform*/
#define GF_MAX_PATH	1023
#endif


#endif /* end platform specific Win32/WinCE/UNIX*/


//! @cond Doxygen_Suppress

/*define what's missing*/

#ifndef NULL
#define NULL 0
#endif

//! @endcond


/*! Double-precision floating point number*/
typedef double Double;
/*! Single-precision floating point number*/
typedef float Float;
/*! 128 bit IDs */
typedef u8 bin128[16];
/*! max positive possible value for Double*/
#define GF_MAX_DOUBLE		DBL_MAX
/*! max negative possible value for Double*/
#define GF_MIN_DOUBLE		-GF_MAX_DOUBLE
/*! max positive possible value for Float*/
#define GF_MAX_FLOAT		FLT_MAX
/*! max negative possible value for Float*/
#define GF_MIN_FLOAT		-GF_MAX_FLOAT
/*! smallest possible value for float*/
#define GF_EPSILON_FLOAT	FLT_EPSILON
/*! max possible value for s16*/
#define GF_SHORT_MAX		SHRT_MAX
/*! min possible value for s16*/
#define GF_SHORT_MIN		SHRT_MIN
/*! max possible value for u32*/
#define GF_UINT_MAX			UINT_MAX
/*! max possible value for s32*/
#define GF_INT_MAX			INT_MAX
/*! min possible value for s32*/
#define GF_INT_MIN			INT_MIN

#ifndef MIN
/*! get the smallest of two numbers*/
#define MIN(X, Y) ((X)<(Y)?(X):(Y))
#endif
#ifndef MAX
/*! get the biggest of two numbers*/
#define MAX(X, Y) ((X)>(Y)?(X):(Y))
#endif

/*! get the absolute difference betwee two numbers*/
#define ABSDIFF(a, b)	( ( (a) > (b) ) ? ((a) - (b)) : ((b) - (a)) )

#ifndef ABS
/*! get the absolute value of a number*/
#define ABS(a)	( ( (a) > 0 ) ? (a) : - (a) )
#endif

#ifndef Bool
/*! boolean value*/
typedef enum {
	GF_FALSE = 0,
	GF_TRUE
} Bool;
#endif

/*! 32 bit fraction*/
typedef struct {
	s32 num;
	u32 den;
} GF_Fraction;

/*! 64 bit fraction*/
typedef struct {
	s64 num;
	u64 den;
} GF_Fraction64;

#if (defined (WIN32) || defined (_WIN32_WCE)) && (defined(__MINGW32__) || !defined(__GNUC__))

#if defined(__MINGW32__)

#ifdef __USE_MINGW_ANSI_STDIO
#undef __USE_MINGW_ANSI_STDIO
#endif
#define __USE_MINGW_ANSI_STDIO 1

/*! macro for cross-platform suffix used for formatting s64 integers in logs and printf routines*/
#define LLD_SUF "lld"
/*! macro for cross-platform suffix used for formatting u64 integers in logs and printf routines*/
#define LLU_SUF "llu"
/*! macro for cross-platform suffix used for formatting u64 integers as hex in logs and printf routines*/
#define LLX_SUF "llx"
#else
/*! macro for cross-platform suffix used for formatting s64 integers in logs and printf routines*/
#define LLD_SUF "I64d"
/*! macro for cross-platform suffix used for formatting u64 integers in logs and printf routines*/
#define LLU_SUF "I64u"
/*! macro for cross-platform suffix used for formatting u64 integers as hex in logs and printf routines*/
#define LLX_SUF "I64x"
#endif

#ifdef _WIN64
/*! macro for cross-platform casting a pointer to an integer*/
#define PTR_TO_U_CAST (u64)
#else
/*! macro for cross-platform casting a pointer to an integer*/
#define PTR_TO_U_CAST (u32)
#endif

#elif defined (__SYMBIAN32__)

/*! macro for cross-platform suffix used for formatting s64 integers in logs and printf routines*/
#define LLD_SUF "d"
/*! macro for cross-platform suffix used for formatting u64 integers in logs and printf routines*/
#define LLU_SUF "u"
/*! macro for cross-platform suffix used for formatting u64 integers as hex in logs and printf routines*/
#define LLX_SUF "x"

/*! macro for cross-platform casting a pointer to an integer*/
#define PTR_TO_U_CAST (u32)

/*seems that even though _LP64 is defined in OSX, %ll modifiers are still needed*/
#elif defined(__DARWIN__) || defined(__APPLE__)

/*! macro for cross-platform suffix used for formatting s64 integers in logs and printf routines*/
#define LLD_SUF "lld"
/*! macro for cross-platform suffix used for formatting u64 integers in logs and printf routines*/
#define LLU_SUF "llu"
/*! macro for cross-platform suffix used for formatting u64 integers as hex in logs and printf routines*/
#define LLX_SUF "llx"

#ifdef __LP64__ /* Mac OS 64 bits */
/*! macro for cross-platform casting a pointer to an integer*/
#define PTR_TO_U_CAST (u64)
#else
/*! macro for cross-platform casting a pointer to an integer*/
#define PTR_TO_U_CAST (u32)
#endif

#elif defined(_LP64) /*Unix 64 bits*/

/*! macro for cross-platform suffix used for formatting s64 integers in logs and printf routines*/
#define LLD_SUF "ld"
/*! macro for cross-platform suffix used for formatting u64 integers in logs and printf routines*/
#define LLU_SUF "lu"
/*! macro for cross-platform suffix used for formatting u64 integers as hex in logs and printf routines*/
#define LLX_SUF "lx"

/*! macro for cross-platform casting a pointer to an integer*/
#define PTR_TO_U_CAST (u64)

#else /*Unix 32 bits*/

/*! macro for cross-platform suffix used for formatting s64 integers in logs and printf routines*/
#define LLD_SUF "lld"
/*! macro for cross-platform suffix used for formatting u64 integers in logs and printf routines*/
#define LLU_SUF "llu"
/*! macro for cross-platform suffix used for formatting u64 integers as hex in logs and printf routines*/
#define LLX_SUF "llx"

/*! macro for cross-platform casting a pointer to an integer*/
#define PTR_TO_U_CAST (u32)

#endif

#ifndef PTR_TO_U_CAST
/*! macro for cross-platform casting a pointer to an integer*/
#define PTR_TO_U_CAST
#endif

/*! macro for cross-platform formatting of s64 integers in logs and printf routines*/
#define LLD "%" LLD_SUF
/*! macro for cross-platform formatting of u64 integers in logs and printf routines*/
#define LLU "%" LLU_SUF
/*! macro for cross-platform formatting of u64 integers as hexadecimal in logs and printf routines*/
#define LLX "%" LLX_SUF


#if !defined(GF_EXPORT)
#if defined(GPAC_CONFIG_EMSCRIPTEN)
//global include for using EM_ASM
#include <emscripten/emscripten.h>
#define GF_EXPORT EMSCRIPTEN_KEEPALIVE
#elif defined(__GNUC__) && __GNUC__ >= 4 && !defined(GPAC_CONFIG_IOS)
/*! macro for cross-platform signaling of exported function of libgpac*/
#define GF_EXPORT __attribute__((visibility("default")))
#else
/*use def files for windows or let compiler decide*/

/*! macro for cross-platform signaling of exported function of libgpac*/
#define GF_EXPORT
#endif
#endif


//! @cond Doxygen_Suppress

#if defined(GPAC_CONFIG_IOS)
#define GPAC_STATIC_MODULES
#endif

/*dependency checks on macros*/

#ifdef GPAC_DISABLE_NETWORK
# ifndef GPAC_DISABLE_STREAMING
# define GPAC_DISABLE_STREAMING
# endif
# ifndef GPAC_DISABLE_ROUTE
# define GPAC_DISABLE_ROUTE
# endif
# ifdef GPAC_HAS_IPV6
# undef GPAC_HAS_IPV6
# endif
# ifdef GPAC_HAS_SOCK_UN
# undef GPAC_HAS_SOCK_UN
# endif
# ifndef GPAC_DISABLE_NETCAP
# define GPAC_DISABLE_NETCAP
# endif
#endif


#if !defined(GPAC_DISABLE_NETWORK) || defined(GPAC_CONFIG_EMSCRIPTEN)
#define GPAC_USE_DOWNLOADER
#endif


#ifdef GPAC_DISABLE_ZLIB
# define GPAC_DISABLE_LOADER_BT
# define GPAC_DISABLE_SWF_IMPORT
#endif

#ifdef GPAC_DISABLE_EVG
# ifndef GPAC_DISABLE_COMPOSITOR
# define GPAC_DISABLE_COMPOSITOR
# endif
# ifndef GPAC_DISABLE_SVG
# define GPAC_DISABLE_SVG
# endif
# ifndef GPAC_DISABLE_FONTS
# define GPAC_DISABLE_FONTS
# endif
#endif

#ifdef GPAC_DISABLE_VRML
# ifndef GPAC_DISABLE_BIFS
# define GPAC_DISABLE_BIFS
# endif
# ifndef GPAC_DISABLE_QTVR
# define GPAC_DISABLE_QTVR
# endif
# ifndef GPAC_DISABLE_X3D
# define GPAC_DISABLE_X3D
# endif
# ifndef GPAC_DISABLE_LOADER_BT
# define GPAC_DISABLE_LOADER_BT
# endif
# ifndef GPAC_DISABLE_LOADER_XMT
# define GPAC_DISABLE_LOADER_XMT
# endif
#endif

#ifdef GPAC_DISABLE_SVG
# ifndef GPAC_DISABLE_LASER
# define GPAC_DISABLE_LASER
# endif
#endif


#ifdef GPAC_DISABLE_AV_PARSERS
# ifndef GPAC_DISABLE_MPEG2PS
# define GPAC_DISABLE_MPEG2PS
# endif
# ifndef GPAC_DISABLE_ISOM_HINTING
# define GPAC_DISABLE_ISOM_HINTING
# endif
#endif

#ifdef GPAC_DISABLE_ISOM
# ifndef GPAC_DISABLE_ISOM_WRITE
# define GPAC_DISABLE_ISOM_WRITE
# endif
# ifndef GPAC_DISABLE_ISOM_HINTING
# define GPAC_DISABLE_ISOM_HINTING
# endif
# ifndef GPAC_DISABLE_ISOM_FRAGMENTS
# define GPAC_DISABLE_ISOM_FRAGMENTS
# endif
# ifndef GPAC_DISABLE_SCENE_ENCODER
# define GPAC_DISABLE_SCENE_ENCODER
# endif
# ifndef GPAC_DISABLE_ISOM_DUMP
# define GPAC_DISABLE_ISOM_DUMP
# endif
# ifndef GPAC_DISABLE_LOADER_ISOM
# define GPAC_DISABLE_LOADER_ISOM
# endif
# ifndef GPAC_DISABLE_MEDIA_EXPORT
# define GPAC_DISABLE_MEDIA_EXPORT
# endif
#endif

#ifdef GPAC_DISABLE_ISOM_WRITE
# ifndef GPAC_DISABLE_MEDIA_IMPORT
# define GPAC_DISABLE_MEDIA_IMPORT
# endif
# ifndef GPAC_DISABLE_QTVR
# define GPAC_DISABLE_QTVR
# endif
# ifndef GPAC_DISABLE_ISOM_HINTING
# define GPAC_DISABLE_ISOM_HINTING
# endif
# ifndef GPAC_DISABLE_SCENE_ENCODER
# define GPAC_DISABLE_SCENE_ENCODER
# endif
#endif

#ifdef GPAC_DISABLE_STREAMING
# ifndef GPAC_DISABLE_ISOM_HINTING
# define GPAC_DISABLE_ISOM_HINTING
# endif
#endif

#ifdef GPAC_DISABLE_BIFS
# ifndef GPAC_DISABLE_BIFS_ENC
# define GPAC_DISABLE_BIFS_ENC
# endif
#endif

#if defined(GPAC_DISABLE_BIFS_ENC) && defined(GPAC_DISABLE_LASER)
# ifndef GPAC_DISABLE_LOADER_ISOM
# define GPAC_DISABLE_LOADER_ISOM
# endif
# ifndef GPAC_DISABLE_SENG
# define GPAC_DISABLE_SENG
# endif
#endif

#ifdef GPAC_DISABLE_MEDIA_IMPORT
# ifndef GPAC_DISABLE_VTT
# define GPAC_DISABLE_VTT
# endif
#endif

//we currently disable all extra IPMP/IPMPX/OCI/extra MPEG-4 descriptors parsing
#ifndef GPAC_MINIMAL_ODF
#define GPAC_MINIMAL_ODF
#endif

#ifdef GPAC_CONFIG_EMSCRIPTEN
#define EM_CAST_PTR	(int)
#endif

#if defined(GPAC_DISABLE_DASHER) && defined(GPAC_DISABLE_DASHIN)
#define GPAC_DISABLE_MPD
#endif

//define this to remove most of built-in doc of libgpac - for now filter description and help is removed, but argument help is not
//#define GPAC_DISABLE_DOC


//! @endcond

/*! @} */

/*!
\addtogroup mem_grp
\brief Memory management

GPAC can use its own memory tracker, depending on compilation option. It is recommended to use only the functions
defined in this section to allocate and free memory whenever developing within the GPAC library.

\warning these functions shall only be used after initializing the library using \ref gf_sys_init
@{
*/
/*GPAC memory tracking*/
#if defined(GPAC_MEMORY_TRACKING)


void *gf_mem_malloc(size_t size, const char *filename, int line);
void *gf_mem_calloc(size_t num, size_t size_of, const char *filename, int line);
void *gf_mem_realloc(void *ptr, size_t size, const char *filename, int line);
void gf_mem_free(void *ptr, const char *filename, int line);
char *gf_mem_strdup(const char *str, const char *filename, int line);
void gf_memory_print(void); /*prints the state of current allocations*/
u64 gf_memory_size(); /*gets memory allocated in bytes*/

/*! free memory allocated with gpac*/
#define gf_free(ptr) gf_mem_free(ptr, __FILE__, __LINE__)
/*! allocates memory, shall be freed using \ref gf_free*/
#define gf_malloc(size) gf_mem_malloc(size, __FILE__, __LINE__)
/*! allocates memory array, shall be freed using \ref gf_free*/
#define gf_calloc(num, size_of) gf_mem_calloc(num, size_of, __FILE__, __LINE__)
/*! duplicates string, shall be freed using \ref gf_free*/
#define gf_strdup(s) gf_mem_strdup(s, __FILE__, __LINE__)
/*! reallocates memory, shall be freed using \ref gf_free*/
#define gf_realloc(ptr1, size) gf_mem_realloc(ptr1, size, __FILE__, __LINE__)

#else

/*! free memory allocated with gpac
\param ptr same as free()
*/
void gf_free(void *ptr);

/*! allocates memory, shall be freed using \ref gf_free
\param size same as malloc()
\return address of allocated block
*/
void* gf_malloc(size_t size);

/*! allocates memory array, shall be freed using \ref gf_free
\param num same as calloc()
\param size_of same as calloc()
\return address of allocated block
*/
void* gf_calloc(size_t num, size_t size_of);

/*! duplicates string, shall be freed using \ref gf_free
\param str same as strdup()
\return duplicated string
*/
char* gf_strdup(const char *str);

/*! reallocates memory, shall be freed using \ref gf_free
\param ptr same as realloc()
\param size same as realloc()
\return address of reallocated block
*/
void* gf_realloc(void *ptr, size_t size);

#endif
/*! @} */


/*end GPAC memory tracking*/

/*! copy source string to destination, ensuring 0-terminated string result
\param dst  destination buffer
\param src  source buffer
\param dsize size of destination buffer
\return same as strlcpy
*/
size_t gf_strlcpy(char *dst, const char *src, size_t dsize);

#ifdef GPAC_ASSERT_FATAL
/*! fatal error assert, will always kill the program if condition is false
 \param _cond condition to test
*/
#define gf_fatal_assert(_cond) if (! (_cond)) { fprintf(stderr, "Fatal error " #_cond " file %s line %d, exiting\n", (strstr(__FILE__, "gpac") ? strstr(__FILE__, "gpac") + 5 : __FILE__), __LINE__ ); exit(10); }
/*!  error assert, will assert if condition is false but will not always kill the program
 \param _cond condition to test
*/
#define gf_assert(_cond) gf_fatal_assert(_cond)
#elif defined(NDEBUG)
//! @cond Doxygen_Suppress
#define gf_assert(_cond)
#define gf_fatal_assert(_cond) if (! (_cond)) { fprintf(stderr, "Fatal error " #_cond " file %s line %d, exiting\n", (strstr(__FILE__, "gpac") ? strstr(__FILE__, "gpac") + 5 : __FILE__), __LINE__ ); exit(10); }
//! @endcond
#else
//! @cond Doxygen_Suppress
#define gf_assert(_cond) assert(_cond)
#define gf_fatal_assert(_cond) assert(_cond)
//! @endcond
#endif

#if defined(_DEBUG)
#ifdef NDEBUG
#undef NDEBUG
#endif
#endif

#if defined(NDEBUG)
#ifdef GPAC_ENABLE_DEBUG
#undef GPAC_ENABLE_DEBUG
#endif
#else
#ifndef GPAC_ENABLE_DEBUG
/*! Macro for detecting debug configurations*/
#define GPAC_ENABLE_DEBUG
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif	/*_GF_SETUP_H_*/
