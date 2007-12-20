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

#include <gpac/utf.h>

 
 /*
 * Original code from the GNU UTF-8 Library
 */

GF_EXPORT
size_t gf_utf8_wcstombs(char* dest, size_t len, const unsigned short** srcp)
{
	size_t count;
	const unsigned short * src = *srcp;

	if (dest != NULL) {
		char* destptr = dest;
        for (;; src++) {
			unsigned char c;
			unsigned short wc = *src;
			if (wc < 0x80) {
				if (wc == (wchar_t)'\0') {
					if (len == 0) {
						*srcp = src;
						break;
					}
					*destptr = '\0';
					*srcp = NULL;
					break;
				}
				count = 0;
				c = (unsigned char) wc;
			} else if (wc < 0x800) {
				count = 1;
				c = (unsigned char) ((wc >> 6) | 0xC0);
			} else {
				count = 2;
				c = (unsigned char) ((wc >> 12) | 0xE0);
			}
			if (len <= count) {
				*srcp = src;
				break;
			}
			len -= count+1;
			*destptr++ = c;
			if (count > 0)
				do {
					*destptr++ = (unsigned char)(((wc >> (6 * --count)) & 0x3F) | 0x80);
				} while (count > 0);
        }
        return destptr - dest;
	} else {
        /* Ignore dest and len. */
        size_t totalcount = 0;
        for (;; src++) {
			unsigned short wc = *src;
			size_t count;
			if (wc < 0x80) {
				if (wc == (wchar_t)'\0') {
					*srcp = NULL;
					break;
				}
				count = 1;
			} else if (wc < 0x800) {
				count = 2;
			} else {
				count = 3;
			}
			totalcount += count;
        }
        return totalcount;
	}
}


typedef struct
{
	u32 count : 16;   /* number of bytes remaining to be processed */
	u32 value : 16;   /* if count > 0: partial wide character */
/* 
   If WCHAR_T_BITS == 16, need 2 bits for count,
   12 bits for value (10 for mbstowcs direction, 12 for wcstombs direction). 
*/
} gf_utf8_mbstate_t;

static gf_utf8_mbstate_t internal;

GF_EXPORT
size_t gf_utf8_mbstowcs(unsigned short* dest, size_t len, const char** srcp)
{
	gf_utf8_mbstate_t* ps = &internal;
	const char *src = *srcp;

    unsigned short* destptr = dest;
    for (; len > 0; destptr++, len--) {
		const char* backup_src = src;
		unsigned char c;
		unsigned short wc;
		size_t count;
		if (ps->count == 0) {
			c = (unsigned char) *src;
			if (c < 0x80) {
				*destptr = (wchar_t) c;
				if (c == 0) {
					src = NULL;
					break;
				}
				src++;
				continue;
			} else if (c < 0xC0) {
				/* Spurious 10XXXXXX byte is invalid. */
				goto bad_input;
			}
			if (c < 0xE0) {
				wc = (wchar_t)(c & 0x1F) << 6;
				count = 1;
				if (c < 0xC2) goto bad_input;
			} else if (c < 0xF0) {
				wc = (wchar_t)(c & 0x0F) << 12;
				count = 2;
			}
			else goto bad_input;
			src++;
		} else {
			wc = ps->value << 6;
			count = ps->count;
		}
		for (;;) {
			c = (unsigned char) *src++ ^ 0x80;
			if (!(c < 0x40)) goto bad_input_backup;
			wc |= (unsigned short) c << (6 * --count);
			if (count == 0)
				break;
			/* The following test is only necessary once for every character,
			but it would be too complicated to perform it once only, on
			the first pass through this loop. */
			if ((unsigned short) wc < ((unsigned short) 1 << (5 * count + 6)))
				goto bad_input_backup;
		}
		*destptr = wc;
		ps->count = 0;
		continue;

bad_input_backup:
		src = backup_src;
		goto bad_input;
    }
    *srcp = src;
    return destptr-dest;

bad_input:
	*srcp = src;
	return (size_t)(-1);
}


GF_EXPORT
size_t gf_utf8_wcslen (const unsigned short *s)
{
  const unsigned short* ptr;
  for (ptr = s; *ptr != (unsigned short)'\0'; ptr++) {
  }
  return ptr - s;
}


