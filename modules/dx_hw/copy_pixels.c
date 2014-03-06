/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre 
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / 2D rendering module
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

#include "dx_hw.h"


static u32 get_yuv_base(u32 in_pf) 
{
	switch (in_pf) {
	case GF_PIXEL_I420:
	case GF_PIXEL_IYUV:
	case GF_PIXEL_YV12:
		return GF_PIXEL_YV12;
	case GF_PIXEL_Y422:
	case GF_PIXEL_UYNV:
	case GF_PIXEL_UYVY:
		return GF_PIXEL_UYVY;
	case GF_PIXEL_YUNV:
	case GF_PIXEL_V422:
	case GF_PIXEL_YUY2:
		return GF_PIXEL_YUY2;
	case GF_PIXEL_YVYU:
		return GF_PIXEL_YVYU;
	case GF_PIXEL_YV12_10:
		return GF_PIXEL_YV12_10;
	default:
		return 0;
	}
}

static Bool format_is_yuv(u32 in_pf)
{
	switch (in_pf) {
	case GF_PIXEL_YUY2:
	case GF_PIXEL_YVYU:
	case GF_PIXEL_UYVY:
	case GF_PIXEL_VYUY:
	case GF_PIXEL_Y422:
	case GF_PIXEL_UYNV:
	case GF_PIXEL_YUNV:
	case GF_PIXEL_V422:
	case GF_PIXEL_YV12:
	case GF_PIXEL_IYUV:
	case GF_PIXEL_I420:
		return 1;
	/*not supported yet*/
	case GF_PIXEL_YUVA:
	default:
		return 0;
	}
}

static Bool is_planar_yuv(u32 pf)
{
	switch (pf) {
	case GF_PIXEL_YV12:
	case GF_PIXEL_I420:
	case GF_PIXEL_IYUV:
		return 1;
	}
	return 0;
}


