/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre 
 *			Copyright (c) Telecom ParisTech 2000-2012
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

#ifndef _GF_TOKEN_H_
#define _GF_TOKEN_H_

#ifdef __cplusplus
extern "C" {
#endif

/*!
 *	\file <gpac/token.h>
 *	\brief tokenizer functions.
 */

 /*!
 *	\addtogroup tok_grp tokenizer
 *	\ingroup utils_grp
 *	\brief String Tokenizer Functions
 *
 *This section documents the basic string tokenizer of the GPAC framework.
 *	@{
 */

#include <gpac/tools.h>

/*!
 *\brief get string component 
 *
 *Gets the next string component comprised in a given set of characters
 *\param Buffer source string to scan
 *\param Start char offset from begining of buffer where tokenization shall start
 *\param Separator separator characters to use
 *\param Container output buffer location
 *\param ContainerSize output buffer allocated size
 *\return position of the first char in the buffer after the last terminating separator, or -1 if token could not be found
 */
s32 gf_token_get(const char* Buffer, s32 Start, const char* Separator, char* Container, s32 ContainerSize);
/*!
 *\brief get string component without delimitting characters
 *
 *Gets the next string component comprised in a given set of characters, removing surrounding characters
 *\param Buffer source string to scan
 *\param Start char offset from begining of buffer where tokenization shall start
 *\param Separator separator characters to use
 *\param strip_set surrounding characters to remove
 *\param Container output buffer location
 *\param ContainerSize output buffer allocated size
 *\return position of the first char in the buffer after the last terminating separator, or -1 if token could not be found
 */
s32 gf_token_get_strip(const char* Buffer, s32 Start, const char* Separator, const char* strip_set, char* Container, s32 ContainerSize);
/*!
 *\brief line removal
 *
 *Gets one line from buffer and remove delimiters CR, LF and CRLF
 *\param buffer source string to scan
 *\param start char offset from begining of buffer where tokenization shall start
 *\param size size of the input buffer to analyze
 *\param line_buffer output buffer location
 *\param line_buffer_size output buffer allocated size
 *\return position of the first char in the buffer after the last line delimiter, or -1 if no line could be found
 */
s32 gf_token_get_line(const char *buffer, u32 start, u32 size, char *line_buffer, u32 line_buffer_size);
/*!
 *\brief pattern location
 *
 *Locates a pattern in the buffer
 *\param Buffer source string to scan
 *\param Start char offset from begining of buffer where tokenization shall start
 *\param Size size of the input buffer to analyze
 *\param Pattern pattern to locate
 *\return position of the first char in the buffer after the pattern, or -1 if pattern could not be found
 */
s32 gf_token_find(const char* Buffer, u32 Start, u32 Size, const char* Pattern);


/*! @} */

#ifdef __cplusplus
}
#endif


#endif		/*_GF_TOKEN_H_*/

