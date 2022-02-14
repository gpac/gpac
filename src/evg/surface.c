/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2022
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


static GF_Err evg_raster_ctx_init(EVGRasterCtx *raster_ctx, GF_EVGSurface *surf)
{
	raster_ctx->max_gray_spans = raster_ctx->alloc_gray_spans = FT_MAX_GRAY_SPANS;
	raster_ctx->gray_spans = gf_malloc(sizeof(EVG_Span)* raster_ctx->max_gray_spans);
	if (!raster_ctx->gray_spans) return GF_OUT_OF_MEM;
	raster_ctx->surf = surf;
	return GF_OK;
}

static void evg_raster_ctx_uninit(EVGRasterCtx *raster_ctx)
{
	if (raster_ctx->gray_spans) gf_free(raster_ctx->gray_spans);
	if (raster_ctx->stencil_pix_run) gf_free(raster_ctx->stencil_pix_run);
	if (raster_ctx->stencil_pix_run2) gf_free(raster_ctx->stencil_pix_run2);
	if (raster_ctx->stencil_pix_run3) gf_free(raster_ctx->stencil_pix_run3);
	if (raster_ctx->uv_alpha) gf_free(raster_ctx->uv_alpha);
}

GF_EXPORT
GF_EVGSurface *gf_evg_surface_new(Bool center_coords)
{
	GF_Err e;
	GF_EVGSurface *surf;
	GF_SAFEALLOC(surf, GF_EVGSurface);
	if (!surf) return NULL;

	surf->center_coords = center_coords;
	surf->qlevel = GF_RASTER_HIGH_QUALITY;
	surf->yuv_prof = 1;
	e = evg_raster_ctx_init(&surf->raster_ctx, surf);
	if (e) {
		gf_free(surf);
		return NULL;
	}
	return surf;
}

EVG_Surface3DExt *evg_init_3d_surface(GF_EVGSurface *surf);