static void write_yv12_to_yuv(GF_VideoSurface *vs,  unsigned char *pY, u32 src_stride, u32 src_pf,
								 u32 src_width, u32 src_height, const GF_Window *src_wnd, u8 *pU, u8 *pV)
{

	if (!pU) {
		pU = pY + src_stride * src_height;
		pV = pY + 5*src_stride * src_height/4;
	}


	pY = pY + src_stride * src_wnd->y + src_wnd->x;
	/*because of U and V downsampling by 2x2, working with odd Y offset will lead to a half-line shift between Y and UV components. We
	therefore force an even Y offset for U and V planes.*/
	pU = pU + (src_stride * (src_wnd->y / 2) + src_wnd->x) / 2;
	pV = pV + (src_stride * (src_wnd->y / 2) + src_wnd->x) / 2;


	if (is_planar_yuv(vs->pixel_format)) {
		/*complete source copy*/
		if ( (vs->pitch_y == (s32) src_stride) && (src_wnd->w == src_width) && (src_wnd->h == src_height)) {
			assert(!src_wnd->x);
			assert(!src_wnd->y);
			memcpy(vs->video_buffer, pY, sizeof(unsigned char)*src_width*src_height);
			if (vs->pixel_format == GF_PIXEL_YV12) {
				memcpy(vs->video_buffer + vs->pitch_y * vs->height, pV, sizeof(unsigned char)*src_width*src_height/4);
				memcpy(vs->video_buffer + 5 * vs->pitch_y * vs->height/4, pU, sizeof(unsigned char)*src_width*src_height/4);
			} else {
				memcpy(vs->video_buffer + vs->pitch_y * vs->height, pU, sizeof(unsigned char)*src_width*src_height/4);
				memcpy(vs->video_buffer + 5 * vs->pitch_y * vs->height/4, pV, sizeof(unsigned char)*src_width*src_height/4);
			}
		} else {
			u32 i;
			unsigned char *dst, *src, *dst2, *src2, *dst3, *src3;

			src = pY;
			dst = vs->video_buffer;
			
			src2 = (vs->pixel_format != GF_PIXEL_YV12) ? pU : pV;
			dst2 = vs->video_buffer + vs->pitch_y * vs->height;
			src3 = (vs->pixel_format != GF_PIXEL_YV12) ? pV : pU;
			dst3 = vs->video_buffer + 5*vs->pitch_y * vs->height/4;
			for (i=0; i<src_wnd->h; i++) {
				memcpy(dst, src, src_wnd->w);
				src += src_stride;
				dst += vs->pitch_y;
				if (i<src_wnd->h/2) {
					memcpy(dst2, src2, src_wnd->w/2);
					src2 += src_stride/2;
					dst2 += vs->pitch_y/2;
					memcpy(dst3, src3, src_wnd->w/2);
					src3 += src_stride/2;
					dst3 += vs->pitch_y/2;
				}
			}
		}
	} else if (get_yuv_base(vs->pixel_format)==GF_PIXEL_UYVY) {
		u32 i, j;
		unsigned char *dst, *y, *u, *v;
		for (i=0; i<src_wnd->h; i++) {
			y = pY + i*src_stride;
			u = pU + (i/2) * src_stride/2;
			v = pV + (i/2) * src_stride/2;
			dst = vs->video_buffer + i*vs->pitch_y;

			for (j=0; j<src_wnd->w/2;j++) {
				*dst = *u;
				dst++;
				u++;
				*dst = *y;
				dst++;
				y++;
				*dst = *v;
				dst++;
				v++;
				*dst = *y;
				dst++;
				y++;
			}
		}
	} else if (get_yuv_base(vs->pixel_format)==GF_PIXEL_YUY2) {
		u32 i, j;
		unsigned char *dst, *y, *u, *v;
		for (i=0; i<src_wnd->h; i++) {
			y = pY + i*src_stride;
			u = pU + (i/2) * src_stride/2;
			v = pV + (i/2) * src_stride/2;
			dst = vs->video_buffer + i*vs->pitch_y;

			for (j=0; j<src_wnd->w/2;j++) {
				*dst = *y;
				dst++;
				y++;
				*dst = *u;
				dst++;
				u++;
				*dst = *y;
				dst++;
				y++;
				*dst = *v;
				dst++;
				v++;
			}
		}
	} else if (vs->pixel_format==GF_PIXEL_YVYU) {
		u32 i, j;
		unsigned char *dst, *y, *u, *v;
		for (i=0; i<src_wnd->h; i++) {
			y = pY + i*src_stride;
			u = pU + (i/2) * src_stride/2;
			v = pV + (i/2) * src_stride/2;
			dst = vs->video_buffer + i*vs->pitch_y;

			for (j=0; j<src_wnd->w/2;j++) {
				*dst = *y;
				dst++;
				y++;
				*dst = *v;
				dst++;
				v++;
				*dst = *y;
				dst++;
				y++;
				*dst = *u;
				dst++;
				u++;
			}
		}
	}
}

