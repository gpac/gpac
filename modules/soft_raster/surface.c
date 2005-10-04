/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
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

static void get_surface_world_matrix(EVGSurface *_this, GF_Matrix2D *mat)
{
	gf_mx2d_init(*mat);
	if (_this->center_coords) {
		gf_mx2d_add_scale(mat, FIX_ONE, -FIX_ONE);
		gf_mx2d_add_translation(mat, INT2FIX(_this->width / 2), INT2FIX(_this->height / 2));
	}
}

GF_SURFACE evg_surface_new(GF_Raster2D *_dr, Bool center_coords)
{
	EVGSurface *_this;
	GF_SAFEALLOC(_this, sizeof(EVGSurface));
	if (_this) {
		_this->center_coords = center_coords;
		_this->texture_filter = GF_TEXTURE_FILTER_DEFAULT;
		_this->ftparams.source = &_this->ftoutline;
		_this->ftparams.user = _this;
		_this->raster = evg_raster_new();
	}
	return _this;
}

void evg_surface_delete(GF_SURFACE _this)
{
	EVGSurface *surf = (EVGSurface *)_this;

	if (surf->contours) free(surf->contours);
	if (surf->tags) free(surf->tags);
	if (surf->points) free(surf->points);
	if (surf->stencil_pix_run) free(surf->stencil_pix_run);
	evg_raster_del(surf->raster);
	free(surf);
}

GF_Err evg_surface_set_matrix(GF_SURFACE _this, GF_Matrix2D *mat)
{
	GF_Matrix2D tmp;
	EVGSurface *surf = (EVGSurface *)_this;
	if (!surf) return GF_BAD_PARAM;
	get_surface_world_matrix(surf, &surf->mat);
	if (!mat) return GF_OK;
	gf_mx2d_init(tmp);
	gf_mx2d_add_matrix(&tmp, mat);
	gf_mx2d_add_matrix(&tmp, &surf->mat);
	gf_mx2d_copy(surf->mat, tmp);
	return GF_OK;
}


GF_Err evg_surface_attach_to_callbacks(GF_SURFACE _this, GF_RasterCallback *callbacks, u32 width, u32 height)
{
	EVGSurface *surf = (EVGSurface *)_this;
	if (!surf || !width || !height || !callbacks) return GF_BAD_PARAM;
	if (!callbacks->cbk || !callbacks->fill_run_alpha || !callbacks->fill_run_no_alpha) return GF_BAD_PARAM;

	surf->width = width;
	surf->height = height;
	if (surf->stencil_pix_run) free(surf->stencil_pix_run);
	GF_SAFEALLOC(surf->stencil_pix_run , sizeof(u32) * (width+2));

	surf->raster_cbk = callbacks->cbk;
	surf->raster_fill_run_alpha = callbacks->fill_run_alpha;
	surf->raster_fill_run_no_alpha = callbacks->fill_run_no_alpha;
	evg_surface_set_matrix(surf, NULL);
	return GF_OK;
}


GF_Err evg_surface_attach_to_buffer(GF_SURFACE _this, unsigned char *pixels, u32 width, u32 height, u32 stride, GF_PixelFormat pixelFormat)
{
	u32 BPP;
	EVGSurface *surf = (EVGSurface *)_this;
	if (!surf || !pixels || (pixelFormat>GF_PIXEL_YUVA)) return GF_BAD_PARAM;

	switch (pixelFormat) {
	case GF_PIXEL_RGB_555:
	case GF_PIXEL_RGB_565:
		BPP = 2;
		break;
	case GF_PIXEL_BGR_24:
	case GF_PIXEL_RGB_24:
		BPP = 3;
		break;
	case GF_PIXEL_RGB_32:
	case GF_PIXEL_ARGB:
		BPP = 4;
		break;
	case GF_PIXEL_BGR_32:
	default:
		return GF_NOT_SUPPORTED;
	}
	surf->stride = stride;
	surf->width = width;
	if (surf->stencil_pix_run) free(surf->stencil_pix_run);
	GF_SAFEALLOC(surf->stencil_pix_run , sizeof(u32) * (width+2));
	surf->height = height;
	surf->pixels = pixels;
	surf->pixelFormat = pixelFormat;
	surf->BPP = BPP;

	surf->raster_cbk = NULL;
	surf->raster_fill_run_alpha = NULL;
	surf->raster_fill_run_no_alpha = NULL;
	evg_surface_set_matrix(_this, NULL);
	return GF_OK;
}


