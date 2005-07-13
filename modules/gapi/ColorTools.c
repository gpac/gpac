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


#include "ColorTools.h"

#define RGB555(r,g,b) (((r&248)<<7) + ((g&248)<<2)  + (b>>3))
#define RGB565(r,g,b) (((r&248)<<8) + ((g&252)<<3)  + (b>>3))

void CopyPrevRow(u8 *src, u8 *dst, u32 dst_w, u8 size)
{
	memcpy(dst, src, dst_w*size);
}

typedef void (*myCopyRow)(void *p_src, u32 inc_w, u32 src_w, void *p_dst, u32 dst_w, s32 dst_x_pitch);


/*copy and stretch a row from src to dst in 8bpp mode*/
void CopyRow_8bpp(void *p_src, u32 inc_w, u32 src_w, void *p_dst, u32 dst_w, s32 dst_x_pitch)
{
	s32 i;
	s32 pos;
	u8 pixel = 0;
	u8 *src = (u8 *)p_src;
	u8 *dst = (u8 *)p_dst;

	pos = 0x10000;
	for ( i=dst_w; i>0; --i ) {
		while ( pos >= 0x10000L ) {
			pixel = *src++;
			pos -= 0x10000L;
		}
		*dst = pixel;
		dst += dst_x_pitch;
		pos += inc_w;
	}
}

/*copy and stretch a row from src to dst in 16bpp mode*/
void CopyRow_16bpp(void *p_src, u32 inc_w, u32 src_w, void *p_dst, u32 dst_w, s32 dst_x_pitch)
{
	s32 i;
	s32 pos;
	u16 pixel = 0;
	u16 *src = (u16 *)p_src;
	u16 *dst = (u16 *)p_dst;

	dst_x_pitch /= 2;
	pos = 0x10000;
	for ( i=dst_w; i>0; --i ) {
		while ( pos >= 0x10000L ) {
			pixel = *src++;
			pos -= 0x10000L;
		}
		*dst = pixel;
		dst += dst_x_pitch;
		pos += inc_w;
	}
}

/*copy and stretch a row from src to dst in 24bpp mode*/
void CopyRow_24bpp(void *p_src, u32 inc_w, u32 src_w, void *p_dst, u32 dst_w, s32 dst_x_pitch)
{
	s32 i;
	s32 pos;
	u8 pixel[3];
	u8 *src = (u8 *)p_src;
	u8 *dst = (u8 *)p_dst;
	pos = 0x10000;
	for ( i=dst_w; i>0; --i ) {
		while ( pos >= 0x10000L ) {
			pixel[0] = *src++;
			pixel[1] = *src++;
			pixel[2] = *src++;
			pos -= 0x10000L;
		}
		dst[0] = pixel[0];
		dst[1] = pixel[1];
		dst[2] = pixel[2];
		dst += dst_x_pitch;
		pos += inc_w;
	}
}


/*converts a pixel row from src_bpp to dst_bpp (no stretch)*/
void ConvertRGBLine(u8 *src_bits, u32 src_bpp, 
					u8 *dst_bits, u32 dst_bpp, 
					u32 width)
{
	u32 i;
	u8 r, g, b, a;

	for (i=0; i<width; i++) {
		switch (src_bpp) {
		case 16:
			r = ((u8)(*((u16*)src_bits + i)&31))<<3;
			g = ((u8)((*((u16*)src_bits + i)&2016)>>5))<<2;
			b = ((u8)((*((u16*)src_bits + i)&63488)>>11))<<3;
			a = 0;
			break;
		case 24:
			r = *(src_bits + i*3);
			g = *(src_bits + i*3 + 1);
			b = *(src_bits + i*3 + 2);
			a = 0;
			break;
		default:
			return;
		}

		switch (dst_bpp) {
		case 15:
			*((u16 *) (dst_bits + i*2)) = RGB555(r, g, b);
			break;
		case 16:
			* ( (u16 *) (dst_bits + i*2)) = RGB565(r, g, b);
			break;
		case 24:
			*(dst_bits + i*3) = r;
			*(dst_bits + i*3 + 1) = g;
			*(dst_bits + i*3 + 2) = b;
			break;
		default:
			return;
		}
	}
}

void StretchBits(void *dst, u32 dst_bpp, u32 dst_w, u32 dst_h, s32 dst_x_pitch, s32 dst_y_pitch,
				void *src, u32 src_bpp, u32 src_w, u32 src_h, s32 src_pitch)
{
	u8 *tmp;
	s32 src_row;
	Bool use_memcpy;
	u32 j;
	u8 size;
	s32 pos, inc_h, inc_w, prev_row;
	myCopyRow copy_row = NULL;
	s8 *src_bits = NULL, *dst_bits = NULL, *copyfrom;


	switch (dst_bpp) {
	case 15:
	case 16:
		size = sizeof(unsigned char)*2;
		copy_row = CopyRow_16bpp;
		break;
	case 24:
		size = sizeof(unsigned char)*3;
		copy_row = CopyRow_24bpp;
		break;
	default:
		return;
	}

	/*we need a local buffer for RGB line conversion*/
	if (dst_bpp != src_bpp) {
		tmp = malloc(sizeof(char) * src_w * size);
	} else {
		tmp = NULL;
	}

	use_memcpy = (!tmp && (dst_x_pitch==size) && (src_w == dst_w)) ? 1 : 0;

	if (use_memcpy && (dst_h==src_h)) {
		for (j = 0; j<dst_h; j++) {
			dst_bits = (s8 *) dst + (j * dst_y_pitch);
			src_bits = (u8 *) src + (j * src_pitch);
			if (use_memcpy) memcpy(dst_bits, src_bits, sizeof(u8)*src_w*size);
		}
		return;
	}


	pos = 0x10000;
	inc_h = (src_h << 16) / dst_h;
	inc_w = (src_w << 16) / dst_w;
	src_row = 0;
	prev_row = -1;
	
	/*copy raw by raw*/
	for (j = 0; j<dst_h; j++) {
		dst_bits = (s8 *) dst + (j * dst_y_pitch);

		while ( pos >= 0x10000L ) {
			src_bits = (u8 *) src + (src_row * src_pitch);
			src_row++;
			pos -= 0x10000L;
		}
		if (prev_row != src_row) {
			/*new row, check if conversion is needed*/
			if (tmp) {
				ConvertRGBLine(src_bits, src_bpp, tmp, dst_bpp, src_w);
				copyfrom = tmp;
			} else {
				copyfrom = src_bits;
			}
		}

		/*finally draw to destination*/
		if (use_memcpy) memcpy(dst_bits, copyfrom, sizeof(u8)*src_w*size);
		else copy_row(copyfrom, inc_w, src_w, dst_bits, dst_w, dst_x_pitch);

		pos += inc_h;
		prev_row = src_row;
	}
	if (tmp) free(tmp);
}