static void write_yvyu_to_yuv(GF_VideoSurface *vs,  unsigned char *src, u32 src_stride, u32 src_pf,
								 u32 src_width, u32 src_height, const GF_Window *src_wnd)
{
	u32 i, j, base_pf;
	unsigned char *pY, *pU, *pV;

	switch (get_yuv_base(src_pf) ) {
	case GF_PIXEL_UYVY:
		pU = src + src_stride * src_wnd->y + src_wnd->x;
		pY = src + src_stride * src_wnd->y + src_wnd->x + 1;
		pV = src + src_stride * src_wnd->y + src_wnd->x + 3;
		break;
	case GF_PIXEL_YUY2:
		pY = src + src_stride * src_wnd->y + src_wnd->x;
		pU = src + src_stride * src_wnd->y + src_wnd->x + 1;
		pV = src + src_stride * src_wnd->y + src_wnd->x + 3;
		break;
	case GF_PIXEL_YVYU:
		pY = src + src_stride * src_wnd->y + src_wnd->x;
		pV = src + src_stride * src_wnd->y + src_wnd->x + 1;
		pU = src + src_stride * src_wnd->y + src_wnd->x + 3;
		break;
	default:
		return;
	}

	if (is_planar_yuv(vs->pixel_format)) {
		u32 i, j;
		unsigned char *dst_y, *dst_u, *dst_v;

		dst_y = vs->video_buffer;
		if (vs->pixel_format == GF_PIXEL_YV12) {
			dst_v = vs->video_buffer + vs->pitch_y * vs->height;
			dst_u = vs->video_buffer + 5*vs->pitch_y * vs->height/4;
		} else {
			dst_u = vs->video_buffer + vs->pitch_y * vs->height;
			dst_v = vs->video_buffer + 5*vs->pitch_y * vs->height/4;
		}
		for (i=0; i<src_wnd->h; i++) {
			for (j=0; j<src_wnd->w; j+=2) {
				*dst_y = * pY;
				*(dst_y+1) = * (pY+2);
				dst_y += 2;
				pY += 4;
				if (i%2) continue;

				*dst_u = (*pU + *(pU + src_stride)) / 2;
				*dst_v = (*pV + *(pV + src_stride)) / 2;
				dst_u++;
				dst_v++;
				pU += 4;
				pV += 4;
			}
			if (i%2) {
				pU += src_stride;
				pV += src_stride;
			}
		}
		return;
	}
	
	if (get_yuv_base(src_pf) == get_yuv_base(vs->pixel_format)) {
		u32 i;
		for (i=0; i<src_wnd->h; i++) {
			char *dst = vs->video_buffer + i*vs->pitch_y;
			pY = src + src_stride * (i+src_wnd->y) + src_wnd->x;
			memcpy(dst, pY, sizeof(char)*2*src_wnd->w);
		}
		return;
	}

	base_pf = get_yuv_base(vs->pixel_format);
	for (i=0; i<src_wnd->h; i++) {
		char *dst = vs->video_buffer + i*vs->pitch_y;
		char *y = pY + src_stride * i;
		char *u = pU + src_stride * i;
		char *v = pV + src_stride * i;
		switch (base_pf) {
		case GF_PIXEL_UYVY:
			for (j=0; j<src_wnd->w; j+=2) {
				dst[0] = *u;
				dst[1] = *y;
				dst[2] = *v;
				dst[3] = *(y+2);
				dst += 4;
				y+=4;
				u+=4;
				v+=4;
			}
			break;
		case GF_PIXEL_YVYU:
			for (j=0; j<src_wnd->w; j+=2) {
				dst[0] = *y;
				dst[1] = *v;
				dst[2] = *(y+2);
				dst[3] = *u;
				dst += 4;
				y+=4;
				u+=4;
				v+=4;
			}
			break;
		case GF_PIXEL_YUY2:
			for (j=0; j<src_wnd->w; j+=2) {
				dst[0] = *y;
				dst[1] = *u;
				dst[2] = *(y+2);
				dst[3] = *v;
				dst += 4;
				y+=4;
				u+=4;
				v+=4;
			}
			break;
		}
	}
}

u32 get_bpp(u32 pf)
{
	switch (pf) {
	case GF_PIXEL_RGB_555:
	case GF_PIXEL_RGB_565:
		return 2;
	case GF_PIXEL_RGB_24:
	case GF_PIXEL_RGBS:
	case GF_PIXEL_BGR_24:
		return 3;
	case GF_PIXEL_RGB_32:
	case GF_PIXEL_BGR_32:
	case GF_PIXEL_ARGB:
	case GF_PIXEL_RGBAS:
	case GF_PIXEL_RGBD:
	case GF_PIXEL_RGBDS:
		return 4;
	}
	return 0;
}

void rgb_to_24(GF_VideoSurface *vs, unsigned char *src, u32 src_stride, u32 src_w, u32 src_h, u32 src_pf, const GF_Window *src_wnd)
{
	u32 i;
	u32 BPP = get_bpp(src_pf);
	if (!BPP) return;

	/*go to start of src*/
	src += src_stride*src_wnd->y + BPP * src_wnd->x;

	if (src_pf==vs->pixel_format) {
		for (i=0; i<src_wnd->h; i++) {
			memcpy(vs->video_buffer + i*vs->pitch_y, src, sizeof(unsigned char) * BPP * src_wnd->w);
			src += src_stride;
		}
		return;
	}
}


