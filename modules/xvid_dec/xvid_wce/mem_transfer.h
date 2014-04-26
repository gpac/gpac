/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - 8<->16 bit buffer transfer header -
 *
 *  Copyright(C) 2001-2003 Peter Ross <pross@xvid.org>
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
 * $Id: mem_transfer.h,v 1.1.1.1 2005-07-13 14:36:16 jeanlf Exp $
 *
 ****************************************************************************/

#ifndef _MEM_TRANSFER_H
#define _MEM_TRANSFER_H

#include "Rules.h"

/*****************************************************************************
 * transfer16to8 API
 ****************************************************************************/
void transfer_16to8copy(byte *dst, const int *src, dword stride);

/*****************************************************************************
 * transfer16to8 + addition op API
 ****************************************************************************/
void transfer_16to8add(byte *dst, const int *src, dword stride);

/*****************************************************************************
 * transfer8to8 + no op
 ****************************************************************************/
#ifdef _ARM_
extern"C"
#endif
void transfer8x8_copy(byte *const dst, const byte * const src, const dword stride);

//----------------------------

inline void transfer16x16_copy(byte * const dst, const byte * const src, const dword stride) {

	transfer8x8_copy(dst, src, stride);
	transfer8x8_copy(dst + 8, src + 8, stride);
	transfer8x8_copy(dst + 8*stride, src + 8*stride, stride);
	transfer8x8_copy(dst + 8*stride + 8, src + 8*stride + 8, stride);
}

//----------------------------

#endif
