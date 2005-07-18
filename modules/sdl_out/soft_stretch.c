/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / SDL audio and video module
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

#include "sdl_out.h"



#define RGB555(r,g,b) (((r&248)<<7) + ((g&248)<<2)  + (b>>3))
#define RGB565(r,g,b) (((r&248)<<8) + ((g&252)<<3)  + (b>>3))

void CopyPrevRow(u8 *src, u8 *dst, u32 dst_w, u32 BPP)
{
	s32 size;
	switch (BPP) {
	case 15:
	case 16:
		size = sizeof(unsigned char)*2;
		break;
	case 24:
		size = sizeof(unsigned char)*3;
		break;
	case 32:
		size = sizeof(unsigned char)*4;
		break;
	default:
		return;
	}
	memcpy(dst, src, dst_w*size);
}

/*copy and stretch a row from src to dst in 8bpp mode (NOT TESTED)*/
void CopyRow_8bpp(u8 *src, u32 src_w, u8 *dst, u32 dst_w)
{
	s32 i;
	s32 pos, inc;
	u8 pixel = 0;

	if (src_w == dst_w) {
		memcpy(dst, src, sizeof(u8)*src_w);
		return;
	}
	pos = 0x10000;
	inc = (src_w << 16) / dst_w;
	for ( i=dst_w; i>0; --i ) {
		while ( pos >= 0x10000L ) {
			pixel = *src++;
			pos -= 0x10000L;
		}
		*dst++ = pixel;
		pos += inc;
	}
}

/*copy and stretch a row from src to dst in 16bpp mode*/
void CopyRow_16bpp(u16 *src, u32 src_w, u16 *dst, u32 dst_w)
{
	s32 i;
	s32 pos, inc;
	u16 pixel = 0;

	if (src_w == dst_w) {
		memcpy(dst, src, sizeof(u16)*src_w);
		return;
	}
	pos = 0x10000;
	inc = (src_w << 16) / dst_w;
	for ( i=dst_w; i>0; --i ) {
		while ( pos >= 0x10000L ) {
			pixel = *src++;
			pos -= 0x10000L;
		}
		*dst++ = pixel;
		pos += inc;
	}
}

/*copy and stretch a row from src to dst in 24bpp mode*/
void CopyRow_24bpp(u8 *src, u32 src_w, u8 *dst, u32 dst_w)
{
	s32 i;
	s32 pos, inc;
	u8 pixel[3];

	if (src_w == dst_w) {
		memcpy(dst, src, sizeof(u8)*3*src_w);
		return;
	}
	pos = 0x10000;
	inc = (src_w << 16) / dst_w;
	for ( i=dst_w; i>0; --i ) {
		while ( pos >= 0x10000L ) {
			pixel[0] = *src++;
			pixel[1] = *src++;
			pixel[2] = *src++;
			pos -= 0x10000L;
		}
		*dst++ = pixel[0];
		*dst++ = pixel[1];
		*dst++ = pixel[2];
		pos += inc;
	}
}

/*copy and stretch a row from src to dst in 32bpp mode*/
void CopyRow_32bpp(u32 *src, u32 src_w, u32 *dst, u32 dst_w)
{
	s32 i;
	s32 pos, inc;
	u32 pixel = 0;

	if (src_w == dst_w) {
		memcpy(dst, src, sizeof(u32)*src_w);
		return;
	}
	pos = 0x10000;
	inc = (src_w << 16) / dst_w;
	for ( i=dst_w; i>0; --i ) {
		while ( pos >= 0x10000L ) {
			pixel = *src++;
			pos -= 0x10000L;
		}
		*dst++ = pixel;
		pos += inc;
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
			a = 0xFF;
			break;
		case 24:
			r = *(src_bits + i*3);
			g = *(src_bits + i*3 + 1);
			b = *(src_bits + i*3 + 2);
			a = 0xFF;
			break;
		case 32:
			r = *(src_bits + i*4);
			g = *(src_bits + i*4 + 1);
			b = *(src_bits + i*4 + 2);
			a = *(src_bits + i*4 + 3);
			break;
		default:
			return;
		}

		switch (dst_bpp) {
		case 15:
			*((u16 *) (dst_bits + i*2)) = RGB555(r, g, b);
			break;
		case 16:
			* ( (u16 *) (dst_bits + i*2)) = RGB565(b, g, r);
			break;
		case 24:
			*(dst_bits + i*3) = r;
			*(dst_bits + i*3 + 1) = g;
			*(dst_bits + i*3 + 2) = b;
			break;
		case 32:
			*(dst_bits + i*4) = r;
			*(dst_bits + i*4 + 1) = g;
			*(dst_bits + i*4 + 2) = b;
			*(dst_bits + i*4 + 3) = a;
			break;
		default:
			return;
		}
	}
}

void StretchBits(void *dst, u32 dst_bpp, u32 dst_w, u32 dst_h, u32 dst_pitch,
				void *src, u32 src_bpp, u32 src_w, u32 src_h, u32 src_pitch,
				Bool FlipIt)
{
	u8 *tmp;
	s32 src_row;
	u32 j;
	s32 pos, inc, prev_row;
	u8 *src_bits = NULL, *dst_bits = NULL, *copyfrom;


	/*we need a local buffer for RGB line conversion*/
	if (dst_bpp != src_bpp) {
		tmp = malloc(sizeof(char) * src_w * dst_bpp/8);
	} else {
		tmp = NULL;
	}

	pos = 0x10000;
	inc = (src_h << 16) / dst_h;
	src_row = 0;
	prev_row = -1;

	/*copy raw by raw*/
	for (j = 0; j<dst_h; j++) {
		if (FlipIt) {
			dst_bits = (u8 *) dst + ((dst_h-j-1) * dst_pitch);
		} else {
			dst_bits = (u8 *) dst + (j * dst_pitch);
		}

		while ( pos >= 0x10000L ) {
			src_bits = (u8 *) src + (src_row * src_pitch);
			src_row++;
			pos -= 0x10000L;
		}
		/*if same row, do a brutal memcpy*/
		if ( prev_row == src_row) {
			CopyPrevRow(((u8 *) dst + ((j - 1) * dst_pitch) ), dst_bits, dst_w, (u8) dst_bpp);
			pos += inc;
			continue;
		}
		/*new row, check if conversion is needed*/
		if (tmp) {
			ConvertRGBLine(src_bits, src_bpp, tmp, dst_bpp, src_w);
			copyfrom = tmp;
		} else {
			copyfrom = src_bits;
		}

		/*finally draw to destination*/
		switch (dst_bpp) {
	    case 8:
			CopyRow_8bpp(copyfrom, src_w, dst_bits, dst_w);
			break;
	    case 15:
	    case 16:
			CopyRow_16bpp((u16 *) copyfrom, src_w, (u16 *) dst_bits, dst_w);
			break;
	    case 24:
			CopyRow_24bpp(copyfrom, src_w, dst_bits, dst_w);
			break;
	    case 32:
			CopyRow_32bpp((u32 *) copyfrom, src_w, (u32 *) dst_bits, dst_w);
			break;
		}
		pos += inc;

		prev_row = src_row;
	}
	if (tmp) free(tmp);
}