void rgb_to_555(GF_VideoSurface *vs, unsigned char *src, u32 src_stride, u32 src_w, u32 src_h, u32 src_pf, const GF_Window *src_wnd)
{
	u32 i, j, r, g, b;
	u32 BPP = get_bpp(src_pf);
	unsigned char *dst, *cur;
	if (!BPP) return;

	/*go to start of src*/
	src += src_stride*src_wnd->y + BPP * src_wnd->x;

	if (src_pf==vs->pixel_format) {
		for (i=0; i<src_wnd->h; i++) {
			memcpy(vs->video_buffer + i*vs->pitch_y, src, sizeof(unsigned char) * BPP * src_wnd->w);
		}
		return;
	}
	switch (src_pf) {
	case GF_PIXEL_RGB_24:
	case GF_PIXEL_RGBS:
		/*nope get all pixels*/
		for (i=0; i<src_wnd->h; i++) {
			dst = vs->video_buffer + i*vs->pitch_y;
			cur = src + i*src_stride;
			for (j=0; j<src_wnd->w; j++) {
				r = *cur++;
				g = *cur++;
				b = *cur++;
				* ((unsigned short *)dst) = GF_COL_555(r, g, b);
				dst += 2;
			}
		}
		break;
	}	
}

void rgb_to_565(GF_VideoSurface *vs, unsigned char *src, u32 src_stride, u32 src_w, u32 src_h, u32 src_pf, const GF_Window *src_wnd)
{
	u32 i, j, r, g, b;
	u32 BPP = get_bpp(src_pf);
	unsigned char *dst, *cur;
	if (!BPP) return;

	/*go to start of src*/
	src += src_stride*src_wnd->y + BPP * src_wnd->x;

	if (src_pf==vs->pixel_format) {
		for (i=0; i<src_wnd->h; i++) {
			memcpy(vs->video_buffer + i*vs->pitch_y, src, sizeof(unsigned char) * BPP * src_wnd->w);
		}
		return;
	}
	/*nope get all pixels*/
	switch (src_pf) {
	case GF_PIXEL_RGB_24:
	case GF_PIXEL_RGBS:
		for (i=0; i<src_wnd->h; i++) {
			dst = vs->video_buffer + i*vs->pitch_y;
			cur = src + i*src_stride;
			for (j=0; j<src_wnd->w; j++) {
				r = *cur++;
				g = *cur++;
				b = *cur++;
				* ((unsigned short *)dst) = GF_COL_565(r, g, b);
				dst += 2;
			}
		}
		break;
	}
}

void rgb_to_32(GF_VideoSurface *vs, unsigned char *src, u32 src_stride, u32 src_w, u32 src_h, u32 src_pf, const GF_Window *src_wnd)
{
	u32 i, j;
	Bool isBGR;
	u32 BPP = get_bpp(src_pf);
	unsigned char *dst, *cur;
	if (!BPP) return;

	/*go to start of src*/
	src += src_stride*src_wnd->y + BPP * src_wnd->x;

	if (src_pf==vs->pixel_format) {
		for (i=0; i<src_wnd->h; i++) {
			memcpy(vs->video_buffer + i*vs->pitch_y, src, sizeof(unsigned char) * BPP * src_wnd->w);
		}
		return;
	}
	/*get all pixels*/
	isBGR = vs->pixel_format==GF_PIXEL_BGR_32;
	if (isBGR) {
		switch (src_pf) {
		case GF_PIXEL_RGB_24:
		case GF_PIXEL_RGBS:
			for (i=0; i<src_wnd->h; i++) {
				dst = vs->video_buffer + i*vs->pitch_y;
				cur = src + i*src_stride;
				for (j=0; j<src_wnd->w; j++) {
					dst[0] = *cur++;
					dst[1] = *cur++;
					dst[2] = *cur++;
					dst += 4;
				}
			}
			break;
		case GF_PIXEL_RGBDS:
		case GF_PIXEL_RGBD:
			for (i=0; i<src_wnd->h; i++) {
				dst = vs->video_buffer + i*vs->pitch_y;
				cur = src + i*src_stride;
				for (j=0; j<src_wnd->w; j++) {
					dst[0] = *cur++;
					dst[1] = *cur++;
					dst[2] = *cur++;
					cur++;
					dst += 4;
				}
			}
			break;
		case GF_PIXEL_BGR_24:
			for (i=0; i<src_wnd->h; i++) {
				dst = vs->video_buffer + i*vs->pitch_y;
				cur = src + i*src_stride;
				for (j=0; j<src_wnd->w; j++) {
					dst[2] = *cur++;
					dst[1] = *cur++;
					dst[0] = *cur++;
					dst += 4;
				}
			}
			break;
		}
	} else {
		switch (src_pf) {
		case GF_PIXEL_RGB_24:
		case GF_PIXEL_RGBS:
			for (i=0; i<src_wnd->h; i++) {
				dst = vs->video_buffer + i*vs->pitch_y;
				cur = src + i*src_stride;
				for (j=0; j<src_wnd->w; j++) {
					dst[2] = *cur++;
					dst[1] = *cur++;
					dst[0] = *cur++;
					dst += 4;
				}
			}
			break;
		case GF_PIXEL_RGBD:
		case GF_PIXEL_RGBDS:
			for (i=0; i<src_wnd->h; i++) {
				dst = vs->video_buffer + i*vs->pitch_y;
				cur = src + i*src_stride;
				for (j=0; j<src_wnd->w; j++) {
					dst[2] = *cur++;
					dst[1] = *cur++;
					dst[0] = *cur++;
					cur++;
					dst += 4;
				}
			}
			break;
		case GF_PIXEL_BGR_24:
			for (i=0; i<src_wnd->h; i++) {
				dst = vs->video_buffer + i*vs->pitch_y;
				cur = src + i*src_stride;
				for (j=0; j<src_wnd->w; j++) {
					dst[0] = *cur++;
					dst[1] = *cur++;
					dst[2] = *cur++;
					dst += 4;
				}
			}
			break;
		}
	}
}

