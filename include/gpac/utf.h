/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2022
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

/*!
\file <gpac/utf.h>
\brief UTF functions.
 */

/*!
\addtogroup utf_grp
\brief UTF and Unicode-related functions

This section documents the UTF functions of the GPAC framework.\n
The wide characters in GPAC are unsignad shorts, in other words GPAC only supports UTF8 and UTF16 coding styles.

\note these functions are just ports of libutf8 library tools into GPAC.

@{
 */

#include <gpac/tools.h>

/*! error code for UTF-8 conversion errors*/
#define GF_UTF8_FAIL	0xFFFFFFFF
/*!
\brief wide-char to multibyte conversion

Converts a wide-char string to a multibyte string
\param dst multibyte destination buffer
\param dst_len multibyte destination buffer size
\param srcp address of the wide-char string. This will be set to the next char to be converted in the input buffer if not enough space in the destination, or NULL if conversion was completed.
\return length (in byte) of the multibyte string or GF_UTF8_FAIL if error.
 */
u32 gf_utf8_wcstombs(char* dst, size_t dst_len, const unsigned short** srcp);

/*!
\brief multibyte to wide-char conversion

Converts a multibyte string to a wide-char string
\param dst wide-char destination buffer
\param dst_len wide-char destination buffer size
\param srcp address of the multibyte character buffer. This will be set to the next char to be converted in the input buffer if not enough space in the destination, or NULL if conversion was completed.
\return length (in unsigned short) of the wide-char string or GF_UTF8_FAIL if error.
 */
u32 gf_utf8_mbstowcs(unsigned short* dst, size_t dst_len, const char** srcp);

/*!
\brief wide-char string length

Gets the length in character of a wide-char string
\param s the wide-char string
\return the wide-char string length
 */
u32 gf_utf8_wcslen(const unsigned short *s);

/*!
\brief returns a string from a string started with BOM

Returns string from data, potentially converting utf16 to utf8
\param data the string or wide-char string
\param size of the data buffer
  size of the data buffer
\param out_ptr set to an allocated buffer if needed for conversion, shall be destroyed by caller. Must not be NULL
\param result set to resulting string. Must not be NULL
\param res_size set to length of resulting string. May be NULL
\return error if any: GF_IO_ERR if UTF decode error or GF_BAD_PARAM
 */
GF_Err gf_utf_get_string_from_bom(const u8 *data, u32 size, char **out_ptr, char **result, u32 *res_size);

/*!
\brief Checks validity of a UTF8 string

Checks if a given byte sequence is a valid UTF-8 encoding
\param data the byte equence buffer
\param size the length of the byte sequence
\return GF_TRUE if valid UTF8, GF_FALSE otherwise
 */
Bool gf_utf8_is_legal(const u8 *data, u32 size);


/*!
\brief string bidi reordering

Performs a simple reordering of words in the string based on each word direction, so that glyphs are sorted in display order.
\param utf_string the wide-char string
\param len the len of the wide-char string
\return 1 if the main direction is right-to-left, 0 otherwise
 */
Bool gf_utf8_reorder_bidi(u16 *utf_string, u32 len);

/*! maximum character size in bytes*/
static const u32 UTF8_MAX_BYTES_PER_CHAR = 4;


/*!
\brief Unicode conversion from UTF-8 to UCS-4
\param ucs4_buf The UCS-4 buffer to fill
\param utf8_len The length of the UTF-8 buffer
\param utf8_buf The buffer containing the UTF-8 data
\return the length of the ucs4_buf. Note that the ucs4_buf should be allocated by parent and should be at least utf8_len * 4
 */
u32 utf8_to_ucs4 (u32 *ucs4_buf, u32 utf8_len, unsigned char *utf8_buf);




#if defined(WIN32)

wchar_t* gf_utf8_to_wcs(const char* str);
char* gf_wcs_to_utf8(const wchar_t* str);

#endif

/*! @} */

#ifdef __cplusplus
}
#endif


#endif		/*_GF_UTF_H_*/

