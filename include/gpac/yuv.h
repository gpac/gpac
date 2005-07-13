/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / common tools interfaces
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


#ifndef _GF_RASTER_2D_H_
#define _GF_RASTER_2D_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/tools.h>


	/*YUV->RGB routines*/
void gf_yuv_to_rgb_555(unsigned char *dst, s32 dst_stride,
				 unsigned char *y_src, unsigned char * u_src, unsigned char * v_src,
				 s32 y_stride, s32 uv_stride, s32 width, s32 height);
void gf_yuv_to_rgb_565(unsigned char * dst, s32 dst_stride,
				 unsigned char* y_src, unsigned char* u_src, unsigned char* v_src,
				 s32 y_stride, s32 uv_stride, s32 width, s32 height);
void gf_yuv_to_bgr_24(unsigned char *dst, s32 dststride, 
				unsigned char *y_src, unsigned char *u_src, unsigned char * v_src, 
				s32 y_stride, s32 uv_stride, s32 width, s32 height);
void gf_yuv_to_rgb_24(unsigned char *dst, s32 dststride, unsigned char *y_src, unsigned char *u_src, unsigned char * v_src, 
				 s32 y_stride, s32 uv_stride, s32 width, s32 height);
void gf_yuv_to_rgb_32(unsigned char *dst, s32 dststride, 
				unsigned char *y_src, unsigned char *v_src, unsigned char * u_src,
				s32 y_stride, s32 uv_stride, s32 width, s32 height);
void gf_yuva_to_rgb_32(unsigned char *dst, s32 dststride, 
				 unsigned char *y_src, unsigned char *v_src, unsigned char * u_src, unsigned char *a_src,
				 s32 y_stride, s32 uv_stride, s32 width, s32 height);

/*for openGL texturing*/
void gf_yuv_to_rgb_24_flip(unsigned char *dst, s32 dststride, unsigned char *y_src, unsigned char *u_src, unsigned char * v_src, 
				 s32 y_stride, s32 uv_stride, s32 width, s32 height);


#ifdef __cplusplus
}
#endif


#endif	/*_GF_RASTER_2D_H_*/

