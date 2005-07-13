/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005 
 *					All rights reserved
 *
 *  This file is part of GPAC / common tools sub-project
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

#ifndef _GF_UTF_H_
#define _GF_UTF_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/tools.h>

/*UTF-8 basic routines*/

/*converts wide-char string to multibyte string - returns (-1) if error. set @srcp to next char to be
converted if not enough space*/
size_t gf_utf8_wcstombs(char* dest, size_t len, const unsigned short** srcp);
/*converts UTF8 string to wide char string - returns (-1) if error. set @srcp to next char to be
converted if not enough space*/
size_t gf_utf8_mbstowcs(unsigned short* dest, size_t len, const char** srcp);
/*returns size in characters of the wide-char string*/
size_t gf_utf8_wcslen(const unsigned short *s);


#ifdef __cplusplus
}
#endif


#endif		/*_GF_UTF_H_*/