GF_EXPORT
GF_Err gf_evg_surface_enable_3d(GF_EVGSurface *surf)
{
	if (!surf) return GF_BAD_PARAM;
	if (surf->ext3d) return GF_OK;
	surf->ext3d = evg_init_3d_surface(surf);
	if (!surf->ext3d) return GF_OUT_OF_MEM;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_evg_enable_threading(GF_EVGSurface *surf, s32 nb_threads)
{
	u32 i, run_size;
	char szName[100];
	if (!surf || surf->nb_threads) return GF_BAD_PARAM;

	if (nb_threads<0) {
		GF_SystemRTInfo rti;
		gf_sys_get_rti(0, &rti, 0);
		if (rti.nb_cores<2) return GF_IO_ERR;
		nb_threads = rti.nb_cores-1;
	}
	surf->nb_threads = (u32) nb_threads;
	if (!surf->nb_threads) return GF_OK;
	surf->th_raster_ctx = gf_malloc(sizeof(EVGRasterCtx) * surf->nb_threads);
	sprintf(szName, "EVGMX%p", surf);
	surf->raster_mutex = gf_mx_new(szName);

	if (!surf->th_raster_ctx || !surf->raster_mutex) {
		surf->nb_threads = 0;
		return GF_OUT_OF_MEM;
	}
	run_size = surf->width ? (sizeof(u32) * (surf->width+2)) : 0;
	if (surf->width && surf->not_8bits) run_size *= 2;


	memset(surf->th_raster_ctx, 0, sizeof(EVGRasterCtx) * surf->nb_threads);
	for (i=0; i<surf->nb_threads; i++) {
		EVGRasterCtx *rctx = &surf->th_raster_ctx[i];
		sprintf(szName, "gf_evg_%d", i+1);
		rctx->th = gf_th_new(szName);
		if (!rctx->th) {
			surf->nb_threads = i;
			break;
		}
		rctx->surf = surf;
		rctx->max_gray_spans = rctx->alloc_gray_spans = FT_MAX_GRAY_SPANS;
		rctx->gray_spans = gf_malloc(sizeof(EVG_Span) * rctx->max_gray_spans);

		if (run_size) {
			rctx->stencil_pix_run = gf_realloc(rctx->stencil_pix_run, run_size);
		}
		
		if (!rctx->gray_spans || !rctx->stencil_pix_run) {
			surf->nb_threads = i;
			break;
		}
		rctx->th_state = 1;
	}
	surf->raster_sem = gf_sema_new(surf->nb_threads, 0);
	if (!surf->raster_sem)
		surf->nb_threads = 0;

	//launch all threads
	for (i=0; i<surf->nb_threads; i++) {
		EVGRasterCtx *rctx = &surf->th_raster_ctx[i];
		gf_th_run(rctx->th, th_sweep_lines, rctx);
	}
	return GF_OK;
}


GF_EXPORT
void gf_evg_surface_set_center_coords(GF_EVGSurface *surf, Bool center_coords)
{
	if (surf) surf->center_coords = center_coords;
}

GF_EXPORT
void gf_evg_surface_delete(GF_EVGSurface *surf)
{
	u32 i;
	if (!surf)
		return;

	for (i=0; i<surf->max_lines; i++) {
		gf_free(surf->scanlines[i].cells);
		if (surf->scanlines[i].pixels)
			gf_free(surf->scanlines[i].pixels);
	}
	gf_free(surf->scanlines);

	if (surf->internal_mask) gf_free(surf->internal_mask);

	evg_raster_ctx_uninit(&surf->raster_ctx);

	if (surf->ext3d) {
		gf_free(surf->ext3d);
	}
	if (surf->nb_threads) {
		for (i=0; i<surf->nb_threads; i++) {
			EVGRasterCtx *rctx = &surf->th_raster_ctx[i];
			rctx->th_state = 0;
		}
		gf_sema_notify(surf->raster_sem, surf->nb_threads);

		for (i=0; i<surf->nb_threads; i++) {
			EVGRasterCtx *rctx = &surf->th_raster_ctx[i];
			//wait for destruction
			while (!rctx->th_state) {
				gf_sleep(0);
			}
			gf_th_del(surf->th_raster_ctx[i].th);
			evg_raster_ctx_uninit(&surf->th_raster_ctx[i]);
		}
		gf_free(surf->th_raster_ctx);
		gf_mx_del(surf->raster_mutex);
		gf_sema_del(surf->raster_sem);
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
	gf_mx2d_copy(surf->shader_mx, *mat);

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
	case GF_PIXEL_YUVA444_PACK:
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
		surf->idx_a=3;
		break;
	case GF_PIXEL_BGRX:
		surf->idx_r=2;
		surf->idx_g=1;
		surf->idx_b=0;
		surf->idx_a=3;
		break;
	case GF_PIXEL_XRGB:
		surf->idx_a=0;
		surf->idx_r=1;
		surf->idx_g=2;
		surf->idx_b=3;
		break;
	case GF_PIXEL_XBGR:
		surf->idx_a=0;
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

GF_EXPORT
GF_Err gf_evg_surface_attach_to_buffer(GF_EVGSurface *surf, u8 *pixels, u32 width, u32 height, s32 pitch_x, s32 pitch_y, GF_PixelFormat pixelFormat)
{
	u32 BPP;
	Bool size_changed=GF_FALSE;
	if (!surf || !pixels) return GF_BAD_PARAM;

	surf->is_transparent = GF_FALSE;
	surf->not_8bits = GF_FALSE;
	switch (pixelFormat) {
	case GF_PIXEL_GREYSCALE:
		BPP = 1;
		break;
	case GF_PIXEL_ALPHAGREY:
	case GF_PIXEL_GREYALPHA:
		BPP = 2;
		surf->is_transparent = GF_TRUE;
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
		surf->is_transparent = GF_TRUE;
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
	case GF_PIXEL_YVU:
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
	case GF_PIXEL_YUVA444_PACK:
		surf->is_transparent = GF_TRUE;
		BPP = 4;
		break;
	default:
		return GF_NOT_SUPPORTED;
	}
	if (!pitch_x) pitch_x = BPP;
	surf->pitch_x = pitch_x;
	surf->pitch_y = pitch_y;
	if (!surf->raster_ctx.stencil_pix_run || (surf->width != width)) {
		u32 i;
		u32 run_size = sizeof(u32) * (width+2);
		if (surf->not_8bits) run_size *= 2;
		surf->raster_ctx.stencil_pix_run = gf_realloc(surf->raster_ctx.stencil_pix_run, run_size);
		if (!surf->raster_ctx.stencil_pix_run) return GF_OUT_OF_MEM;

		if (surf->raster_ctx.stencil_pix_run2) {
			surf->raster_ctx.stencil_pix_run2 = gf_realloc(surf->raster_ctx.stencil_pix_run2, run_size);
			if (!surf->raster_ctx.stencil_pix_run2) return GF_OUT_OF_MEM;
		}
		if (surf->raster_ctx.stencil_pix_run3) {
			surf->raster_ctx.stencil_pix_run3 = gf_realloc(surf->raster_ctx.stencil_pix_run3, run_size);
			if (!surf->raster_ctx.stencil_pix_run3) return GF_OUT_OF_MEM;
		}

		for (i=0; i<surf->nb_threads; i++) {
			EVGRasterCtx *rctx = &surf->th_raster_ctx[i];
			rctx->stencil_pix_run = gf_realloc(rctx->stencil_pix_run, run_size);
			if (!rctx->stencil_pix_run) return GF_OUT_OF_MEM;

			if (rctx->stencil_pix_run2) {
				rctx->stencil_pix_run2 = gf_realloc(rctx->stencil_pix_run2, run_size);
				if (!rctx->stencil_pix_run2) return GF_OUT_OF_MEM;
			}
			if (rctx->stencil_pix_run3) {
				rctx->stencil_pix_run3 = gf_realloc(rctx->stencil_pix_run3, run_size);
				if (!rctx->stencil_pix_run3) return GF_OUT_OF_MEM;
			}
		}
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

	if (size_changed && surf->internal_mask) {
		gf_free(surf->internal_mask);
		surf->internal_mask = NULL;
	}
	if (surf->ext3d && size_changed) {
		surf->ext3d->depth_buffer = NULL;

		if (!surf->vp_w || !surf->vp_h) {
			surf->vp_x = 0;
			surf->vp_y = 0;
			surf->vp_w = surf->width;
			surf->vp_h = surf->height;
		}
	}
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
		if ((_x>=(s32) surf->width) || (_x + rc->width < 0))
			return GF_OK;
		if ((_y>= (s32) surf->height) || (_y + rc->height < 0))
			return GF_OK;

		clear.width = (u32) rc->width;
		if (_x>=0) {
			clear.x = (u32) _x;
		} else {
			if ( (s32) clear.width + _x < 0) return GF_BAD_PARAM;
			clear.width += _x;
			clear.x = 0;
		}
		if (clear.x + clear.width > (s32) surf->width)
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
		if (clear.y + clear.height > (s32) surf->height)
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
	case GF_PIXEL_YVU:
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

	case GF_PIXEL_YUVA444_PACK:
		return evg_surface_clear_argb(surf, clear, gf_evg_argb_to_ayuv(surf, color) );
	default:
		return GF_BAD_PARAM;
	}
}

GF_EXPORT
GF_Err gf_evg_surface_set_raster_level(GF_EVGSurface *surf, GF_RasterQuality RasterSetting)
{
	if (!surf) return GF_BAD_PARAM;
	surf->qlevel = RasterSetting;
	return GF_OK;
}
GF_EXPORT
GF_RasterQuality gf_evg_surface_get_raster_level(GF_EVGSurface *surf)
{
	if (!surf) return GF_RASTER_HIGH_QUALITY;
	return surf->qlevel;
}

GF_EXPORT
GF_Err gf_evg_surface_force_aa(GF_EVGSurface *surf)
{
	if (!surf) return GF_BAD_PARAM;
	surf->aa_level = 0xFF;
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

GF_EXPORT
Bool gf_evg_surface_use_clipper(GF_EVGSurface *surf)
{
	if (!surf) return 0;
	return surf->useClipper ? GF_TRUE : GF_FALSE;
}

static Bool setup_grey_callback(GF_EVGSurface *surf, Bool for_3d, Bool multi_sten)
{
	u32 a, uv_alpha_size=0;
	Bool use_const = GF_TRUE;

	//default fill_run callback
	surf->fill_run = evg_fill_run;

	//mask mode draw,
	if (surf->mask_mode == GF_EVGMASK_DRAW) {
		if (surf->sten && (surf->sten->type == GF_STENCIL_SOLID)) {
			EVG_Brush *sc = (EVG_Brush *)surf->sten;
			u32 col = sc->color;
			if (sc->alpha < 0xFF) {
				u32 ca = ((u32) (GF_COL_A(col) + 1) * sc->alpha) >> 8;
				col = ( ((ca<<24) & 0xFF000000) ) | (col & 0x00FFFFFF);
			}
			surf->fill_col = col;
			surf->fill_col_wide = evg_col_to_wide(surf->fill_col);
			if (GF_COL_A(col)<0xFF)
				surf->fill_spans = (EVG_SpanFunc) evg_grey_fill_const_a;
			else
				surf->fill_spans = (EVG_SpanFunc) evg_grey_fill_const;
		} else {
			surf->fill_spans = (EVG_SpanFunc) evg_grey_fill_var;
		}
		if (surf->ext3d) {
			surf->fill_single = evg_grey_fill_single;
			surf->fill_single_a = evg_grey_fill_single_a;
		}
		return GF_TRUE;
	}

	//in 3D mode, we only write one pixel at a time except in YUV modes
	if (for_3d) {
		a = 1;
		use_const = GF_TRUE;
	}
	else if (!multi_sten && surf->sten && (surf->sten->type == GF_STENCIL_SOLID)) {
		EVG_Brush *sc = (EVG_Brush *)surf->sten;
		u32 col = sc->color;
		if (sc->alpha < 0xFF) {
			u32 ca = ((u32) (GF_COL_A(col) + 1) * sc->alpha) >> 8;
			col = ( ((ca<<24) & 0xFF000000) ) | (col & 0x00FFFFFF);
		}
		surf->fill_col = col;
		a = GF_COL_A(surf->fill_col);
	} else {
		a = 0;
		use_const = GF_FALSE;
	}
	//mask is used, force fill with variable alpha (xxx_fill_var) and use masking callback for fill_run
	if ((surf->mask_mode == GF_EVGMASK_USE) || (surf->mask_mode == GF_EVGMASK_RECORD) ){
		a = 0;
		use_const = GF_FALSE;
		if (surf->mask_mode == GF_EVGMASK_USE)
			surf->fill_run = evg_fill_run_mask;
	} else if (surf->mask_mode == GF_EVGMASK_USE_INV) {
		a = 0;
		use_const = GF_FALSE;
		surf->fill_run = evg_fill_run_mask_inv;
	}

	if (use_const && !a && !surf->is_transparent) return GF_FALSE;

	surf->is_422 = GF_FALSE;
	surf->yuv_type = 0;
	surf->swap_uv = GF_FALSE;
	surf->yuv_flush_uv = NULL;
	surf->direct_yuv_3d = GF_FALSE;

	if (surf->comp_mode) a=100;
	else if (surf->get_alpha) a=100;
	surf->fill_single = NULL;
	surf->fill_single_a = NULL;
	
	switch (surf->pixelFormat) {
	case GF_PIXEL_GREYSCALE:
		if (use_const) {
			if (a!=0xFF) {
				surf->fill_spans = (EVG_SpanFunc) evg_grey_fill_const_a;
			} else {
				surf->fill_spans = (EVG_SpanFunc) evg_grey_fill_const;
			}
		} else {
			surf->fill_spans = (EVG_SpanFunc) evg_grey_fill_var;
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
				surf->fill_spans = (EVG_SpanFunc) evg_alphagrey_fill_const_a;
			} else {
				surf->fill_spans = (EVG_SpanFunc) evg_alphagrey_fill_const;
			}
		} else {
			surf->fill_spans = (EVG_SpanFunc) evg_alphagrey_fill_var;
		}
		if (surf->ext3d) {
			surf->fill_single = evg_alphagrey_fill_single;
			surf->fill_single_a = evg_alphagrey_fill_single_a;
		}
		break;
	case GF_PIXEL_YUVA444_PACK:
		surf->yuv_type = EVG_YUV;
	case GF_PIXEL_ARGB:
	case GF_PIXEL_RGBA:
	case GF_PIXEL_BGRA:
	case GF_PIXEL_ABGR:
		if (use_const) {
			if (!a) {
				surf->fill_spans = (EVG_SpanFunc) evg_argb_fill_erase;
			} else if (a!=0xFF) {
				surf->fill_spans = (EVG_SpanFunc) evg_argb_fill_const_a;
			} else {
				surf->fill_spans = (EVG_SpanFunc) evg_argb_fill_const;
			}
		} else {
			surf->fill_spans = (EVG_SpanFunc) evg_argb_fill_var;
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
				surf->fill_spans = (EVG_SpanFunc) evg_rgbx_fill_const_a;
			} else {
				surf->fill_spans = (EVG_SpanFunc) evg_rgbx_fill_const;
			}
		} else {
			surf->fill_spans = (EVG_SpanFunc) evg_rgbx_fill_var;
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
				surf->fill_spans = (EVG_SpanFunc) evg_rgb_fill_const_a;
			} else {
				surf->fill_spans = (EVG_SpanFunc) evg_rgb_fill_const;
			}
		} else {
			surf->fill_spans = (EVG_SpanFunc) evg_rgb_fill_var;
		}
		if (surf->ext3d) {
			surf->fill_single = evg_rgb_fill_single;
			surf->fill_single_a = evg_rgb_fill_single_a;
		}
		break;

	case GF_PIXEL_RGB_565:
		if (use_const) {
			if (a!=0xFF) {
				surf->fill_spans = (EVG_SpanFunc) evg_565_fill_const_a;
			} else {
				surf->fill_spans = (EVG_SpanFunc) evg_565_fill_const;
			}
		} else {
			surf->fill_spans = (EVG_SpanFunc) evg_565_fill_var;
		}
		if (surf->ext3d) {
			surf->fill_single = evg_565_fill_single;
			surf->fill_single_a = evg_565_fill_single_a;
		}
		break;
	case GF_PIXEL_RGB_444:
		if (use_const) {
			if (a!=0xFF) {
				surf->fill_spans = (EVG_SpanFunc) evg_444_fill_const_a;
			} else {
				surf->fill_spans = (EVG_SpanFunc) evg_444_fill_const;
			}
		} else {
			surf->fill_spans = (EVG_SpanFunc) evg_444_fill_var;
		}
		if (surf->ext3d) {
			surf->fill_single = evg_444_fill_single;
			surf->fill_single_a = evg_444_fill_single_a;
		}
		break;
	case GF_PIXEL_RGB_555:
		if (use_const) {
			if (a!=0xFF) {
				surf->fill_spans = (EVG_SpanFunc) evg_555_fill_const_a;
			} else {
				surf->fill_spans = (EVG_SpanFunc) evg_555_fill_const;
			}
		} else {
			surf->fill_spans = (EVG_SpanFunc) evg_555_fill_var;
		}
		if (surf->ext3d) {
			surf->fill_single = evg_555_fill_single;
			surf->fill_single_a = evg_555_fill_single_a;
		}
		break;
	case GF_PIXEL_YVU:
		surf->swap_uv = GF_TRUE;
	case GF_PIXEL_YUV:
		surf->yuv_type = EVG_YUV;
		if (use_const && !for_3d) {
			surf->yuv_flush_uv = evg_yuv420p_flush_uv_const;

			if (a!=0xFF) {
				surf->fill_spans = (EVG_SpanFunc) evg_yuv420p_fill_const_a;
			} else {
				surf->fill_spans = (EVG_SpanFunc) evg_yuv420p_fill_const;
			}
			uv_alpha_size=2*surf->width;
		} else {
			surf->yuv_flush_uv = evg_yuv420p_flush_uv_var;
			surf->fill_spans = (EVG_SpanFunc) evg_yuv420p_fill_var;
			uv_alpha_size = 2 * 3*surf->width;
			if (for_3d) {
				surf->sten = &surf->ext3d->yuv_sten;
				surf->direct_yuv_3d = GF_TRUE;
			}

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
				surf->fill_spans = (EVG_SpanFunc) evg_yuv420p_fill_const_a;
			} else {
				surf->fill_spans = (EVG_SpanFunc) evg_yuv420p_fill_const;
			}
			uv_alpha_size = 2*surf->width;
		} else {
			surf->yuv_flush_uv = evg_nv12_flush_uv_var;
			surf->fill_spans = (EVG_SpanFunc) evg_yuv420p_fill_var;
			uv_alpha_size = 2 * 3*surf->width;
		}
		break;
	case GF_PIXEL_YUV422:
		surf->yuv_type = EVG_YUV;
		surf->is_422 = GF_TRUE;
		if (use_const && !for_3d) {
			surf->yuv_flush_uv = evg_yuv422p_flush_uv_const;

			if (a!=0xFF) {
				surf->fill_spans = (EVG_SpanFunc) evg_yuv420p_fill_const_a;
			} else {
				surf->fill_spans = (EVG_SpanFunc) evg_yuv420p_fill_const;
			}
			uv_alpha_size = 2*surf->width;
		} else {
			surf->yuv_flush_uv = evg_yuv422p_flush_uv_var;
			surf->fill_spans = (EVG_SpanFunc) evg_yuv420p_fill_var;
			uv_alpha_size = 2 * 3*surf->width;
		}
		break;
	case GF_PIXEL_YUV444:
		surf->yuv_type = EVG_YUV_444;
		if (use_const) {
			if (a!=0xFF) {
				surf->fill_spans = (EVG_SpanFunc) evg_yuv444p_fill_const_a;
			} else {
				surf->fill_spans = (EVG_SpanFunc) evg_yuv444p_fill_const;
			}
		} else {
			surf->fill_spans = (EVG_SpanFunc) evg_yuv444p_fill_var;
		}
		break;
	case GF_PIXEL_YUYV:
	case GF_PIXEL_YVYU:
	case GF_PIXEL_UYVY:
	case GF_PIXEL_VYUY:
		surf->yuv_type = EVG_YUV;
		if (use_const && !for_3d) {
			if (a!=0xFF) {
				surf->fill_spans = (EVG_SpanFunc) evg_yuyv_fill_const_a;
			} else {
				surf->fill_spans = (EVG_SpanFunc) evg_yuyv_fill_const;
			}
			uv_alpha_size = 2*surf->width;
		} else {
			surf->fill_spans = (EVG_SpanFunc) evg_yuyv_fill_var;
			uv_alpha_size = 2 * 3*surf->width;
		}
		break;

	case GF_PIXEL_YUV_10:
		surf->yuv_type = EVG_YUV;
		if (use_const && !for_3d) {
			surf->yuv_flush_uv = evg_yuv420p_10_flush_uv_const;

			if (a!=0xFF) {
				surf->fill_spans = (EVG_SpanFunc) evg_yuv420p_10_fill_const_a;
			} else {
				surf->fill_spans = (EVG_SpanFunc) evg_yuv420p_10_fill_const;
			}
			uv_alpha_size = 4*surf->width;
		} else {
			surf->yuv_flush_uv = evg_yuv420p_10_flush_uv_var;
			surf->fill_spans = (EVG_SpanFunc) evg_yuv420p_10_fill_var;
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
				surf->fill_spans = (EVG_SpanFunc) evg_yuv420p_10_fill_const_a;
			} else {
				surf->fill_spans = (EVG_SpanFunc) evg_yuv420p_10_fill_const;
			}
			uv_alpha_size = 4*surf->width;
		} else {
			surf->yuv_flush_uv = evg_nv12_10_flush_uv_var;
			surf->fill_spans = (EVG_SpanFunc) evg_yuv420p_10_fill_var;
			uv_alpha_size = 4 * 3*surf->width;
		}
		break;
	case GF_PIXEL_YUV422_10:
		surf->yuv_type = EVG_YUV;
		surf->is_422 = GF_TRUE;
		if (use_const && !for_3d) {
			surf->yuv_flush_uv = evg_yuv422p_10_flush_uv_const;

			if (a!=0xFF) {
				surf->fill_spans = (EVG_SpanFunc) evg_yuv420p_10_fill_const_a;
			} else {
				surf->fill_spans = (EVG_SpanFunc) evg_yuv420p_10_fill_const;
			}
			uv_alpha_size = 4*surf->width;
		} else {
			surf->yuv_flush_uv = evg_yuv422p_10_flush_uv_var;
			surf->fill_spans = (EVG_SpanFunc) evg_yuv420p_10_fill_var;
			uv_alpha_size = 4 * 3*surf->width;
		}
		break;
	case GF_PIXEL_YUV444_10:
		surf->yuv_type = EVG_YUV_444;
		if (use_const) {
			if (a!=0xFF) {
				surf->fill_spans = (EVG_SpanFunc) evg_yuv444p_10_fill_const_a;
			} else {
				surf->fill_spans = (EVG_SpanFunc) evg_yuv444p_10_fill_const;
			}
		} else {
			surf->fill_spans = (EVG_SpanFunc) evg_yuv444p_10_fill_var;
		}
		break;
	default:
		return GF_FALSE;
	}

	if (uv_alpha_size) {
		u32 i;
		if (surf->uv_alpha_alloc < uv_alpha_size) {
			surf->uv_alpha_alloc = uv_alpha_size;
			surf->raster_ctx.uv_alpha = gf_realloc(surf->raster_ctx.uv_alpha, uv_alpha_size);
			if (!surf->raster_ctx.uv_alpha) return GF_FALSE;
			for (i=0;i<surf->nb_threads; i++) {
				EVGRasterCtx *rctx = &surf->th_raster_ctx[i];
				rctx->uv_alpha = gf_realloc(rctx->uv_alpha, uv_alpha_size);
				if (!rctx->uv_alpha) return GF_FALSE;
			}
		}
		memset(surf->raster_ctx.uv_alpha, 0, sizeof(char)*surf->uv_alpha_alloc);
		for (i=0;i<surf->nb_threads; i++) {
			EVGRasterCtx *rctx = &surf->th_raster_ctx[i];
			memset(rctx->uv_alpha, 0, sizeof(char)*surf->uv_alpha_alloc);
		}
	}
	if (surf->yuv_type && for_3d) {
		surf->sten = &surf->ext3d->yuv_sten;
		surf->direct_yuv_3d = GF_TRUE;
	}

	if (use_const && surf->yuv_type) {
		u8 y, cb, cr;
		gf_evg_rgb_to_yuv(surf, surf->fill_col, &y, &cb, &cr);
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
	surf->aa_level = 0;
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
	if (gp->flags & GF_PATH_FILL_ZERO_NONZERO)
		surf->ftoutline.flags |= GF_PATH_FILL_ZERO_NONZERO;
	if (gp->flags & GF_PATH_FILL_EVEN)
		surf->ftoutline.flags |= GF_PATH_FILL_EVEN;

	surf->ftoutline.n_points = gp->n_points;
	surf->ftoutline.points = gp->points;
	surf->mx = &surf->mat;
	return GF_OK;
}