GF_Err evg_surface_attach_to_texture(GF_SURFACE _this, GF_STENCIL sten)
{
	u32 BPP;
	EVGSurface *surf = (EVGSurface *)_this;
	EVG_Texture *tx = (EVG_Texture *) sten;;
	if (!surf || (tx->type != GF_STENCIL_TEXTURE)) return GF_BAD_PARAM;

	switch (tx->pixel_format) {
	case GF_PIXEL_GREYSCALE:
		BPP = 1;
		break;
	case GF_PIXEL_ALPHAGREY:
	case GF_PIXEL_RGB_555:
	case GF_PIXEL_RGB_565:
		BPP = 2;
		break;
	case GF_PIXEL_BGR_24:
	case GF_PIXEL_RGB_24:
		BPP = 3;
		break;
	case GF_PIXEL_RGB_32:
	case GF_PIXEL_ARGB:
		BPP = 4;
		break;
	default:
		return GF_NOT_SUPPORTED;
	}
	surf->stride = tx->stride;
	if (surf->stencil_pix_run) free(surf->stencil_pix_run);
	GF_SAFEALLOC(surf->stencil_pix_run , sizeof(u32) * (tx->width+2));

	surf->width = tx->width;
	surf->height = tx->height;
	surf->pixels = tx->pixels;
	surf->pixelFormat = tx->pixel_format;
	surf->BPP = BPP;
	surf->raster_cbk = NULL;
	surf->raster_fill_run_alpha = NULL;
	surf->raster_fill_run_no_alpha = NULL;
	evg_surface_set_matrix(surf, NULL);
	return GF_OK;
}

void evg_surface_detach(GF_SURFACE _this)
{
	EVGSurface *surf = (EVGSurface *)_this;
	surf->raster_cbk = NULL;
	surf->raster_fill_run_alpha = NULL;
	surf->raster_fill_run_no_alpha = NULL;
}

GF_Err evg_surface_clear(GF_SURFACE _this, GF_IRect *rc, u32 color)
{
	GF_IRect clear;
	EVGSurface *surf = (EVGSurface *)_this;
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

		clear.width = (u32) rc->width;
		if (_x>=0) {
			clear.x = (u32) _x;
		} else {
			if ( (s32) clear.width + _x < 0) return GF_BAD_PARAM;
			clear.width += _x;
			clear.x = 0;
		}
		clear.height = (u32) rc->height;
		if (_y>=0) {
			clear.y = _y;
		} else {
			if ( (s32) clear.height + _y < 0) return GF_BAD_PARAM;
			clear.height += _y;
			clear.y = 0;
		}
	} else {
		clear.x = clear.y = 0;
		clear.width = surf->width;
		clear.height = surf->height;
	}
	
	if (surf->raster_cbk) {
		return evg_surface_clear_user(surf, clear, color);
	}

	switch (surf->pixelFormat) {
	case GF_PIXEL_ARGB:
	case GF_PIXEL_RGB_32:
		return evg_surface_clear_argb(surf, clear, color);
	case GF_PIXEL_BGR_24:
		return evg_surface_clear_rgb(surf, clear, color);
	case GF_PIXEL_RGB_24:
		return evg_surface_clear_bgr(surf, clear, color);
	case GF_PIXEL_RGB_565:
		return evg_surface_clear_565(surf, clear, color);
	case GF_PIXEL_RGB_555:
		return evg_surface_clear_555(surf, clear, color);
	default:
		return GF_BAD_PARAM;
	}
}

