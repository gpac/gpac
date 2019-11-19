/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2019
 *					All rights reserved
 *
 *  This file is part of GPAC / software 2D rasterizer module
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
 *
 */


#include "rast_soft.h"

static void get_surface_world_matrix(GF_EVGSurface *surf, GF_Matrix2D *mat)
{
	gf_mx2d_init(*mat);
	if (surf->center_coords) {
		gf_mx2d_add_scale(mat, FIX_ONE, -FIX_ONE);
		gf_mx2d_add_translation(mat, INT2FIX(surf->width / 2), INT2FIX(surf->height / 2));
	}
}

GF_EXPORT
GF_EVGSurface *gf_evg_surface_new(Bool center_coords)
{
	GF_EVGSurface *surf;
	GF_SAFEALLOC(surf, GF_EVGSurface);
	if (surf) {
		surf->center_coords = center_coords;
		surf->texture_filter = GF_TEXTURE_FILTER_DEFAULT;
		surf->raster = evg_raster_new();
		surf->yuv_prof=1;
	}
	return surf;
}

EVG_Surface3DExt *evg_init_3d_surface(GF_EVGSurface *surf);

GF_EXPORT
GF_EVGSurface *gf_evg_surface3d_new()
{
	GF_EVGSurface *surf;
	GF_SAFEALLOC(surf, GF_EVGSurface);
	if (surf) {
		surf->texture_filter = GF_TEXTURE_FILTER_DEFAULT;
		surf->raster = evg_raster_new();
		surf->yuv_prof=1;

		surf->ext3d = evg_init_3d_surface(surf);
		if (!surf->ext3d) {
			gf_free(surf);
			return NULL;
		}
	}
	return surf;
}

GF_EXPORT
void gf_evg_surface_set_center_coords(GF_EVGSurface *surf, Bool center_coords)
{
	if (surf) surf->center_coords = center_coords;
}

GF_EXPORT
void gf_evg_surface_delete(GF_EVGSurface *surf)
{
	if (!surf)
		return;
	if (surf->stencil_pix_run) gf_free(surf->stencil_pix_run);
	surf->stencil_pix_run = NULL;
	if (surf->raster) evg_raster_del(surf->raster);
	surf->raster = NULL;
	if (surf->uv_alpha) gf_free(surf->uv_alpha);

	if (surf->ext3d) {
		if (surf->ext3d->pix_vals)
			gf_free(surf->ext3d->pix_vals);
		gf_free(surf->ext3d);
	}
	gf_free(surf);
}