static u32 shader_get_pix(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *rctx)
{
	rctx->frag_param.tx_x = x;
	rctx->frag_param.tx_y = _this->height-y;

	evg_get_fragment(rctx->surf, rctx, NULL);
	rctx->frag_param.screen_x += 1;
	return rctx->fill_col;
}

static u64 shader_get_pix_wide(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *rctx)
{
	rctx->frag_param.tx_x = x;
	rctx->frag_param.tx_y = _this->height-y;

	evg_get_fragment(rctx->surf, rctx, NULL);
	rctx->frag_param.screen_x += 1;
	return rctx->fill_col_wide;
}

static GF_Err gf_evg_setup_stencil(GF_EVGSurface *surf, GF_EVGStencil *sten, GF_Matrix2D *mat)
{
	GF_Rect rc;

	/*get path frame for texture convertion */
	if (sten->type != GF_STENCIL_SOLID) {
		rc = surf->path_bounds;
		if (surf->is_shader) {
			EVG_Texture *_tx = (EVG_Texture *)sten;
			_tx->width = (u32) rc.width;
			_tx->height = (u32) rc.height;
			_tx->cmat.identity = 1;
			_tx->alpha = 255;
			gf_mx2d_init(_tx->smat);
			gf_mx2d_add_translation(&_tx->smat, rc.width/2, rc.height/2);
			gf_mx2d_add_matrix(&_tx->smat, &surf->shader_mx);
//			_tx->fill_run = tex_fill_run;
			_tx->tx_get_pixel = shader_get_pix;
			_tx->tx_get_pixel_wide = shader_get_pix_wide;
			_tx->is_wide = 1;
			
			_tx->is_yuv = surf->yuv_type ? 1 : 0;
		}
		gf_mx2d_apply_rect(mat, &rc);
		rc.x = rc.y = 0;
		/*assign target frame and matrix*/
		sten->frame = rc;
		gf_mx2d_copy(sten->pmat, surf->mat);
		gf_mx2d_inverse(&sten->pmat);

		gf_mx2d_copy(sten->smat_bck, sten->smat);
		gf_mx2d_init(sten->smat);

		switch (sten->type) {
		case GF_STENCIL_TEXTURE:

			//in 3D mode, we only need the orginal stencil matrix
			if (!surf->is_3d_matrix) {
				if (!surf->is_shader) {
					EVG_Texture *texture = (EVG_Texture *) sten;
					if (!texture->tx_callback && ! texture->pixels)
						return GF_BAD_PARAM;

					if (texture->mod & GF_TEXTURE_FLIP_Y) {
						if (!surf->center_coords) gf_mx2d_add_scale(&sten->smat, FIX_ONE, -FIX_ONE);
					} else {
						if (surf->center_coords) gf_mx2d_add_scale(&sten->smat, FIX_ONE, -FIX_ONE);
					}
				}

				//add texture transform (normalized)
				gf_mx2d_add_matrix(&sten->smat, &sten->smat_bck);
				if (sten->auto_mx) {
					EVG_Texture *_tx = (EVG_Texture *)sten;
					//move translation to texture size
					sten->smat.m[2] *= _tx->width;
					sten->smat.m[5] *= _tx->height;
					//in auto mx, matrix is given inverted, as in OpenGL
					gf_mx2d_inverse(&sten->smat);

					//add texture -> untransformed path bounds matrix scale and translation
					gf_mx2d_add_scale(&sten->smat, surf->path_bounds.width/_tx->width, surf->path_bounds.height/_tx->height);
					gf_mx2d_add_translation(&sten->smat, -surf->path_bounds.width/2, surf->path_bounds.height/2);

					//add final path matrix
					gf_mx2d_add_matrix(&sten->smat, &surf->shader_mx);
				}

				//add surface matrix matrix
				gf_mx2d_add_matrix(&sten->smat, mat);
				gf_mx2d_inverse(&sten->smat);
			} else {
				if (!sten->auto_mx) {
					return GF_NOT_SUPPORTED;
				}
			}

			evg_texture_init(sten, surf);
			if (surf->is_shader) {
				EVG_Texture *_tx = (EVG_Texture *)sten;
				_tx->tx_get_pixel = shader_get_pix;
				_tx->tx_get_pixel_wide = shader_get_pix_wide;
			}
			break;
		case GF_STENCIL_LINEAR_GRADIENT:
		{
			EVG_LinearGradient *lin = (EVG_LinearGradient *)sten;
			gf_mx2d_copy(sten->smat, sten->smat_bck);
			if (!surf->is_3d_matrix) {
				if (sten->auto_mx) {
					//add [0,1] -> untransformed path bounds and path translation
					gf_mx2d_add_translation(&sten->smat, gf_divfix(surf->path_bounds.x, surf->path_bounds.width), gf_divfix(surf->path_bounds.y, surf->path_bounds.height));
					gf_mx2d_add_scale(&sten->smat, surf->path_bounds.width, surf->path_bounds.height);
					gf_mx2d_add_matrix(&sten->smat, &surf->shader_mx);
				}
				gf_mx2d_add_matrix(&sten->smat, mat);
			} else {
				if (!sten->auto_mx) {
					return GF_NOT_SUPPORTED;
				}
			}
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
			gf_mx2d_copy(sten->smat, sten->smat_bck);
			if (!surf->is_3d_matrix) {
				if (sten->auto_mx) {
					//add [0,1] -> untransformed path bounds and path translation
					gf_mx2d_add_translation(&sten->smat, gf_divfix(surf->path_bounds.x, surf->path_bounds.width), gf_divfix(surf->path_bounds.y, surf->path_bounds.height));
					gf_mx2d_add_scale(&sten->smat, surf->path_bounds.width, surf->path_bounds.height);
					gf_mx2d_add_matrix(&sten->smat, &surf->shader_mx);
				}
				gf_mx2d_add_matrix(&sten->smat, mat);
			} else {
				if (!sten->auto_mx) {
					return GF_NOT_SUPPORTED;
				}
			}
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
	} else {
		EVG_Brush *sc = (EVG_Brush *) sten;
		u32 col = sc->color;
		if (sc->alpha < 0xFF) {
			u32 ca = ((u32) (GF_COL_A(col) + 1) * sc->alpha) >> 8;
			col = ( ((ca<<24) & 0xFF000000) ) | (col & 0x00FFFFFF);
		}
		if (surf->yuv_type) {
			sc->fill_col = gf_evg_argb_to_ayuv(surf, col);
		} else {
			sc->fill_col = col;
		}
		if (surf->not_8bits)
			sc->fill_col_wide = evg_col_to_wide(sc->fill_col);
	}
	return GF_OK;
}

static void gf_evg_restore_stencil(GF_EVGSurface *surf, GF_EVGStencil *sten)
{
	/*restore stencil matrix*/
	if (sten->type != GF_STENCIL_SOLID) {
		gf_mx2d_copy(sten->smat, sten->smat_bck);
	}
}

GF_EXPORT
GF_Err gf_evg_surface_multi_fill(GF_EVGSurface *surf, GF_EVGMultiTextureMode operand, GF_EVGStencil *sten1, GF_EVGStencil *sten2, GF_EVGStencil *sten3, Float params[4])
{
	GF_Err e;
	Bool reset_aa = GF_FALSE;
	u32 max_gray;
	GF_Matrix2D mat;
	if (!surf || (!sten1 && !surf->frag_shader) ) return GF_BAD_PARAM;
	if (!surf->ftoutline.n_points) return GF_OK;
	surf->sten = sten1;
	surf->update_run = NULL;

	/*setup ft raster calllbacks*/
	if (!setup_grey_callback(surf, GF_FALSE, sten2 ? GF_TRUE : GF_FALSE)) return GF_OK;

	get_surface_world_matrix(surf, &mat);
	surf->is_shader = GF_FALSE;
	if (!sten1) {
		sten1 = (GF_EVGStencil *) &surf->shader_sten;
		sten1->type = GF_STENCIL_TEXTURE;
		surf->is_shader = GF_TRUE;
		surf->sten = sten1;
		operand = GF_EVG_OPERAND_NONE;
	}

	e = gf_evg_setup_stencil(surf, sten1, &mat);
	if (e) return e;

	if (operand) {
		e = gf_evg_setup_multi_texture(surf, operand, sten2, sten3, params);
		if (e) return e;
		if (sten2 && (sten2 != sten1)) {
			e = gf_evg_setup_stencil(surf, sten2, &mat);
			if (e) return e;
		}
		if (sten3 && (sten3 != sten1) && (sten3 != sten2)) {
			e = gf_evg_setup_stencil(surf, sten3, &mat);
			if (e) return e;
		}
	}

	if (!surf->aa_level) {
		//no AA
		if (surf->qlevel == GF_RASTER_HIGH_SPEED) surf->aa_level = 0;
		//skip pixles with less than 25% coverage
		else if (surf->qlevel == GF_RASTER_MID) surf->aa_level = 63;
		else surf->aa_level = 0xFF;
		reset_aa = GF_TRUE;
	}


	max_gray = surf->max_gray_spans;
	/*force complete line callback for YUV 420/422*/
	if (surf->yuv_type==EVG_YUV)
		surf->max_gray_spans = 0xFFFFFFFF;

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

	if (surf->mask_mode == GF_EVGMASK_DRAW) {
		u32 old_pitch_x = surf->pitch_x;
		u32 old_pitch_y = surf->pitch_y;
		u8 *old_pixels = surf->pixels;

		surf->pixels = surf->internal_mask;
		surf->pitch_x = 1;
		surf->pitch_y = surf->width;

		/*and call the raster*/
		if (surf->is_3d_matrix)
			e = evg_raster_render_path_3d(surf);
		else
			e = evg_raster_render(surf);

		surf->pitch_x = old_pitch_x;
		surf->pitch_y = old_pitch_y;
		surf->pixels = old_pixels;
	} else {
		/*and call the raster*/
		if (surf->is_3d_matrix)
			e = evg_raster_render_path_3d(surf);
		else
			e = evg_raster_render(surf);
	}

	if (reset_aa) surf->aa_level = 0;

	surf->max_gray_spans = max_gray;

	gf_evg_restore_stencil(surf, sten1);
	if (sten2) gf_evg_restore_stencil(surf, sten2);
	if (sten3) gf_evg_restore_stencil(surf, sten3);
	surf->sten = surf->sten2 = surf->sten3 = NULL;
	return e;
}

GF_EXPORT
GF_Err gf_evg_surface_fill(GF_EVGSurface *surf, GF_EVGStencil *sten)
{
	return gf_evg_surface_multi_fill(surf, 0, sten, NULL, NULL, NULL);
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
	if (!setup_grey_callback(surf, GF_TRUE, GF_FALSE)) return GF_OK;

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
	max_gray = surf->max_gray_spans;
	/*force complete line callback for YUV 420/422*/
	if (surf->yuv_type==EVG_YUV)
		surf->max_gray_spans = 0xFFFFFFFF;

	surf->aa_level = 0xFF;
	
	/*and call the raster*/
	e = evg_raster_render3d(surf, indices, nb_idx, vertices, nb_vertices, nb_comp, prim_type);
	surf->max_gray_spans =  max_gray;
	return e;
}

GF_Err evg_raster_render3d_path(GF_EVGSurface *surf, GF_Path *path, Float z);

GF_Err gf_evg_surface_draw_path(GF_EVGSurface *surf, GF_Path *path, Float z)
{
	GF_Err e;
	u32 max_gray;
	if (!surf || !surf->ext3d) return GF_BAD_PARAM;

	/*setup ft raster calllbacks*/
	if (!setup_grey_callback(surf, GF_TRUE, GF_FALSE)) return GF_OK;

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
	max_gray = surf->max_gray_spans;
	/*force complete line callback for YUV 420/422*/
	if (surf->yuv_type==EVG_YUV)
		surf->max_gray_spans = 0xFFFFFFFF;

	surf->aa_level = 0xFF;

	/*and call the raster*/
	e = evg_raster_render3d_path(surf, path, z);
	surf->max_gray_spans =  max_gray;
	return e;
}


GF_Err gf_evg_surface_set_mask_mode(GF_EVGSurface *surf, GF_EVGMaskMode mask_mode)
{
	if (!surf) return GF_BAD_PARAM;

	if ((mask_mode==GF_EVGMASK_DRAW)
		|| (mask_mode==GF_EVGMASK_DRAW_NO_CLEAR)
		|| (mask_mode==GF_EVGMASK_RECORD)
	) {
		u8 clear_val = 0;
		Bool clear_mask = GF_FALSE;
		if (!surf->internal_mask) {
			surf->internal_mask = gf_malloc(sizeof(u8) * surf->width * surf->height);
			if (!surf->internal_mask) return GF_OUT_OF_MEM;
			clear_mask = GF_TRUE;
		}
		if ((mask_mode==GF_EVGMASK_DRAW) && (surf->mask_mode != GF_EVGMASK_DRAW))
			clear_mask = GF_TRUE;

		if (mask_mode==GF_EVGMASK_RECORD) {
			clear_mask = GF_TRUE;
			clear_val = 0xFF;
		} else {
			mask_mode = GF_EVGMASK_DRAW;
		}

		if (clear_mask) {
			memset(surf->internal_mask, clear_val, sizeof(u8) * surf->width * surf->height);
		}
	}
	surf->mask_mode = mask_mode;
	return GF_OK;
}

GF_EVGMaskMode gf_evg_surface_get_mask_mode(GF_EVGSurface *surf)
{
	if (!surf) return GF_EVGMASK_NONE;
	return surf->mask_mode;
}