GF_Err evg_surface_set_raster_level(GF_SURFACE _this, GF_RasterLevel RasterSetting)
{
	EVGSurface *surf = (EVGSurface *)_this;
	if (!surf) return GF_BAD_PARAM;
	switch (RasterSetting) {
	case GF_RASTER_HIGH_QUALITY:
		surf->AALevel = 1;/*don't draw pixels with 0 alpha...*/
		surf->texture_filter = GF_TEXTURE_FILTER_HIGH_QUALITY;
		break;
	case GF_RASTER_MID:
		surf->AALevel = 90;
		surf->texture_filter = GF_TEXTURE_FILTER_HIGH_QUALITY;
		break;
	case GF_RASTER_HIGH_SPEED:
	default:
		surf->AALevel = 180;
		surf->texture_filter = GF_TEXTURE_FILTER_HIGH_SPEED;
		break;
	}
	return GF_OK;
}


GF_Err evg_surface_set_clipper(GF_SURFACE _this , GF_IRect *rc)
{
	EVGSurface *surf = (EVGSurface *)_this;
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


static GF_Err evg_resizecontours(EVGSurface *surf, u32 num)
{
	if (surf->contourlen < num) {
		if (surf->contours != NULL) free(surf->contours);
		surf->contours = malloc(sizeof (s16) * num);
		if (surf->contours == NULL) {
			surf->contourlen = 0;
			return GF_OUT_OF_MEM;
		}
		surf->contourlen = num;
	}
	return GF_OK;
}

static GF_Err evg_resizepoints(EVGSurface *surf, u32 num)
{
	if (surf->pointlen < num) {
		if (surf->points != NULL) free(surf->points);
		surf->points = NULL;

		if (surf->tags != NULL) free(surf->tags);
		surf->tags = NULL;

		surf->points = malloc(sizeof (EVG_Vector) * num);
		if (surf->points == NULL) {
			surf->pointlen = 0;
			return GF_OUT_OF_MEM;
		}
		surf->tags = malloc(sizeof(s8) * num);
		if (surf->tags == NULL) {
			surf->pointlen = 0;
			return GF_OUT_OF_MEM;
		}
		surf->pointlen = num;
	}
	return GF_OK;
}


static Bool setup_ft_callbacks(EVGSurface *surf)
{
	u32 col, a;
	Bool use_const = 1;

	if (surf->sten->type == GF_STENCIL_SOLID) {
		col = surf->fill_col = ((EVG_Brush *)surf->sten)->color;
		a = GF_COL_A(surf->fill_col);
	} else {
		col = a = 0;
		use_const = 0;
	}
	
	if (surf->raster_cbk) {
		if (use_const) {
			if (!a) return 0;
			if (a!=0xFF) {
				surf->ftparams.gray_spans = (EVG_Raster_Span_Func) evg_user_fill_const_a;
			} else {
				surf->ftparams.gray_spans = (EVG_Raster_Span_Func) evg_user_fill_const;
			}
		} else {
			surf->ftparams.gray_spans = (EVG_Raster_Span_Func) evg_user_fill_var;
		}
		return 1;
	}

	switch (surf->pixelFormat) {
	case GF_PIXEL_ARGB:
		if (use_const) {
			if (!a) return 0;
			if (a!=0xFF) {
				surf->ftparams.gray_spans = (EVG_Raster_Span_Func) evg_argb_fill_const_a;
			} else {
				surf->ftparams.gray_spans = (EVG_Raster_Span_Func) evg_argb_fill_const;
			}
		} else {
			surf->ftparams.gray_spans = (EVG_Raster_Span_Func) evg_argb_fill_var;
		}
		break;

	case GF_PIXEL_RGB_32:
		if (use_const) {
			if (!a) return 0;
			if (a!=0xFF) {
				surf->ftparams.gray_spans = (EVG_Raster_Span_Func) evg_rgb32_fill_const_a;
			} else {
				surf->ftparams.gray_spans = (EVG_Raster_Span_Func) evg_rgb32_fill_const;
			}
		} else {
			surf->ftparams.gray_spans = (EVG_Raster_Span_Func) evg_rgb32_fill_var;
		}
		break;
	case GF_PIXEL_RGB_24:
		if (use_const) {
			if (!a) return 0;
			if (a!=0xFF) {
				surf->ftparams.gray_spans = (EVG_Raster_Span_Func) evg_rgb_fill_const_a;
			} else {
				surf->ftparams.gray_spans = (EVG_Raster_Span_Func) evg_rgb_fill_const;
			}
		} else {
			surf->ftparams.gray_spans = (EVG_Raster_Span_Func) evg_rgb_fill_var;
		}
		break;
	case GF_PIXEL_BGR_24:
		if (use_const) {
			if (!a) return 0;
			if (a!=0xFF) {
				surf->ftparams.gray_spans = (EVG_Raster_Span_Func) evg_bgr_fill_const_a;
			} else {
				surf->ftparams.gray_spans = (EVG_Raster_Span_Func) evg_bgr_fill_const;
			}
		} else {
			surf->ftparams.gray_spans = (EVG_Raster_Span_Func) evg_bgr_fill_var;
		}
		break;
	case GF_PIXEL_RGB_565:
		if (use_const) {
			surf->fill_565 = GF_COL_TO_565(col);
			if (!a) return 0;
			if (a!=0xFF) {
				surf->ftparams.gray_spans = (EVG_Raster_Span_Func) evg_565_fill_const_a;
			} else {
				surf->ftparams.gray_spans = (EVG_Raster_Span_Func) evg_565_fill_const;
			}
		} else {
			surf->ftparams.gray_spans = (EVG_Raster_Span_Func) evg_565_fill_var;
		}
		break;
	case GF_PIXEL_RGB_555:
		if (use_const) {
			surf->fill_555 = GF_COL_TO_555(col);
			if (!a) return 0;
			if (a!=0xFF) {
				surf->ftparams.gray_spans = (EVG_Raster_Span_Func) evg_555_fill_const_a;
			} else {
				surf->ftparams.gray_spans = (EVG_Raster_Span_Func) evg_555_fill_const;
			}
		} else {
			surf->ftparams.gray_spans = (EVG_Raster_Span_Func) evg_555_fill_var;
		}
		break;
	}
	return 1;
}

GF_Err evg_surface_set_path(GF_SURFACE _this, GF_Path *gp)
{
	u32 n, i;
	GF_Err e;
	Bool is_identity;
	GF_Point2D pt;
	EVGSurface *surf = (EVGSurface *)_this;

	if (!surf) return GF_BAD_PARAM;
	if (!gp || !gp->n_points) {
		surf->ftoutline.n_points = 0;
		surf->ftoutline.n_contours = 0;
		return GF_OK;
	}

	gf_path_flatten(gp);
	n=0;

	if (gp->n_points > 32767) return GF_OUT_OF_MEM;

	surf->ftoutline.n_points = gp->n_points;
	surf->ftoutline.n_contours = gp->n_contours;

	e = evg_resizepoints(surf, gp->n_points);
	if (e) return e;
	e = evg_resizecontours(surf, gp->n_contours);
	if (e) return e;

	surf->ftoutline.points = surf->points;
	surf->ftoutline.tags = surf->tags;
	surf->ftoutline.contours = surf->contours;

	n = 0;

	for (i=0; i<gp->n_contours; i++) surf->contours[i] = gp->contours[i];

	is_identity = gf_mx2d_is_identity(surf->mat);
	gf_path_get_bounds(gp, &surf->path_bounds);

	/*invert Y (ft uses min Y)*/
	surf->path_bounds.y -= surf->path_bounds.height;

	if (is_identity) {
		for (i=0; i<gp->n_points; i++) {
			pt = gp->points[i];
#ifdef GPAC_FIXED_POINT
			surf->points[i].x = pt.x;
			surf->points[i].y = pt.y;
#else
			/*move to 16.16 representation*/
			surf->points[i].x = (u32) (pt.x * 0x10000L);
			surf->points[i].y = (u32) (pt.y * 0x10000L);
#endif
			surf->tags[i] = gp->tags[i];
		}
	} else {
		for (i=0; i<gp->n_points; i++) {
			pt = gp->points[i];
			gf_mx2d_apply_point(&surf->mat, &pt);
#ifdef GPAC_FIXED_POINT
			surf->points[i].x = pt.x;
			surf->points[i].y = pt.y;
#else
			/*move to 16.16 representation*/
			surf->points[i].x = (u32) (pt.x * 0x10000L);
			surf->points[i].y = (u32) (pt.y * 0x10000L);
#endif
			surf->tags[i] = gp->tags[i];
		}
	}

	surf->ftoutline.flags = 0;
	if (gp->flags & GF_PATH_FILL_ZERO_NONZERO) surf->ftoutline.flags = GF_PATH_FILL_ZERO_NONZERO;
	return GF_OK;
}


GF_Err evg_surface_fill(GF_SURFACE _this, GF_STENCIL stencil)
{
	GF_Rect rc;
	GF_Matrix2D mat, st_mat;
	Bool restore_filter;
	EVGSurface *surf = (EVGSurface *)_this;
	EVGStencil *sten = (EVGStencil *)stencil;
	if (!surf || !stencil) return GF_BAD_PARAM;
	if (!surf->ftoutline.n_points) return GF_OK;
	surf->sten = sten;

	/*setup ft raster calllbacks*/
	if (!setup_ft_callbacks(surf)) return GF_OK;

	get_surface_world_matrix(surf, &mat);

	restore_filter = 0;
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
			if (! ((EVG_Texture *)sten)->pixels) return GF_BAD_PARAM;
			if (surf->center_coords) gf_mx2d_add_scale(&sten->smat, FIX_ONE, -FIX_ONE);
			evg_set_texture_active(sten);
			gf_mx2d_add_matrix(&sten->smat, &st_mat);
			gf_mx2d_add_matrix(&sten->smat, &mat);
			gf_mx2d_inverse(&sten->smat);
			evg_bmp_init(sten);
			if (((EVG_Texture *)sten)->filter == GF_TEXTURE_FILTER_DEFAULT) {
				restore_filter = 1;
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
			gf_mx2d_add_scale(&sten->smat, INT2FIX(1<<EVGGRADIENTSCALEBITS), INT2FIX(1<<EVGGRADIENTSCALEBITS));

		}
			break;
		case GF_STENCIL_RADIAL_GRADIENT:
		{
			EVG_RadialGradient *rad = (EVG_RadialGradient*)sten;
			gf_mx2d_copy(sten->smat, st_mat);
			gf_mx2d_add_matrix(&sten->smat, &mat);
			gf_mx2d_inverse(&sten->smat);
			gf_mx2d_add_translation(&sten->smat, -rad->center.x, -rad->center.y);
			gf_mx2d_add_scale(&sten->smat, gf_divfix(FIX_ONE, rad->radius.x), gf_divfix(FIX_ONE, rad->radius.y));
			rad->d_f.x = gf_divfix(rad->focus.x - rad->center.x, rad->radius.x);
			rad->d_f.y = gf_divfix(rad->focus.y - rad->center.y, rad->radius.y);
			/*init*/
			evg_radial_init(rad);
		}
			break;
		}
	}

	if (surf->useClipper) {
		surf->ftparams.clip_box.xMin = surf->clipper.x;
		surf->ftparams.clip_box.yMin = surf->clipper.y;
		surf->ftparams.clip_box.xMax = (surf->clipper.x + surf->clipper.width);
		surf->ftparams.clip_box.yMax = (surf->clipper.y + surf->clipper.height);
	} else {
		surf->ftparams.clip_box.xMin = 0;
		surf->ftparams.clip_box.yMin = 0;
		surf->ftparams.clip_box.xMax = (surf->width);
		surf->ftparams.clip_box.yMax = (surf->height);
	}

	/*and call the raster*/
	evg_raster_render(surf->raster, &surf->ftparams);

	/*restore stencil matrix*/
	if (sten->type != GF_STENCIL_SOLID) {
		gf_mx2d_copy(sten->smat, st_mat);
		if (restore_filter) ((EVG_Texture *)sten)->filter = GF_TEXTURE_FILTER_DEFAULT;
	}
	surf->sten = 0L;
	return GF_OK;
}



