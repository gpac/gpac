/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / GAPI WinCE video render module
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


#ifndef __COLORCONV_H_
#define __COLORCONV_H_


#include <gpac/constants.h>

#ifdef __cplusplus
extern "C" {
#endif


/*stretch bits*/
void StretchBits(void *dst, u32 dst_bpp, u32 dst_w, u32 dst_h, s32 dst_x_pitch, s32 dst_y_pitch,
				void *src, u32 src_bpp, u32 src_w, u32 src_h, s32 src_pitch);

/*stretch and rotate (PI/2) bits*/
void RotateBits(void *dst, u32 dst_bpp, u32 dst_w, u32 dst_h, s32 dst_x_pitch, s32 dst_y_pitch,
				void *src, u32 src_bpp, u32 src_w, u32 src_h, s32 src_pitch);
#ifdef __cplusplus
}
#endif

#endif