GF_EXPORT
GF_Err gf_evg_surface_set_matrix(GF_EVGSurface *surf, GF_Matrix2D *mat)
{
	GF_Matrix2D tmp;
	if (!surf) return GF_BAD_PARAM;
	get_surface_world_matrix(surf, &surf->mat);
	surf->is_3d_matrix = GF_FALSE;
	if (!mat) return GF_OK;

	gf_mx2d_init(tmp);
	gf_mx2d_add_matrix(&tmp, mat);
	gf_mx2d_add_matrix(&tmp, &surf->mat);
	gf_mx2d_copy(surf->mat, tmp);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_evg_surface_set_matrix_3d(GF_EVGSurface *surf, GF_Matrix *mat)
{
	if (!surf) return GF_BAD_PARAM;
	get_surface_world_matrix(surf, &surf->mat);
	surf->is_3d_matrix = GF_FALSE;
	if (!mat) return GF_OK;
	gf_mx_copy(surf->mx3d, *mat);
	surf->is_3d_matrix = GF_TRUE;
	return GF_OK;
}

static void evg_surface_set_components_idx(GF_EVGSurface *surf)
{
	surf->idx_y1=surf->idx_u=surf->idx_v=0;
	surf->idx_a=surf->idx_r=surf->idx_g=surf->idx_b=0;
	switch (surf->pixelFormat) {
	case GF_PIXEL_YUYV:
		surf->idx_y1=0;
		surf->idx_u=1;
		surf->idx_v=3;
		break;
	case GF_PIXEL_YVYU:
		surf->idx_y1=0;
		surf->idx_u=3;
		surf->idx_v=1;
		break;
	case GF_PIXEL_UYVY:
		surf->idx_y1=1;
		surf->idx_u=0;
		surf->idx_v=2;
		break;
	case GF_PIXEL_VYUY:
		surf->idx_y1=1;
		surf->idx_u=2;
		surf->idx_v=0;
		break;
	case GF_PIXEL_ALPHAGREY:
		surf->idx_a = 0;
		surf->idx_g = 1;
		break;
	case GF_PIXEL_GREYALPHA:
		surf->idx_a = 1;
		surf->idx_g = 0;
		break;
	case GF_PIXEL_ARGB:
		surf->idx_a=0;
		surf->idx_r=1;
		surf->idx_g=2;
		surf->idx_b=3;
		break;
	case GF_PIXEL_RGBA:
		surf->idx_a=3;
		surf->idx_r=0;
		surf->idx_g=1;
		surf->idx_b=2;
		break;
	case GF_PIXEL_BGRA:
		surf->idx_a=3;
		surf->idx_r=2;
		surf->idx_g=1;
		surf->idx_b=0;
		break;
	case GF_PIXEL_ABGR:
		surf->idx_a=0;
		surf->idx_r=3;
		surf->idx_g=2;
		surf->idx_b=1;
		break;
	case GF_PIXEL_RGBX:
		surf->idx_r=0;
		surf->idx_g=1;
		surf->idx_b=2;
		break;
	case GF_PIXEL_BGRX:
		surf->idx_r=2;
		surf->idx_g=1;
		surf->idx_b=0;
		break;
	case GF_PIXEL_XRGB:
		surf->idx_r=1;
		surf->idx_g=2;
		surf->idx_b=3;
		break;
	case GF_PIXEL_XBGR:
		surf->idx_r=3;
		surf->idx_g=2;
		surf->idx_b=1;
		break;
	case GF_PIXEL_RGB:
		surf->idx_r=0;
		surf->idx_g=1;
		surf->idx_b=2;
		break;
	case GF_PIXEL_BGR:
		surf->idx_r=2;
		surf->idx_g=1;
		surf->idx_b=0;
		break;
	}
}

GF_Err evg_3d_resize(GF_EVGSurface *surf);


GF_EXPORT
GF_Err gf_evg_surface_attach_to_buffer(GF_EVGSurface *surf, u8 *pixels, u32 width, u32 height, s32 pitch_x, s32 pitch_y, GF_PixelFormat pixelFormat)
{
	u32 BPP;
	Bool size_changed=GF_FALSE;
	if (!surf || !pixels) return GF_BAD_PARAM;

	surf->not_8bits = GF_FALSE;
	switch (pixelFormat) {
	case GF_PIXEL_GREYSCALE:
		BPP = 1;
		break;
	case GF_PIXEL_ALPHAGREY:
	case GF_PIXEL_GREYALPHA:
		BPP = 2;
		break;
	case GF_PIXEL_RGB_444:
	case GF_PIXEL_RGB_555:
	case GF_PIXEL_RGB_565:
		BPP = 2;
		break;
	case GF_PIXEL_ARGB:
	case GF_PIXEL_BGRA:
	case GF_PIXEL_RGBA:
	case GF_PIXEL_ABGR:
	case GF_PIXEL_RGBX:
	case GF_PIXEL_BGRX:
	case GF_PIXEL_XRGB:
	case GF_PIXEL_XBGR:
		BPP = 4;
		break;
	case GF_PIXEL_BGR:
	case GF_PIXEL_RGB:
		BPP = 3;
		break;
	case GF_PIXEL_YUV:
	case GF_PIXEL_NV12:
	case GF_PIXEL_NV21:
	case GF_PIXEL_YUV422:
	case GF_PIXEL_YUV444:
	case GF_PIXEL_YUYV:
	case GF_PIXEL_YVYU:
	case GF_PIXEL_UYVY:
	case GF_PIXEL_VYUY:
		BPP = 1;
		break;
	case GF_PIXEL_YUV_10:
	case GF_PIXEL_NV12_10:
	case GF_PIXEL_NV21_10:
	case GF_PIXEL_YUV422_10:
	case GF_PIXEL_YUV444_10:
		BPP = 2;
		surf->not_8bits = GF_TRUE;
		break;
	default:
		return GF_NOT_SUPPORTED;
	}
	if (!pitch_x) pitch_x = BPP;
	surf->pitch_x = pitch_x;
	surf->pitch_y = pitch_y;
	if (!surf->stencil_pix_run || (surf->width != width)) {
		u32 run_size = sizeof(u32) * (width+2);
		if (surf->not_8bits) run_size *= 2;
		if (surf->stencil_pix_run) gf_free(surf->stencil_pix_run);
		surf->stencil_pix_run = (u32 *) gf_malloc(run_size);
	}
	if (surf->width != width) {
		surf->width = width;
		size_changed = GF_TRUE;
	}
	if (surf->height != height) {
		surf->height = height;
		size_changed = GF_TRUE;
	}
	surf->pixels = (char*)pixels;
	surf->pixelFormat = pixelFormat;
	surf->BPP = BPP;
	evg_surface_set_components_idx(surf);
	gf_evg_surface_set_matrix(surf, NULL);
	if (surf->ext3d && size_changed)
		evg_3d_resize(surf);
	return GF_OK;
}


GF_EXPORT
GF_Err gf_evg_surface_attach_to_texture(GF_EVGSurface *surf, GF_EVGStencil * sten)
{
	EVG_Texture *tx = (EVG_Texture *) sten;
	if (!surf || (tx->type != GF_STENCIL_TEXTURE)) return GF_BAD_PARAM;

	return gf_evg_surface_attach_to_buffer(surf, tx->pixels, tx->width, tx->height, 0, tx->stride, tx->pixel_format);
}


GF_EXPORT
GF_Err gf_evg_surface_clear(GF_EVGSurface *surf, GF_IRect *rc, u32 color)
{
	GF_IRect clear;
	if (!surf) return GF_BAD_PARAM;

	if (rc) {
		s32 _x, _y;
		if (surf->center_coords) {
			_x = rc->x + surf->width / 2;
			_y = surf->height / 2 - rc->y;
		} else {
			_x = rc->x;
			_y = rc->y - rc->height;
		}
		/*clip is outside our bounds, ignore*/
		if ((_x>=surf->width) || (_x + rc->width < 0))
			return GF_OK;
		if ((_y>=surf->height) || (_y + rc->height < 0))
			return GF_OK;

		clear.width = (u32) rc->width;
		if (_x>=0) {
			clear.x = (u32) _x;
		} else {
			if ( (s32) clear.width + _x < 0) return GF_BAD_PARAM;
			clear.width += _x;
			clear.x = 0;
		}
		if (clear.x + clear.width > surf->width)
			clear.width = surf->width - clear.x;

		if (!clear.width)
			return GF_OK;

		clear.height = (u32) rc->height;
		if (_y>=0) {
			clear.y = _y;
		} else {
			if ( (s32) clear.height + _y < 0) return GF_BAD_PARAM;
			clear.height += _y;
			clear.y = 0;
		}
		if (clear.y + clear.height > surf->height)
			clear.height = surf->height - clear.y;

		if (!clear.height)
			return GF_OK;
	} else {
		clear.x = clear.y = 0;
		clear.width = surf->width;
		clear.height = surf->height;
	}

	switch (surf->pixelFormat) {
	/*RGB formats*/
	case GF_PIXEL_GREYSCALE:
		return evg_surface_clear_grey(surf, clear, color);
	case GF_PIXEL_ALPHAGREY:
	case GF_PIXEL_GREYALPHA:
		return evg_surface_clear_alphagrey(surf, clear, color);
	case GF_PIXEL_ARGB:
	case GF_PIXEL_BGRA:
	case GF_PIXEL_RGBA:
	case GF_PIXEL_ABGR:
		return evg_surface_clear_argb(surf, clear, color);

	case GF_PIXEL_RGBX:
	case GF_PIXEL_BGRX:
	case GF_PIXEL_XRGB:
	case GF_PIXEL_XBGR:
		return evg_surface_clear_rgbx(surf, clear, color);

	case GF_PIXEL_RGB:
	case GF_PIXEL_BGR:
		return evg_surface_clear_rgb(surf, clear, color);
	case GF_PIXEL_RGB_444:
		return evg_surface_clear_444(surf, clear, color);
	case GF_PIXEL_RGB_555:
		return evg_surface_clear_555(surf, clear, color);
	case GF_PIXEL_RGB_565:
		return evg_surface_clear_565(surf, clear, color);

	/*YUV formats*/
	case GF_PIXEL_YUV:
		return evg_surface_clear_yuv420p(surf, clear, color);
	case GF_PIXEL_YUV422:
		return evg_surface_clear_yuv422p(surf, clear, color);
	case GF_PIXEL_NV12:
		return evg_surface_clear_nv12(surf, clear, color, GF_FALSE);
	case GF_PIXEL_NV21:
		return evg_surface_clear_nv12(surf, clear, color, GF_TRUE);
	case GF_PIXEL_YUV444:
		return evg_surface_clear_yuv444p(surf, clear, color);
	case GF_PIXEL_YUYV:
	case GF_PIXEL_YVYU:
	case GF_PIXEL_UYVY:
	case GF_PIXEL_VYUY:
		return evg_surface_clear_yuyv(surf, clear, color);
	case GF_PIXEL_YUV_10:
		return evg_surface_clear_yuv420p_10(surf, clear, color);
	case GF_PIXEL_NV12_10:
		return evg_surface_clear_nv12_10(surf, clear, color, GF_FALSE);
	case GF_PIXEL_NV21_10:
		return evg_surface_clear_nv12_10(surf, clear, color, GF_TRUE);
	case GF_PIXEL_YUV422_10:
		return evg_surface_clear_yuv422p_10(surf, clear, color);
	case GF_PIXEL_YUV444_10:
		return evg_surface_clear_yuv444p_10(surf, clear, color);
	default:
		return GF_BAD_PARAM;
	}
}

GF_EXPORT
GF_Err gf_evg_surface_set_raster_level(GF_EVGSurface *surf, GF_RasterQuality RasterSetting)
{
	if (!surf) return GF_BAD_PARAM;
	switch (RasterSetting) {
	case GF_RASTER_HIGH_QUALITY:
		surf->texture_filter = GF_TEXTURE_FILTER_HIGH_QUALITY;
		break;
	case GF_RASTER_MID:
		surf->texture_filter = GF_TEXTURE_FILTER_HIGH_QUALITY;
		break;
	case GF_RASTER_HIGH_SPEED:
	default:
		surf->texture_filter = GF_TEXTURE_FILTER_HIGH_SPEED;
		break;
	}
	return GF_OK;
}


GF_EXPORT
GF_Err gf_evg_surface_set_clipper(GF_EVGSurface *surf , GF_IRect *rc)
{
	if (!surf) return GF_BAD_PARAM;
	if (rc) {
		surf->clipper = *rc;
		surf->useClipper = 1;
		/*clipper was given in BIFS like coords, we work with bottom-min for rect, (0,0) top-left of surface*/
		if (surf->center_coords) {
			surf->clipper.x += surf->width / 2;
			surf->clipper.y = surf->height / 2 - rc->y;
		} else {
			surf->clipper.y -= rc->height;
		}

		if (surf->clipper.x <=0) {
			if (surf->clipper.x + (s32) surf->clipper.width < 0) return GF_BAD_PARAM;
			surf->clipper.width += surf->clipper.x;
			surf->clipper.x = 0;
		}
		if (surf->clipper.y <=0) {
			if (surf->clipper.y + (s32) surf->clipper.height < 0) return GF_BAD_PARAM;
			surf->clipper.height += surf->clipper.y;
			surf->clipper.y = 0;
		}
		if (surf->clipper.x + surf->clipper.width > (s32) surf->width) {
			surf->clipper.width = surf->width - surf->clipper.x;
		}
		if (surf->clipper.y + surf->clipper.height > (s32) surf->height) {
			surf->clipper.height = surf->height - surf->clipper.y;
		}
	} else {
		surf->useClipper = 0;
	}
	return GF_OK;
}


static Bool setup_grey_callback(GF_EVGSurface *surf, Bool for_3d)
{
	u32 a, uv_alpha_size=0;
	Bool use_const = GF_TRUE;

	//in 3D mode, we only write one pixel at a time except in YUV modes
	if (for_3d) {
		a = 1;
		use_const = GF_TRUE;
	}
	else if (surf->sten->type == GF_STENCIL_SOLID) {
		surf->fill_col = ((EVG_Brush *)surf->sten)->color;
		a = GF_COL_A(surf->fill_col);
	} else {
		a = 0;
		use_const = GF_FALSE;
	}

	if (use_const && !a) return GF_FALSE;

	surf->is_422 = GF_FALSE;
	surf->yuv_type = 0;
	surf->swap_uv = GF_FALSE;
	surf->yuv_flush_uv = NULL;

	if (surf->comp_mode) a=100;
	else if (surf->get_alpha) a=100;
	surf->fill_single = NULL;
	surf->fill_single_a = NULL;
	
	switch (surf->pixelFormat) {
	case GF_PIXEL_GREYSCALE:
		if (use_const) {
			if (a!=0xFF) {
				surf->gray_spans = (EVG_Raster_Span_Func) evg_grey_fill_const_a;
			} else {
				surf->gray_spans = (EVG_Raster_Span_Func) evg_grey_fill_const;
			}
		} else {
			surf->gray_spans = (EVG_Raster_Span_Func) evg_grey_fill_var;
		}
		if (surf->ext3d) {
			surf->fill_single = evg_grey_fill_single;
			surf->fill_single_a = evg_grey_fill_single_a;
		}
		break;
	case GF_PIXEL_ALPHAGREY:
	case GF_PIXEL_GREYALPHA:
		if (use_const) {
			if (a!=0xFF) {
				surf->gray_spans = (EVG_Raster_Span_Func) evg_alphagrey_fill_const_a;
			} else {
				surf->gray_spans = (EVG_Raster_Span_Func) evg_alphagrey_fill_const;
			}
		} else {
			surf->gray_spans = (EVG_Raster_Span_Func) evg_alphagrey_fill_var;
		}
		if (surf->ext3d) {
			surf->fill_single = evg_alphagrey_fill_single;
			surf->fill_single_a = evg_alphagrey_fill_single_a;
		}
		break;
	case GF_PIXEL_ARGB:
	case GF_PIXEL_RGBA:
	case GF_PIXEL_BGRA:
	case GF_PIXEL_ABGR:
		if (use_const) {
			if (!a) {
				surf->gray_spans = (EVG_Raster_Span_Func) evg_argb_fill_erase;
			} else if (a!=0xFF) {
				surf->gray_spans = (EVG_Raster_Span_Func) evg_argb_fill_const_a;
			} else {
				surf->gray_spans = (EVG_Raster_Span_Func) evg_argb_fill_const;
			}
		} else {
			surf->gray_spans = (EVG_Raster_Span_Func) evg_argb_fill_var;
		}
		if (surf->ext3d) {
			surf->fill_single = evg_argb_fill_single;
			surf->fill_single_a = evg_argb_fill_single_a;
		}
		break;

	case GF_PIXEL_RGBX:
	case GF_PIXEL_BGRX:
	case GF_PIXEL_XRGB:
	case GF_PIXEL_XBGR:
		if (use_const) {
			if (a!=0xFF) {
				surf->gray_spans = (EVG_Raster_Span_Func) evg_rgbx_fill_const_a;
			} else {
				surf->gray_spans = (EVG_Raster_Span_Func) evg_rgbx_fill_const;
			}
		} else {
			surf->gray_spans = (EVG_Raster_Span_Func) evg_rgbx_fill_var;
		}
		if (surf->ext3d) {
			surf->fill_single = evg_rgbx_fill_single;
			surf->fill_single_a = evg_rgbx_fill_single_a;
		}
		break;

	case GF_PIXEL_RGB:
	case GF_PIXEL_BGR:
		if (use_const) {
			if (a!=0xFF) {
				surf->gray_spans = (EVG_Raster_Span_Func) evg_rgb_fill_const_a;
			} else {
				surf->gray_spans = (EVG_Raster_Span_Func) evg_rgb_fill_const;
			}
		} else {
			surf->gray_spans = (EVG_Raster_Span_Func) evg_rgb_fill_var;
		}
		if (surf->ext3d) {
			surf->fill_single = evg_rgb_fill_single;
			surf->fill_single_a = evg_rgb_fill_single_a;
		}
		break;

	case GF_PIXEL_RGB_565:
		if (use_const) {
			if (a!=0xFF) {
				surf->gray_spans = (EVG_Raster_Span_Func) evg_565_fill_const_a;
			} else {
				surf->gray_spans = (EVG_Raster_Span_Func) evg_565_fill_const;
			}
		} else {
			surf->gray_spans = (EVG_Raster_Span_Func) evg_565_fill_var;
			assert(surf->sten->fill_run);
		}
		if (surf->ext3d) {
			surf->fill_single = evg_565_fill_single;
			surf->fill_single_a = evg_565_fill_single_a;
		}
		break;
	case GF_PIXEL_RGB_444:
		if (use_const) {
			if (a!=0xFF) {
				surf->gray_spans = (EVG_Raster_Span_Func) evg_444_fill_const_a;
			} else {
				surf->gray_spans = (EVG_Raster_Span_Func) evg_444_fill_const;
			}
		} else {
			surf->gray_spans = (EVG_Raster_Span_Func) evg_444_fill_var;
		}
		if (surf->ext3d) {
			surf->fill_single = evg_444_fill_single;
			surf->fill_single_a = evg_444_fill_single_a;
		}
		break;
	case GF_PIXEL_RGB_555:
		if (use_const) {
			if (a!=0xFF) {
				surf->gray_spans = (EVG_Raster_Span_Func) evg_555_fill_const_a;
			} else {
				surf->gray_spans = (EVG_Raster_Span_Func) evg_555_fill_const;
			}
		} else {
			surf->gray_spans = (EVG_Raster_Span_Func) evg_555_fill_var;
		}
		if (surf->ext3d) {
			surf->fill_single = evg_555_fill_single;
			surf->fill_single_a = evg_555_fill_single_a;
		}
		break;
	case GF_PIXEL_YUV:
		surf->yuv_type = EVG_YUV;
		if (use_const && !for_3d) {
			surf->yuv_flush_uv = evg_yuv420p_flush_uv_const;

			if (a!=0xFF) {
				surf->gray_spans = (EVG_Raster_Span_Func) evg_yuv420p_fill_const_a;
			} else {
				surf->gray_spans = (EVG_Raster_Span_Func) evg_yuv420p_fill_const;
			}
			uv_alpha_size=2*surf->width;
		} else {
			surf->yuv_flush_uv = evg_yuv420p_flush_uv_var;
			surf->gray_spans = (EVG_Raster_Span_Func) evg_yuv420p_fill_var;
			uv_alpha_size = 2 * 3*surf->width;
			if (for_3d)
				surf->sten = &surf->ext3d->yuv_sten;
		}
		break;
	case GF_PIXEL_NV21:
		surf->swap_uv = GF_TRUE;
	case GF_PIXEL_NV12:
		surf->yuv_type = EVG_YUV;

		if (surf->swap_uv) {
			surf->idx_u = 1;
			surf->idx_v = 0;
		} else {
			surf->idx_u = 0;
			surf->idx_v = 1;
		}

		if (use_const && !for_3d) {
			surf->yuv_flush_uv = evg_nv12_flush_uv_const;

			if (a!=0xFF) {
				surf->gray_spans = (EVG_Raster_Span_Func) evg_yuv420p_fill_const_a;
			} else {
				surf->gray_spans = (EVG_Raster_Span_Func) evg_yuv420p_fill_const;
			}
			uv_alpha_size = 2*surf->width;
		} else {
			surf->yuv_flush_uv = evg_nv12_flush_uv_var;
			surf->gray_spans = (EVG_Raster_Span_Func) evg_yuv420p_fill_var;
			uv_alpha_size = 2 * 3*surf->width;
		}
		break;
	case GF_PIXEL_YUV422:
		surf->yuv_type = EVG_YUV;
		surf->is_422 = GF_TRUE;
		if (use_const && !for_3d) {
			surf->yuv_flush_uv = evg_yuv422p_flush_uv_const;

			if (a!=0xFF) {
				surf->gray_spans = (EVG_Raster_Span_Func) evg_yuv420p_fill_const_a;
			} else {
				surf->gray_spans = (EVG_Raster_Span_Func) evg_yuv420p_fill_const;
			}
			uv_alpha_size = 2*surf->width;
		} else {
			surf->yuv_flush_uv = evg_yuv422p_flush_uv_var;
			surf->gray_spans = (EVG_Raster_Span_Func) evg_yuv420p_fill_var;
			uv_alpha_size = 2 * 3*surf->width;
		}
		break;
	case GF_PIXEL_YUV444:
		surf->yuv_type = EVG_YUV_444;
		if (use_const) {
			if (a!=0xFF) {
				surf->gray_spans = (EVG_Raster_Span_Func) evg_yuv444p_fill_const_a;
			} else {
				surf->gray_spans = (EVG_Raster_Span_Func) evg_yuv444p_fill_const;
			}
		} else {
			surf->gray_spans = (EVG_Raster_Span_Func) evg_yuv444p_fill_var;
		}
		break;
	case GF_PIXEL_YUYV:
	case GF_PIXEL_YVYU:
	case GF_PIXEL_UYVY:
	case GF_PIXEL_VYUY:
		surf->yuv_type = EVG_YUV;
		if (use_const && !for_3d) {
			if (a!=0xFF) {
				surf->gray_spans = (EVG_Raster_Span_Func) evg_yuyv_fill_const_a;
			} else {
				surf->gray_spans = (EVG_Raster_Span_Func) evg_yuyv_fill_const;
			}
			uv_alpha_size = 2*surf->width;
		} else {
			surf->gray_spans = (EVG_Raster_Span_Func) evg_yuyv_fill_var;
			uv_alpha_size = 2 * 3*surf->width;
		}
		break;

	case GF_PIXEL_YUV_10:
		surf->yuv_type = EVG_YUV;
		if (use_const && !for_3d) {
			surf->yuv_flush_uv = evg_yuv420p_10_flush_uv_const;

			if (a!=0xFF) {
				surf->gray_spans = (EVG_Raster_Span_Func) evg_yuv420p_10_fill_const_a;
			} else {
				surf->gray_spans = (EVG_Raster_Span_Func) evg_yuv420p_10_fill_const;
			}
			uv_alpha_size = 4*surf->width;
		} else {
			surf->yuv_flush_uv = evg_yuv420p_10_flush_uv_var;
			surf->gray_spans = (EVG_Raster_Span_Func) evg_yuv420p_10_fill_var;
			uv_alpha_size = 4*3*surf->width;
		}
		break;
	case GF_PIXEL_NV21_10:
		surf->swap_uv = GF_TRUE;
	case GF_PIXEL_NV12_10:
		surf->yuv_type = EVG_YUV;

		if (surf->swap_uv) {
			surf->idx_u = 1;
			surf->idx_v = 0;
		} else {
			surf->idx_u = 0;
			surf->idx_v = 1;
		}

		if (use_const && !for_3d) {
			surf->yuv_flush_uv = evg_nv12_10_flush_uv_const;

			if (a!=0xFF) {
				surf->gray_spans = (EVG_Raster_Span_Func) evg_yuv420p_10_fill_const_a;
			} else {
				surf->gray_spans = (EVG_Raster_Span_Func) evg_yuv420p_10_fill_const;
			}
			uv_alpha_size = 4*surf->width;
		} else {
			surf->yuv_flush_uv = evg_nv12_10_flush_uv_var;
			surf->gray_spans = (EVG_Raster_Span_Func) evg_yuv420p_10_fill_var;
			uv_alpha_size = 4 * 3*surf->width;
		}
		break;
	case GF_PIXEL_YUV422_10:
		surf->yuv_type = EVG_YUV;
		surf->is_422 = GF_TRUE;
		if (use_const && !for_3d) {
			surf->yuv_flush_uv = evg_yuv422p_10_flush_uv_const;

			if (a!=0xFF) {
				surf->gray_spans = (EVG_Raster_Span_Func) evg_yuv420p_10_fill_const_a;
			} else {
				surf->gray_spans = (EVG_Raster_Span_Func) evg_yuv420p_10_fill_const;
			}
			uv_alpha_size = 4*surf->width;
		} else {
			surf->yuv_flush_uv = evg_yuv422p_10_flush_uv_var;
			surf->gray_spans = (EVG_Raster_Span_Func) evg_yuv420p_10_fill_var;
			uv_alpha_size = 4 * 3*surf->width;
		}
		break;
	case GF_PIXEL_YUV444_10:
		surf->yuv_type = EVG_YUV_444;
		if (use_const) {
			if (a!=0xFF) {
				surf->gray_spans = (EVG_Raster_Span_Func) evg_yuv444p_10_fill_const_a;
			} else {
				surf->gray_spans = (EVG_Raster_Span_Func) evg_yuv444p_10_fill_const;
			}
		} else {
			surf->gray_spans = (EVG_Raster_Span_Func) evg_yuv444p_10_fill_var;
		}
		break;
	default:
		return GF_FALSE;
	}

	if (uv_alpha_size) {
		if (surf->uv_alpha_alloc < uv_alpha_size) {
			surf->uv_alpha_alloc = uv_alpha_size;
			surf->uv_alpha = gf_realloc(surf->uv_alpha, uv_alpha_size);
			memset(surf->uv_alpha, 0, uv_alpha_size);
		}
		memset(surf->uv_alpha, 0, sizeof(char)*surf->uv_alpha_alloc);
	}
	if (surf->yuv_type && for_3d)
		surf->sten = &surf->ext3d->yuv_sten;

	if (use_const && surf->yuv_type) {
		u8 y, cb, cr;
		evg_rgb_to_yuv(surf, surf->fill_col, &y, &cb, &cr);
		if (surf->swap_uv) {
			u8 t = cb;
			cb = cr;
			cr = (u8) t;
			surf->swap_uv = GF_FALSE;
		}

		surf->fill_col = GF_COL_ARGB(a, y, cb, cr);
		surf->fill_col_wide = evg_col_to_wide(surf->fill_col);
	}

	return GF_TRUE;
}


GF_EXPORT
GF_Err gf_evg_surface_set_path(GF_EVGSurface *surf, GF_Path *gp)
{
	if (!surf) return GF_BAD_PARAM;
	if (!gp || !gp->n_points) {
		surf->ftoutline.n_points = 0;
		surf->ftoutline.n_contours = 0;
		return GF_OK;
	}
	gf_path_flatten(gp);
	surf->ftoutline.n_points = gp->n_points;
	surf->ftoutline.n_contours = gp->n_contours;

	surf->ftoutline.tags = gp->tags;
	surf->ftoutline.contours = (s32*) gp->contours;

	/*store path bounds for gradient/textures*/
	gf_path_get_bounds(gp, &surf->path_bounds);
	/*invert Y (ft uses min Y)*/
	surf->path_bounds.y -= surf->path_bounds.height;

	surf->ftoutline.flags = 0;
	if (gp->flags & GF_PATH_FILL_ZERO_NONZERO) surf->ftoutline.flags = GF_PATH_FILL_ZERO_NONZERO;

	surf->ftoutline.n_points = gp->n_points;
	surf->ftoutline.points = gp->points;
	surf->mx = &surf->mat;
	return GF_OK;
}

/* static void gray_spans_stub(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf){} */

GF_EXPORT
GF_Err gf_evg_surface_fill(GF_EVGSurface *surf, GF_EVGStencil *sten)
{
	GF_Err e;
	GF_Rect rc;
	u32 max_gray;
	GF_Matrix2D mat, st_mat;
	Bool restore_filter;
	if (!surf || !sten) return GF_BAD_PARAM;
	if (!surf->ftoutline.n_points) return GF_OK;
	surf->sten = sten;

	/*setup ft raster calllbacks*/
	if (!setup_grey_callback(surf, GF_FALSE)) return GF_OK;

	/*	surf->gray_spans = gray_spans_stub; */

	/*TODO, check matrix with 3D transform*/

	get_surface_world_matrix(surf, &mat);
	restore_filter = GF_FALSE;
	/*get path frame for texture convertion */
	if (sten->type != GF_STENCIL_SOLID) {
		rc = surf->path_bounds;
		gf_mx2d_apply_rect(&mat, &rc);
		rc.x = rc.y = 0;
		/*assign target frame and matrix*/
		sten->frame = rc;
		gf_mx2d_copy(sten->pmat, surf->mat);
		gf_mx2d_inverse(&sten->pmat);

		gf_mx2d_copy(st_mat, sten->smat);
		gf_mx2d_init(sten->smat);

		switch (sten->type) {
		case GF_STENCIL_TEXTURE:
			if ( ((EVG_Texture *)sten)->tx_callback) {

			} else {
				if (! ((EVG_Texture *)sten)->pixels) return GF_BAD_PARAM;
			}

			if (((EVG_Texture *)sten)->mod & GF_TEXTURE_FLIP_Y) {
				if (!surf->center_coords) gf_mx2d_add_scale(&sten->smat, FIX_ONE, -FIX_ONE);
			} else {
				if (surf->center_coords) gf_mx2d_add_scale(&sten->smat, FIX_ONE, -FIX_ONE);
			}
			gf_mx2d_add_matrix(&sten->smat, &st_mat);
			gf_mx2d_add_matrix(&sten->smat, &mat);
			gf_mx2d_inverse(&sten->smat);

			evg_texture_init(sten, surf);
			if (((EVG_Texture *)sten)->filter == GF_TEXTURE_FILTER_DEFAULT) {
				restore_filter = GF_TRUE;
				((EVG_Texture *)sten)->filter = surf->texture_filter;
			}

			break;
		case GF_STENCIL_LINEAR_GRADIENT:
		{
			EVG_LinearGradient *lin = (EVG_LinearGradient *)sten;
			gf_mx2d_add_matrix(&sten->smat, &st_mat);
			gf_mx2d_add_matrix(&sten->smat, &mat);
			gf_mx2d_inverse(&sten->smat);
			/*and finalize matrix in gradient coord system*/
			gf_mx2d_add_matrix(&sten->smat, &lin->vecmat);
			gf_mx2d_add_scale(&sten->smat, INT2FIX(1<<EVGGRADIENTBITS), INT2FIX(1<<EVGGRADIENTBITS));

			/*init*/
			evg_gradient_precompute((EVG_BaseGradient *)lin, surf);

		}
		break;
		case GF_STENCIL_RADIAL_GRADIENT:
		{
			EVG_RadialGradient *rad = (EVG_RadialGradient*)sten;
			gf_mx2d_copy(sten->smat, st_mat);
			gf_mx2d_add_matrix(&sten->smat, &mat);
			gf_mx2d_inverse(&sten->smat);
			gf_mx2d_add_translation(&sten->smat, -rad->center.x, -rad->center.y);
			gf_mx2d_add_scale(&sten->smat, gf_invfix(rad->radius.x), gf_invfix(rad->radius.y));

			rad->d_f.x = gf_divfix(rad->focus.x - rad->center.x, rad->radius.x);
			rad->d_f.y = gf_divfix(rad->focus.y - rad->center.y, rad->radius.y);
			/*init*/
			evg_radial_init(rad);
			evg_gradient_precompute((EVG_BaseGradient *)rad, surf);
		}
		break;
		}
	}
	max_gray = surf->raster->max_gray_spans;
	/*force complete line callback for YUV 420/422*/
	if (surf->yuv_type==EVG_YUV)
		surf->raster->max_gray_spans = 0xFFFFFFFF;

	if (surf->useClipper) {
		surf->clip_xMin = surf->clipper.x;
		surf->clip_yMin = surf->clipper.y;
		surf->clip_xMax = (surf->clipper.x + surf->clipper.width);
		surf->clip_yMax = (surf->clipper.y + surf->clipper.height);
	} else {
		surf->clip_xMin = 0;
		surf->clip_yMin = 0;
		surf->clip_xMax = (surf->width);
		surf->clip_yMax = (surf->height);
	}

	/*and call the raster*/
	if (surf->is_3d_matrix)
		e = evg_raster_render_path_3d(surf);
	else
		e = evg_raster_render(surf);

	surf->raster->max_gray_spans = max_gray;

	/*restore stencil matrix*/
	if (sten->type != GF_STENCIL_SOLID) {
		gf_mx2d_copy(sten->smat, st_mat);
		if (restore_filter) ((EVG_Texture *)sten)->filter = GF_TEXTURE_FILTER_DEFAULT;
	}
	surf->sten = 0L;
	return e;
}



void gf_evg_surface_set_composite_mode(GF_EVGSurface *surf, GF_EVGCompositeMode comp_mode)
{
	if (surf) surf->comp_mode = comp_mode;
}

void gf_evg_surface_set_alpha_callback(GF_EVGSurface *surf, u8 (*get_alpha)(void *udta, u8 src_alpha, s32 x, s32 y), void *cbk)
{
	if (surf) {
		surf->get_alpha = get_alpha;
		surf->get_alpha_udta = cbk;
	}
}

GF_Err gf_evg_surface_set_projection(GF_EVGSurface *surf, GF_Matrix *mx)
{
	if (!surf || !surf->ext3d) return GF_BAD_PARAM;
	gf_mx_copy(surf->ext3d->proj, *mx);
	return GF_OK;
}
GF_Err gf_evg_surface_set_modelview(GF_EVGSurface *surf, GF_Matrix *mx)
{
	if (!surf || !surf->ext3d) return GF_BAD_PARAM;
	gf_mx_copy(surf->ext3d->modelview, *mx);
	return GF_OK;
}

GF_Err evg_raster_render3d(GF_EVGSurface *surf, u32 *indices, u32 nb_idx, Float *vertices, u32 nb_vertices, u32 nb_comp, GF_EVGPrimitiveType prim_type);

GF_Err gf_evg_surface_draw_array(GF_EVGSurface *surf, u32 *indices, u32 nb_idx, Float *vertices, u32 nb_vertices, u32 nb_comp, GF_EVGPrimitiveType prim_type)
{
	GF_Err e;
	u32 max_gray;
	if (!surf || !surf->ext3d) return GF_BAD_PARAM;

	/*setup ft raster calllbacks*/
	if (!setup_grey_callback(surf, GF_TRUE)) return GF_OK;

	if (surf->useClipper) {
		surf->clip_xMin = surf->clipper.x;
		surf->clip_yMin = surf->clipper.y;
		surf->clip_xMax = (surf->clipper.x + surf->clipper.width);
		surf->clip_yMax = (surf->clipper.y + surf->clipper.height);
	} else {
		surf->clip_xMin = 0;
		surf->clip_yMin = 0;
		surf->clip_xMax = (surf->width);
		surf->clip_yMax = (surf->height);
	}
	max_gray = surf->raster->max_gray_spans;
	/*force complete line callback for YUV 420/422*/
	if (surf->yuv_type==EVG_YUV)
		surf->raster->max_gray_spans = 0xFFFFFFFF;

	/*and call the raster*/
	e = evg_raster_render3d(surf, indices, nb_idx, vertices, nb_vertices, nb_comp, prim_type);
	surf->raster->max_gray_spans =  max_gray;
	return e;
}

GF_Err evg_raster_render3d_path(GF_EVGSurface *surf, GF_Path *path, Float z);

GF_Err gf_evg_surface_draw_path(GF_EVGSurface *surf, GF_Path *path, Float z)
{
	GF_Err e;
	u32 max_gray;
	if (!surf || !surf->ext3d) return GF_BAD_PARAM;

	/*setup ft raster calllbacks*/
	if (!setup_grey_callback(surf, GF_TRUE)) return GF_OK;

	if (surf->useClipper) {
		surf->clip_xMin = surf->clipper.x;
		surf->clip_yMin = surf->clipper.y;
		surf->clip_xMax = (surf->clipper.x + surf->clipper.width);
		surf->clip_yMax = (surf->clipper.y + surf->clipper.height);
	} else {
		surf->clip_xMin = 0;
		surf->clip_yMin = 0;
		surf->clip_xMax = (surf->width);
		surf->clip_yMax = (surf->height);
	}
	max_gray = surf->raster->max_gray_spans;
	/*force complete line callback for YUV 420/422*/
	if (surf->yuv_type==EVG_YUV)
		surf->raster->max_gray_spans = 0xFFFFFFFF;

	/*and call the raster*/
	e = evg_raster_render3d_path(surf, path, z);
	surf->raster->max_gray_spans =  max_gray;
	return e;
}