void dx_copy_pixels(GF_VideoSurface *dst_s, const GF_VideoSurface *src_s, const GF_Window *src_wnd)
{
	/*handle YUV input*/
	if (get_yuv_base(src_s->pixel_format)==GF_PIXEL_YV12) {
		if (format_is_yuv(dst_s->pixel_format)) {
			/*generic YV planar to YUV (planar or not) */
			write_yv12_to_yuv(dst_s, src_s->video_buffer, src_s->pitch_y, src_s->pixel_format, src_s->width, src_s->height, src_wnd, src_s->u_ptr, src_s->v_ptr);
			return;
		}
	} else if (get_yuv_base(src_s->pixel_format)==GF_PIXEL_YV12_10) {
		if (format_is_yuv(dst_s->pixel_format)) {
			/*generic YV planar to YUV (planar or not) */
			gf_color_write_yv12_10_to_yuv(dst_s, src_s->video_buffer, src_s->u_ptr, src_s->v_ptr, src_s->pitch_y, src_s->width, src_s->height, src_wnd);
			return;
		}
	} else if (format_is_yuv(src_s->pixel_format)) {
		if (format_is_yuv(dst_s->pixel_format)) {
			write_yvyu_to_yuv(dst_s, src_s->video_buffer, src_s->pitch_y, src_s->pixel_format, src_s->width, src_s->height, src_wnd);
			return;
		}
	} else {
		switch (dst_s->pixel_format) {
		case GF_PIXEL_RGB_555:
			rgb_to_555(dst_s, src_s->video_buffer, src_s->pitch_y, src_s->width, src_s->height, src_s->pixel_format, src_wnd);
			return;
		case GF_PIXEL_RGB_565:
			rgb_to_565(dst_s, src_s->video_buffer, src_s->pitch_y, src_s->width, src_s->height, src_s->pixel_format, src_wnd);
			return;
		case GF_PIXEL_RGB_24:
		case GF_PIXEL_RGBS:
		case GF_PIXEL_BGR_24:
			rgb_to_24(dst_s, src_s->video_buffer, src_s->pitch_y, src_s->width, src_s->height, src_s->pixel_format, src_wnd);
			return;
		case GF_PIXEL_RGB_32:
		case GF_PIXEL_RGBD:
		case GF_PIXEL_RGBDS:
		case GF_PIXEL_BGR_32:
			rgb_to_32(dst_s, src_s->video_buffer, src_s->pitch_y, src_s->width, src_s->height, src_s->pixel_format, src_wnd);
			return;
		}
	}
	
	gf_stretch_bits(dst_s, (GF_VideoSurface*) src_s, NULL, (GF_Window *)src_wnd, 0xFF, 0, NULL, NULL);
}


