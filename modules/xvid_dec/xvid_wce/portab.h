/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - Portable macros, types and inlined assembly -
 *
 *  Copyright(C) 2002      Michael Militzer <isibaar@xvid.org>
 *               2002-2003 Peter Ross <pross@xvid.org>
 *               2002-2003 Edouard Gomez <ed.gomez@free.fr>
 *
 *  This program is free software ; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation ; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY ; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program ; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 * $Id: portab.h,v 1.1.1.1 2005-07-13 14:36:16 jeanlf Exp $
 *
 ****************************************************************************/

#ifndef _PORTAB_H_
#define _PORTAB_H_

#include "Rules.h"

#define ARCH_IS_LITTLE_ENDIAN
#define ARCH_IS_GENERIC


/*****************************************************************************
 *  Common things
 ****************************************************************************/

/* Buffer size for msvc implementation because it outputs to DebugOutput */
#ifdef _DEBUG
extern dword xvid_debug;
#define DPRINTF_BUF_SZ  1024
#endif

/*****************************************************************************
 *  Types used in XviD sources
 ****************************************************************************/

/*----------------------------------------------------------------------------
 | For MSVC
 *---------------------------------------------------------------------------*/

//#if defined(_MSC_VER) || defined (__WATCOMC__)
#define int64_t  int

/*----------------------------------------------------------------------------
 | For all other compilers, use the standard header file
 | (compiler should be ISO C99 compatible, perhaps ISO C89 is enough)
 *---------------------------------------------------------------------------*/

//#endif

/*****************************************************************************
 *  Some things that are only architecture dependant
 ****************************************************************************/

#define CACHE_LINE 32

/*****************************************************************************
 *  MSVC compiler specific macros, functions
 ****************************************************************************/

#ifdef _MSC_VER

/*----------------------------------------------------------------------------
 | Common msvc stuff
 *---------------------------------------------------------------------------*/

/*
 * This function must be declared/defined all the time because MSVC does
 * not support C99 variable arguments macros.
 *
 * Btw, if the MS compiler does its job well, it should remove the nop
 * DPRINTF function when not compiling in _DEBUG mode
 */
#if defined _DEBUG && defined _WINDOWS

void DPRINTF(int level, char *fmt, int p=0);

#else

inline void DPRINTF(int level, char *fmt, int p=0) {}

#endif


/*****************************************************************************
 *  GNU CC compiler stuff
 ****************************************************************************/

#elif defined __GNUC__ || defined __ICC /* Compiler test */

/*----------------------------------------------------------------------------
 | Common gcc stuff
 *---------------------------------------------------------------------------*/

/*
 * As gcc is (mostly) C99 compliant, we define DPRINTF only if it's realy needed
 * and it's a macro calling fprintf directly
 */
#    ifdef _DEBUG

/* Needed for all debuf fprintf calls */
#       include <stdio.h>
#       include <stdarg.h>

inline void DPRINTF(int level, char *format, int p=0) {
	va_list args;
	va_start(args, format);
	if(xvid_debug & level) {
		vfprintf(stderr, format, args);
	}
}

# else /* _DEBUG */
inline void DPRINTF(int level, char *format, int p=0) {}
# endif /* _DEBUG */

#else                         //Compiler test

/*
 * Ok we know nothing about the compiler, so we fallback to ANSI C
 * features, so every compiler should be happy and compile the code.
 *
 * This is (mostly) equivalent to ARCH_IS_GENERIC.
 */

#ifdef _DEBUG
/* Needed for all debuf fprintf calls */
# include <stdio.h>
# include <stdarg.h>

static __inline void DPRINTF(int level, char *format, int p=0) {
	va_list args;
	va_start(args, format);
	if(xvid_debug & level) {
		vfprintf(stderr, format, args);
	}
}

# else //_DEBUG
inline void DPRINTF(int level, char *format, int p=0) {}
#endif //_DEBUG

#define ByteSwap(a) \
   ((a) = ((a) << 24)  | (((a) & 0xff00) << 8) | (((a) >> 8) & 0xff00) | (((a) >> 24) & 0xff))

#define DECLARE_ALIGNED_MATRIX(name, sizex, sizey, type, alignment) \
   type name[(sizex)*(sizey)]

#endif //Compiler test


#endif /* PORTAB_H */
