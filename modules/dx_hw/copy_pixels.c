/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
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


static void VR_write_yv12_to_yuv(GF_VideoSurface *vs,  unsigned char *src, u32 src_stride, u32 src_pf,
								 u32 src_width, u32 src_height, const GF_Window *src_wnd)
{
	unsigned char *pY, *pU, *pV;
	pY = src;
	pU = src + src_stride * src_height;
	pV = src + 5*src_stride * src_height/4;

	pY = pY + src_stride * src_wnd->y + src_wnd->x;
	pU = pU + (src_stride * src_wnd->y / 2 + src_wnd->x) / 2;
	pV = pV + (src_stride * src_wnd->y / 2 + src_wnd->x) / 2;


	if (is_planar_yuv(vs->pixel_format)) {
		/*complete source copy*/
		if ( (vs->pitch == (s32) src_stride) && (src_wnd->w == src_width) && (src_wnd->h == src_height)) {
			assert(!src_wnd->x);
			assert(!src_wnd->y);
			memcpy(vs->video_buffer, pY, sizeof(unsigned char)*src_width*src_height);
			if (vs->pixel_format == GF_PIXEL_YV12) {
				memcpy(vs->video_buffer + vs->pitch * vs->height, pV, sizeof(unsigned char)*src_width*src_height/4);
				memcpy(vs->video_buffer + 5 * vs->pitch * vs->height/4, pU, sizeof(unsigned char)*src_width*src_height/4);
			} else {
				memcpy(vs->video_buffer + vs->pitch * vs->height, pU, sizeof(unsigned char)*src_width*src_height/4);
				memcpy(vs->video_buffer + 5 * vs->pitch * vs->height/4, pV, sizeof(unsigned char)*src_width*src_height/4);
			}
		} else {
			u32 i;
			unsigned char *dst, *src, *dst2, *src2, *dst3, *src3;

			src = pY;
			dst = vs->video_buffer;
			
			src2 = (vs->pixel_format != GF_PIXEL_YV12) ? pU : pV;
			dst2 = vs->video_buffer + vs->pitch * vs->height;
			src3 = (vs->pixel_format != GF_PIXEL_YV12) ? pV : pU;
			dst3 = vs->video_buffer + 5*vs->pitch * vs->height/4;
			for (i=0; i<src_wnd->h; i++) {
				memcpy(dst, src, src_wnd->w);
				src += src_stride;
				dst += vs->pitch;
				if (i<src_wnd->h/2) {
					memcpy(dst2, src2, src_wnd->w/2);
					src2 += src_stride/2;
					dst2 += vs->pitch/2;
					memcpy(dst3, src3, src_wnd->w/2);
					src3 += src_stride/2;
					dst3 += vs->pitch/2;
				}
			}
		}
	} else if (vs->pixel_format==GF_PIXEL_UYVY) {
		u32 i, j;
		unsigned char *dst, *y, *u, *v;
		for (i=0; i<src_wnd->h; i++) {
			y = pY + i*src_stride;
			u = pU + (i/2) * src_stride/2;
			v = pV + (i/2) * src_stride/2;
			dst = vs->video_buffer + i*vs->pitch;

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
	} else if (vs->pixel_format==GF_PIXEL_YUY2) {
		u32 i, j;
		unsigned char *dst, *y, *u, *v;
		for (i=0; i<src_wnd->h; i++) {
			y = pY + i*src_stride;
			u = pU + (i/2) * src_stride/2;
			v = pV + (i/2) * src_stride/2;
			dst = vs->video_buffer + i*vs->pitch;

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
			dst = vs->video_buffer + i*vs->pitch;

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

u32 get_bpp(u32 pf)
{
	switch (pf) {
	case GF_PIXEL_RGB_555:
	case GF_PIXEL_RGB_565:
		return 2;
	case GF_PIXEL_RGB_24:
	case GF_PIXEL_BGR_24:
		return 3;
	case GF_PIXEL_RGB_32:
	case GF_PIXEL_BGR_32:
	case GF_PIXEL_ARGB:
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
			memcpy(vs->video_buffer + i*vs->pitch, src, sizeof(unsigned char) * BPP * src_wnd->w);
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
			memcpy(vs->video_buffer + i*vs->pitch, src, sizeof(unsigned char) * BPP * src_wnd->w);
		}
		return;
	}
	/*nope get all pixels*/
	for (i=0; i<src_wnd->h; i++) {
		dst = vs->video_buffer + i*vs->pitch;
		cur = src + i*src_stride;
		for (j=0; j<src_wnd->w; j++) {
			switch (src_pf) {
			case GF_PIXEL_RGB_24:
				r = *cur++;
				g = *cur++;
				b = *cur++;
				* ((unsigned short *)dst) = GF_COL_555(r, g, b);
				dst += 2;
				break;
			}
		}
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
			memcpy(vs->video_buffer + i*vs->pitch, src, sizeof(unsigned char) * BPP * src_wnd->w);
		}
		return;
	}
	/*nope get all pixels*/
	for (i=0; i<src_wnd->h; i++) {
		dst = vs->video_buffer + i*vs->pitch;
		cur = src + i*src_stride;
		for (j=0; j<src_wnd->w; j++) {
			switch (src_pf) {
			case GF_PIXEL_RGB_24:
				r = *cur++;
				g = *cur++;
				b = *cur++;
				* ((unsigned short *)dst) = GF_COL_565(r, g, b);
				dst += 2;
				break;
			}
		}
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
			memcpy(vs->video_buffer + i*vs->pitch, src, sizeof(unsigned char) * BPP * src_wnd->w);
		}
		return;
	}
	/*get all pixels*/
	isBGR = vs->pixel_format==GF_PIXEL_BGR_32;
	if (isBGR) {
		for (i=0; i<src_wnd->h; i++) {
			dst = vs->video_buffer + i*vs->pitch;
			cur = src + i*src_stride;
			for (j=0; j<src_wnd->w; j++) {
				switch (src_pf) {
				case GF_PIXEL_RGB_24:
					dst[0] = *cur++;
					dst[1] = *cur++;
					dst[2] = *cur++;
					dst += 4;
					break;
				case GF_PIXEL_BGR_24:
					dst[2] = *cur++;
					dst[1] = *cur++;
					dst[0] = *cur++;
					dst += 4;
					break;
				}
			}
		}
	} else {
		for (i=0; i<src_wnd->h; i++) {
			dst = vs->video_buffer + i*vs->pitch;
			cur = src + i*src_stride;
			for (j=0; j<src_wnd->w; j++) {
				switch (src_pf) {
				case GF_PIXEL_RGB_24:
					dst[2] = *cur++;
					dst[1] = *cur++;
					dst[0] = *cur++;
					dst += 4;
					break;
				case GF_PIXEL_BGR_24:
					dst[0] = *cur++;
					dst[1] = *cur++;
					dst[2] = *cur++;
					dst += 4;
					break;
				}
			}
		}
	}
}

void dx_copy_pixels(GF_VideoSurface *dst_s, const GF_VideoSurface *src_s, const GF_Window *src_wnd)
{
	/*handle YUV input*/
	if (get_yuv_base(src_s->pixel_format)==GF_PIXEL_YV12) {
		if (format_is_yuv(dst_s->pixel_format)) {
			/*generic YV planar to YUV (planar or not) */
			VR_write_yv12_to_yuv(dst_s, src_s->video_buffer, src_s->pitch, src_s->pixel_format, src_s->width, src_s->height, src_wnd);
		} else {
			gf_stretch_bits(dst_s, (GF_VideoSurface*) src_s, NULL, (GF_Window *)src_wnd, 0, 0xFF, 0, NULL, NULL);
		}
	} else {
		switch (dst_s->pixel_format) {
		case GF_PIXEL_RGB_555:
			rgb_to_555(dst_s, src_s->video_buffer, src_s->pitch, src_s->width, src_s->height, src_s->pixel_format, src_wnd);
			break;
		case GF_PIXEL_RGB_565:
			rgb_to_565(dst_s, src_s->video_buffer, src_s->pitch, src_s->width, src_s->height, src_s->pixel_format, src_wnd);
			break;
		case GF_PIXEL_RGB_24:
		case GF_PIXEL_BGR_24:
			rgb_to_24(dst_s, src_s->video_buffer, src_s->pitch, src_s->width, src_s->height, src_s->pixel_format, src_wnd);
			break;
		case GF_PIXEL_RGB_32:
		case GF_PIXEL_BGR_32:
			rgb_to_32(dst_s, src_s->video_buffer, src_s->pitch, src_s->width, src_s->height, src_s->pixel_format, src_wnd);
			break;
		}
	}
}


