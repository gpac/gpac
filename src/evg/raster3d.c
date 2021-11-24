/*
*			GPAC - Multimedia Framework C SDK
*
*			Authors: Jean Le Feuvre
*			Copyright (c) Telecom ParisTech 2019-2021
*					All rights reserved
*
*  This file is part of GPAC / software 3D rasterizer module
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



#if defined(WIN32) && !defined(__GNUC__)
# include <intrin.h>
# define GPAC_HAS_SSE2
#else
# ifdef __SSE2__
#  include <emmintrin.h>
#  define GPAC_HAS_SSE2
# endif
#endif

#ifdef GPAC_HAS_SSE2

static Float float_clamp(Float val, Float minval, Float maxval)
{
    _mm_store_ss( &val, _mm_min_ss( _mm_max_ss(_mm_set_ss(val),_mm_set_ss(minval)), _mm_set_ss(maxval) ) );
    return val;
}
#else

#define float_clamp(_val, _minval, _maxval)\
	(_val<_minval) ? _minval : (_val>_maxval) ? _maxval : _val;

#endif


static float edgeFunction(const GF_Vec4 *a, const GF_Vec4 *b, const GF_Vec4 *c)
{
	return (c->x - a->x) * (b->y - a->y) - (c->y - a->y) * (b->x - a->x);
}

float edgeFunction_pre(const GF_Vec4 *a, const Float b_minus_a_x, const Float b_minus_a_y, const GF_Vec4 *c)
{
	return (c->x - a->x) * (b_minus_a_y) - (c->y - a->y) * (b_minus_a_x);
}

static GFINLINE Bool evg3d_persp_divide(GF_Vec4 *pt)
{
	//perspective divide
	if (!pt->q) return GF_FALSE;
	pt->q = gf_divfix(FIX_ONE, pt->q);
	pt->x = gf_mulfix(pt->x, pt->q);
	pt->y = gf_mulfix(pt->y, pt->q);
	pt->z = gf_mulfix(pt->z, pt->q);
	return GF_TRUE;
}

static GFINLINE void evg_ndc_to_raster(GF_EVGSurface *surf, GF_Vec4 *pt, TPos *x, TPos *y)
{
	if (!surf->vp_w || !surf->vp_h) {
		surf->vp_w = surf->width;
		surf->vp_h = surf->height;
		surf->vp_x = surf->vp_y = 0;
	}
	//from [-1, 1] to [0,2] to [0,1] to [0,vp_width] to [vp left, vp right]
	pt->x = (pt->x + FIX_ONE) / 2 * surf->vp_w + surf->vp_x;
	//idem but flip Y
	pt->y = (FIX_ONE - (pt->y + FIX_ONE)/2) * surf->vp_h + surf->vp_y;
	/*compute depth*/
	if (!surf->ext3d || !surf->ext3d->clip_zero) {
		//move Z from [-1, 1] to [min_depth, max_depth]
		pt->z += FIX_ONE;
		pt->z /= 2;
	}

	if (surf->ext3d)
		pt->z = surf->ext3d->min_depth + pt->z * surf->ext3d->depth_range;

#ifdef GPAC_FIXED_POINT
	*x = UPSCALE(pt->x);
	*y = UPSCALE(pt->y);
#else
	*x = (s32) (pt->x * ONE_PIXEL);
	*y = (s32) (pt->y * ONE_PIXEL);
#endif
}

static GFINLINE int gray3d_move_to(GF_EVGSurface *surf, TPos x, TPos y)
{
	TCoord  ex, ey;

	/* record current cell, if any */
	gray_record_cell(surf);

	ex = TRUNC(x);
	ey = TRUNC(y);
	if ( ex < surf->min_ex ) ex = (TCoord)(surf->min_ex - 1);
	surf->area    = 0;
	surf->cover   = 0;
	gray_set_cell(surf, ex, ey );
	if (ey<0) ey=0;
	surf->last_ey = SUBPIXELS( ey );

	surf->x = x;
	surf->y = y;
	return 0;
}

GF_Err evg_raster_render_path_3d(GF_EVGSurface *surf)
{
	EVG_Vector   v_start;
	int   n;         /* index of contour in outline     */
	int   first;     /* index of first point in contour */
	TPos _x, _y, _sx, _sy;
	GF_Vec dir;
	u32 size_y;
	EVG_Outline *outline = &surf->ftoutline;

	/* Set up state in the raster object */
	if (surf->vp_w && surf->vp_h) {
		if ((u32) surf->clip_xMin < surf->vp_x)
			surf->clip_xMin = surf->vp_x;
		if ((u32) surf->clip_yMin < surf->vp_y)
			surf->clip_yMin = surf->vp_y;
		if ((u32) surf->clip_xMax > surf->vp_x + surf->vp_w)
			surf->clip_xMax = surf->vp_x + surf->vp_w;
		if ((u32) surf->clip_yMax > surf->vp_y + surf->vp_h)
			surf->clip_yMax = surf->vp_y + surf->vp_h;
	}
	surf->min_ex = surf->clip_xMin;
	surf->min_ey = surf->clip_yMin;
	surf->max_ex = surf->clip_xMax;
	surf->max_ey = surf->clip_yMax;

	size_y = (u32) (surf->max_ey - surf->min_ey);
	if ((u32) surf->max_lines < size_y) {
		surf->scanlines = (AAScanline*)gf_realloc(surf->scanlines, sizeof(AAScanline)*size_y);
		memset(&surf->scanlines[surf->max_lines], 0, sizeof(AAScanline)*(size_y - surf->max_lines) );
		surf->max_lines = size_y;
	}

	surf->ex = (int) (surf->max_ex+1);
	surf->ey = (int) (surf->max_ey+1);
	surf->cover = 0;
	surf->area = 0;
	surf->first_scanline = surf->max_ey;

	//raster coordinates of points
	TPos _x1, _y1, _x2, _y2, _x3, _y3;
	/*vertices in raster/window space: x and y are in pixel space, z is [min_depth, max_depth]*/
	GF_Vec4 s_pt1, s_pt2, s_pt3;

	/*get bounds, and compute interpolation from (top-left, top-right, bottom-right) vertices
	they are only used for tx coord interpolation, using {0,0}, {1,0} and {0,1}
	so that we flip y*/
	GF_Rect rc = surf->path_bounds;
	Float z = 0;
	//rc.y is bottom
	rc.y += rc.height;
	s_pt1.x = rc.x; s_pt1.y = rc.y; s_pt1.z = z; s_pt1.q = 1;
	s_pt2.x = rc.x + rc.width; s_pt2.y = rc.y; s_pt2.z = z; s_pt2.q = 1;
	s_pt3.x = rc.x; s_pt3.y = rc.y - rc.height ; s_pt3.z = z; s_pt3.q = 1;

	gf_mx_apply_vec_4x4(&surf->mx3d, &s_pt1);
	evg3d_persp_divide(&s_pt1);
	evg_ndc_to_raster(surf, &s_pt1, &_x1, &_y1);
	gf_mx_apply_vec_4x4(&surf->mx3d, &s_pt2);
	evg3d_persp_divide(&s_pt2);
	evg_ndc_to_raster(surf, &s_pt2, &_x2, &_y2);
	gf_mx_apply_vec_4x4(&surf->mx3d, &s_pt3);
	evg3d_persp_divide(&s_pt3);
	evg_ndc_to_raster(surf, &s_pt3, &_x3, &_y3);

	//precompute triangle
	surf->tri_area = edgeFunction(&s_pt1, &s_pt2, &s_pt3);
	surf->s_v1 = s_pt1;
	surf->s_v2 = s_pt2;
	surf->s_v3 = s_pt3;
	surf->s3_m_s2_x = surf->s_v3.x - surf->s_v2.x;
	surf->s3_m_s2_y = surf->s_v3.y - surf->s_v2.y;
	surf->s1_m_s3_x = surf->s_v1.x - surf->s_v3.x;
	surf->s1_m_s3_y = surf->s_v1.y - surf->s_v3.y;
	surf->s2_m_s1_x = surf->s_v2.x - surf->s_v1.x;
	surf->s2_m_s1_y = surf->s_v2.y - surf->s_v1.y;

	dir.x = dir.y = 0;
	dir.z = -FIX_ONE;

	first = 0;
	for ( n = 0; n < outline->n_contours; n++ ) {
		GF_Vec4 pt;
		EVG_Vector *point;
		EVG_Vector *limit;
		int  last;  /* index of last point in contour */
		last  = outline->contours[n];
		limit = outline->points + last;
		v_start = outline->points[first];
		point = outline->points + first;

		pt.x = v_start.x;
		pt.y = v_start.y;
		pt.z = 0;
		pt.q = 1;
		gf_mx_apply_vec_4x4(&surf->mx3d, &pt);
		if (!evg3d_persp_divide(&pt)) {
			continue;
		}
		evg_ndc_to_raster(surf, &pt, &_sx, &_sy);
		gray3d_move_to(surf, _sx, _sy);
		while ( point < limit ) {
			point++;

			pt.x = point->x;
			pt.y = point->y;
			pt.z = 0;
			pt.q = 1;
			gf_mx_apply_vec_4x4(&surf->mx3d, &pt);
			if (!evg3d_persp_divide(&pt)) {
				break;
			}
			evg_ndc_to_raster(surf, &pt, &_x, &_y);
			gray_render_line(surf, _x, _y);
		}
		gray_render_line(surf, _sx, _sy);

		first = last + 1;
	}

	gray_record_cell( surf );

	surf->render_span = (EVG_SpanFunc) surf->fill_spans;
	return evg_sweep_lines(surf, size_y, GF_TRUE, GF_FALSE, NULL);
}

void evg_get_fragment(GF_EVGSurface *surf, EVGRasterCtx *rctx, Bool *is_transparent)
{
	if (!surf->frag_shader(surf->frag_shader_udta, &rctx->frag_param)) {
		rctx->frag_param.frag_valid = 0;
		rctx->fill_col = 0;
		rctx->fill_col_wide = 0;
		return;
	}

#if 0
	frag_param->color.x = float_clamp(frag_param->color.x, 0.0, 1.0);
	frag_param->color.y = float_clamp(frag_param->color.y, 0.0, 1.0);
	frag_param->color.z = float_clamp(frag_param->color.z, 0.0, 1.0);
	frag_param->color.q = float_clamp(frag_param->color.q, 0.0, 1.0);
#endif

	if (surf->not_8bits) {
		if ((rctx->frag_param.frag_valid==GF_EVG_FRAG_RGB_PACK) || (rctx->frag_param.frag_valid==GF_EVG_FRAG_YUV_PACK)) {
			if (rctx->frag_param.color_pack) {
				u16 a = GF_COL_A(rctx->frag_param.color_pack); a *= 255;
				u16 r = GF_COL_R(rctx->frag_param.color_pack); r *= 255;
				u16 g = GF_COL_G(rctx->frag_param.color_pack); g *= 255;
				u16 b = GF_COL_B(rctx->frag_param.color_pack); b *= 255;
				rctx->fill_col_wide = GF_COLW_ARGB(a, r, g, b);
			} else {
				rctx->fill_col_wide = rctx->frag_param.color_pack_wide;
			}
		} else {
			GF_Vec4 color = rctx->frag_param.color;
			rctx->fill_col_wide = GF_COLW_ARGB((u16) color.q*0xFFFF, (u16) color.x*0xFFFF, (u16) color.y*0xFFFF, (u16) color.z*0xFFFF);
		}
		if (is_transparent && (GF_COLW_A(rctx->fill_col_wide) <0xFFFF))
			*is_transparent=1;
	} else {
		if ((rctx->frag_param.frag_valid==GF_EVG_FRAG_RGB_PACK) || (rctx->frag_param.frag_valid==GF_EVG_FRAG_YUV_PACK)) {
			if (rctx->frag_param.color_pack_wide) {
				u16 a = GF_COLW_A(rctx->frag_param.color_pack_wide); a /= 255;
				u16 r = GF_COLW_R(rctx->frag_param.color_pack_wide); r /= 255;
				u16 g = GF_COLW_G(rctx->frag_param.color_pack_wide); g /= 255;
				u16 b = GF_COLW_B(rctx->frag_param.color_pack_wide); b /= 255;
				rctx->fill_col = GF_COL_ARGB(a, r, g, b);
			} else {
				rctx->fill_col = rctx->frag_param.color_pack;
			}
		} else {
			GF_Vec4 color = rctx->frag_param.color;
			u8 a = (u8) (color.q*255);
			u8 r = (u8) (color.x*255);
			u8 g = (u8) (color.y*255);
			u8 b = (u8) (color.z*255);
			rctx->fill_col = GF_COL_ARGB(a, r, g, b);
		}
		if (is_transparent && (GF_COL_A(rctx->fill_col) <0xFF))
			*is_transparent=1;
	}

	if (surf->not_8bits) {
		if (surf->yuv_type) {
			if (rctx->frag_param.frag_valid==GF_EVG_FRAG_RGB) {
				rctx->fill_col_wide = gf_evg_argb_to_ayuv_wide(surf, rctx->fill_col_wide);
			}
		} else {
			if (rctx->frag_param.frag_valid==GF_EVG_FRAG_YUV) {
				rctx->fill_col_wide = gf_evg_ayuv_to_argb_wide(surf, rctx->fill_col_wide);
			}
		}
	} else {
		if (surf->yuv_type) {
			/*RGB frag*/
			if (rctx->frag_param.frag_valid==GF_EVG_FRAG_RGB) {
				rctx->fill_col = gf_evg_argb_to_ayuv(surf, rctx->fill_col);
			}
		} else {
			if (rctx->frag_param.frag_valid==GF_EVG_FRAG_YUV) {
				rctx->fill_col = gf_evg_ayuv_to_argb(surf, rctx->fill_col);
			}
		}
	}
	return;
}


static void push_patch_pixel(AAScanline *sl, s32 x, u32 col, u8 coverage, Float depth, Float write_depth, u32 idx1, u32 idx2)
{
	u32 i;
	PatchPixel *pp;
	for (i=0; i<sl->pnum; i++) {
		if (sl->pixels[i].x>x) break;
		if (sl->pixels[i].x<x) continue;
		sl->pixels[i].color = col;
		sl->pixels[i].cover = coverage;
		sl->pixels[i].depth = depth;
		sl->pixels[i].write_depth = write_depth;
		sl->pixels[i].idx1 = idx1;
		sl->pixels[i].idx2 = idx2;
		return;
	}
	if (coverage==0xFF) return;

	if (sl->pnum == sl->palloc) {
		sl->palloc += AA_CELL_STEP_ALLOC;
		sl->pixels = gf_realloc(sl->pixels, sizeof(PatchPixel) * sl->palloc);
	}
	if (i==sl->pnum) {
		pp = &sl->pixels[sl->pnum];
	} else {
		memmove(&sl->pixels[i+1], &sl->pixels[i], sizeof(PatchPixel)*(sl->pnum - i));
		pp = &sl->pixels[i];
	}
	sl->pnum++;
	pp->color = col;
	pp->cover = coverage;
	pp->x = x;
	pp->depth = depth;
	pp->write_depth = write_depth;
	pp->idx1 = idx1;
	pp->idx2 = idx2;
}

static PatchPixel *get_patch_pixel(AAScanline *sl, s32 x)
{
	u32 i;
	for (i=0; i<sl->pnum; i++) {
		if (sl->pixels[i].x>x) break;
		if (sl->pixels[i].x<x) continue;
		return &sl->pixels[i];
	}
	return NULL;
}


static void remove_patch_pixel(AAScanline *sl, s32 x)
{
	u32 i;
	for (i=0; i<sl->pnum; i++) {
		if (sl->pixels[i].x>x) break;
		if (sl->pixels[i].x<x) continue;
		if (i+1<sl->pnum)
			memmove(&sl->pixels[i], &sl->pixels[i+1], sizeof(PatchPixel)*(sl->pnum-i-1));
		sl->pnum--;
		return;
	}
}

void EVG3D_SpanFunc(int y, int count, EVG_Span *spans, GF_EVGSurface *surf, EVGRasterCtx *rctx)
{
	int i;
	GF_Vec4 pix;
	EVG_Surface3DExt *s3d = surf->ext3d;
	AAScanline *sl = &surf->scanlines[y];
	Float *depth_line = s3d->depth_buffer ? &s3d->depth_buffer[y*surf->width] : NULL;
	Float depth_buf_val;

	pix.z=0;
	pix.q=1;

	for (i=0; i<count; i++) {
		u8 coverage = spans[i].coverage;
		u32 j, len = spans[i].len;
		s32 sx = spans[i].x;
		for (j=0; j<len; j++) {


	Bool transparent=GF_FALSE;
	Float depth, bc1, bc2, bc3;
	Bool full_cover=GF_TRUE;
	Bool edge_merge=GF_FALSE;
	s32 x = sx + j;
	PatchPixel *prev_partial = NULL;

	/*no AA, force full opacity and test pixel/line below*/
	if (s3d->disable_aa)
		coverage = 0xFF;
	else if (!s3d->mode2d)
		prev_partial = get_patch_pixel(sl, x);

	pix.x = (Float)x + 0.5f;
	pix.y = (Float)y + 0.5f;

	if (s3d->prim_type==GF_EVG_POINTS) {
		if (s3d->smooth_points) {
			Float dx, dy;
			dx = (Float)x - surf->s_v1.x;
			dy = (Float)y - surf->s_v1.y;
			dx *=dx;
			dy *=dy;
			if (dx+dy > s3d->pt_radius) {
				spans[i].coverage=0;
				continue;
			}
		}
		depth = surf->s_v1.z;

		bc1 = bc2 = bc3 = 0;
		if (coverage!=0xFF) {
			full_cover = GF_FALSE;
		}
	} else if (s3d->prim_type==GF_EVG_LINES) {
		GF_Vec pt;

		gf_vec_diff(pt, pix, surf->s_v1);
		bc1 = gf_vec_len(pt);
		bc1 /= s3d->v1v2_length;
		bc1 = float_clamp(bc1, 0, 1);
		bc2 = FIX_ONE - bc1;
		depth = surf->s_v1.z * bc1 + surf->s_v2.z * bc2;

		bc3 = 0;
		if (coverage!=0xFF) {
			full_cover = GF_FALSE;
		}
	} else {
		bc1 = edgeFunction_pre(&surf->s_v2, surf->s3_m_s2_x, surf->s3_m_s2_y, &pix);
		bc1 /= surf->tri_area;
		bc2 = edgeFunction_pre(&surf->s_v3, surf->s1_m_s3_x, surf->s1_m_s3_y, &pix);
		bc2 /= surf->tri_area;

		/* in antialiased mode, we don't need to test for bc1>=0 && bc2>=0 && bc3>=0 (ie, is the pixel in the triangle),
		because we already know the point is in the triangle since we are called back
		in non-AA mode, coverage is forced to full pixel, and we check if we are or not in the triangle*/
		if (s3d->disable_aa) {
			bc3 = edgeFunction_pre(&surf->s_v1, surf->s2_m_s1_x, surf->s2_m_s1_y, &pix);
			bc3 /= surf->tri_area;
			if ((bc1<0) || (bc2<0) || (bc3<0)) {
				spans[i].coverage=0;
				continue;
			}
		}
		else {
			bc3 = 1.0f - bc1 - bc2;
			/*if coverage is not full, we are on a line - recompute the weights assuming the pixel is exactly on the line*/
			if (coverage!=0xFF) {
				if (!s3d->mode2d) {
					bc1 = float_clamp(bc1, 0, 1);
					bc2 = float_clamp(bc2, 0, 1);
					bc3 = float_clamp(bc3, 0, 1);
				}

				full_cover = GF_FALSE;
				transparent = GF_TRUE;
			}
		}
		depth = surf->s_v1.z * bc1 + surf->s_v2.z * bc2 + surf->s_v3.z * bc3;
	}

	//clip by depth
	if ((depth<s3d->min_depth) || (depth>s3d->max_depth)) {
		spans[i].coverage=0;
		continue;
	}

	depth_buf_val = depth_line ? depth_line[x] : s3d->max_depth;
	if (prev_partial) {
		if ((spans[i].idx1==prev_partial->idx1)
			|| (spans[i].idx1==prev_partial->idx2)
			|| (spans[i].idx2==prev_partial->idx1)
			|| (spans[i].idx2==prev_partial->idx2)
		) {
			depth = prev_partial->depth;
			edge_merge = GF_TRUE;
		} else {
			edge_merge = GF_FALSE;
			if (!s3d->depth_test(prev_partial->write_depth, depth_buf_val))
				depth_buf_val = prev_partial->write_depth;
		}
	}


	//do depth test except for edges
	if (! edge_merge && s3d->early_depth_test) {
		if (! s3d->depth_test(depth_buf_val, depth)) {
			spans[i].coverage=0;
			continue;
		}
	}

	rctx->frag_param.screen_x = pix.x;
	rctx->frag_param.screen_y = pix.y;
	//compute Z window coord
	rctx->frag_param.screen_z = s3d->zw_factor * depth + s3d->zw_offset;
	rctx->frag_param.depth = depth;
	rctx->frag_param.color.q = 1.0;
	rctx->frag_param.frag_valid = 0;
	/*perspective corrected barycentric, eg bc1/q1, bc2/q2, bc3/q3 - we already have store 1/q in the perspective divide step*/
	rctx->frag_param.pbc1 = bc1 * surf->s_v1.q;
	rctx->frag_param.pbc2 = bc2 * surf->s_v2.q;
	rctx->frag_param.pbc3 = bc3 * surf->s_v3.q;

	rctx->frag_param.persp_denum = rctx->frag_param.pbc1 + rctx->frag_param.pbc2 + rctx->frag_param.pbc3;

	evg_get_fragment(surf, rctx, &transparent);
	if (!rctx->frag_param.frag_valid) {
		spans[i].coverage=0;
		continue;
	}
	if (!s3d->early_depth_test) {
		if (! s3d->depth_test(depth_buf_val, rctx->frag_param.depth)) {
			spans[i].coverage=0;
			continue;
		}
	}
	//we overwrite a partial
	if (prev_partial) {
		if (edge_merge) {
			u32 ncov = coverage;

			ncov += prev_partial->cover;
			if (!s3d->depth_test(depth_buf_val, depth)) {
				remove_patch_pixel(sl, prev_partial->x);
				spans[i].coverage=0;
				continue;
			}
			if (ncov>=0xFF) {
				if (prev_partial->cover==0xFF) {
					spans[i].coverage=0;
					continue;
				}
				remove_patch_pixel(sl, prev_partial->x);
				full_cover = GF_TRUE;
				coverage = 0xFF;
			} else {
				prev_partial->cover = ncov;
				prev_partial->color = rctx->fill_col;
				spans[i].coverage=0;
				continue;
			}
		}
		else if (s3d->depth_test(prev_partial->write_depth, depth)) {
			prev_partial->write_depth = depth;
		}
	}

	//partial coverage, store
	if (!full_cover && !s3d->mode2d) {
		push_patch_pixel(sl, x, rctx->fill_col, coverage, depth, depth_buf_val, spans[i].idx1, spans[i].idx2);
		spans[i].coverage=0;
		continue;
	}
	//full opacity, write
	else {
		if (surf->fill_single) {
			if (transparent) {
				surf->fill_single_a(y, x, coverage, rctx->fill_col, surf);
			} else {
				surf->fill_single(y, x, rctx->fill_col, surf);
			}
		} else {
			spans[i].coverage = 0xFF;
			((u32 *)rctx->stencil_pix_run)[x] = rctx->fill_col;
		}
	}

	//write depth
	if (s3d->run_write_depth)
		depth_line[x] = rctx->frag_param.depth;


		}	//end span.len loop
	} //end spans count loop

	if (!surf->fill_single) {
		surf->fill_spans(y, count, spans, surf, rctx);
	}
}

static GFINLINE Bool precompute_tri(GF_EVGSurface *surf, GF_EVGFragmentParam *fparam, TPos xmin, TPos xmax, TPos ymin, TPos ymax,
									TPos _x1, TPos _y1, TPos _x2, TPos _y2, TPos _x3, TPos _y3,
									GF_Vec4 *s_pt1, GF_Vec4 *s_pt2, GF_Vec4 *s_pt3,
									u32 vidx1, u32 vidx2, u32 vidx3
)
{
	/*completely outside viewport*/
	if ((_x1<xmin) && (_x2<xmin) && (_x3<xmin))
		return GF_FALSE;
	if ((_y1<ymin) && (_y2<ymin) && (_y3<ymin))
		return GF_FALSE;
	if ((_x1>=xmax) && (_x2>=xmax) && (_x3>=xmax))
		return GF_FALSE;
	if ((_y1>=ymax) && (_y2>=ymax) && (_y3>=ymax))
		return GF_FALSE;

	surf->tri_area = edgeFunction(s_pt1, s_pt2, s_pt3);

	surf->s_v1 = *s_pt1;
	surf->s_v2 = *s_pt2;
	surf->s_v3 = *s_pt3;

	//precompute a few things for this run
	surf->s3_m_s2_x = surf->s_v3.x - surf->s_v2.x;
	surf->s3_m_s2_y = surf->s_v3.y - surf->s_v2.y;
	surf->s1_m_s3_x = surf->s_v1.x - surf->s_v3.x;
	surf->s1_m_s3_y = surf->s_v1.y - surf->s_v3.y;
	surf->s2_m_s1_x = surf->s_v2.x - surf->s_v1.x;
	surf->s2_m_s1_y = surf->s_v2.y - surf->s_v1.y;

	if (fparam) {
		fparam->idx1 = vidx1;
		fparam->idx2 = vidx2;
		fparam->idx3 = vidx3;
	}
	return GF_TRUE;
}

GF_Err evg_raster_render3d(GF_EVGSurface *surf, u32 *indices, u32 nb_idx, Float *vertices, u32 nb_vertices, u32 nb_comp, GF_EVGPrimitiveType prim_type)
{
	u32 i, li, size_y, nb_comp_1, idx_inc;
	GF_Matrix projModeView;
	EVG_Span span;
	u32 is_strip_fan=0;
	EVG_Surface3DExt *s3d = surf->ext3d;
	TPos xmin, xmax, ymin, ymax;
	Bool glob_quad_done = GF_TRUE;
	u32 prim_index=0;
	GF_EVGFragmentParam fparam;
	GF_EVGVertexParam vparam;
	int hpw=0;
	int hlw=0;
	if (!surf->frag_shader) return GF_BAD_PARAM;

	surf->render_span  = (EVG_SpanFunc) EVG3D_SpanFunc;

	/* Set up state in the raster object */
	surf->min_ex = surf->clip_xMin;
	surf->min_ey = surf->clip_yMin;
	surf->max_ex = surf->clip_xMax;
	surf->max_ey = surf->clip_yMax;

	s3d->run_write_depth = s3d->depth_buffer ? s3d->write_depth : GF_FALSE;

	size_y = (u32) (surf->max_ey - surf->min_ey);
	if (surf->max_lines < size_y) {
		surf->scanlines = (AAScanline*)gf_realloc(surf->scanlines, sizeof(AAScanline)*size_y);
		memset(&surf->scanlines[surf->max_lines], 0, sizeof(AAScanline)*(size_y - surf->max_lines) );
		surf->max_lines = size_y;
	}
	for (li=0; li<size_y; li++) {
		surf->scanlines[li].pnum = 0;
	}
	surf->first_patch = 0xFFFFFFFF;
	surf->last_patch = 0;

	gf_mx_copy(projModeView, s3d->proj);
	gf_mx_add_matrix_4x4(&projModeView, &s3d->modelview);

	switch (prim_type) {
	case GF_EVG_LINE_STRIP:
		is_strip_fan = 1;
	case GF_EVG_LINES:
		idx_inc = 2;
		hlw = (int) (s3d->line_size*ONE_PIXEL/2);
		prim_type = GF_EVG_LINES;
		break;
	case GF_EVG_TRIANGLES:
		idx_inc = 3;
		break;
	case GF_EVG_TRIANGLE_STRIP:
		is_strip_fan = 1;
		idx_inc = 3;
		prim_type = GF_EVG_TRIANGLES;
		break;
	case GF_EVG_POLYGON:
	case GF_EVG_TRIANGLE_FAN:
		is_strip_fan = 2;
		idx_inc = 3;
		prim_type = GF_EVG_TRIANGLES;
		break;
	case GF_EVG_QUADS:
		idx_inc = 4;
		glob_quad_done = GF_FALSE;
		break;
	case GF_EVG_QUAD_STRIP:
		idx_inc = 4;
		glob_quad_done = GF_FALSE;
		is_strip_fan = 1;
		prim_type = GF_EVG_QUADS;
		break;
	default:
		idx_inc = 1;
		hpw = (int) (s3d->point_size*ONE_PIXEL/2);
		s3d->pt_radius = (Float) (s3d->point_size*s3d->point_size) / 4;

		break;
	}
	if (!is_strip_fan && (nb_idx % idx_inc))
		return GF_BAD_PARAM;

	s3d->prim_type = prim_type;
	memset(&fparam, 0, sizeof(GF_EVGFragmentParam));
	fparam.ptype = prim_type;
	fparam.prim_index = 0;

	xmin = surf->min_ex * ONE_PIXEL;
	xmax = surf->max_ex * ONE_PIXEL;
	ymin = surf->min_ey * ONE_PIXEL;
	ymax = surf->max_ey * ONE_PIXEL;

	//raster coordinates of points
	TPos _x1=0, _y1=0, _x2=0, _y2=0, _x3=0, _y3=0, _x4=0, _y4=0, _prev_x2=0, _prev_y2=0;
	/*vertices in raster/window space: x and y are in pixel space, z is [min_depth, max_depth]*/
	GF_Vec4 s_pt1, s_pt2, s_pt3, s_pt4, prev_s_pt2;
	u32 idx1=0, idx2=0, idx3=0, idx4=0;
	u32 vidx1=0, vidx2=0, vidx3=0, vidx4=0, prev_vidx2=0;

	memset(&vparam, 0, sizeof(GF_EVGVertexParam));
	vparam.ptype = prim_type;

	nb_comp_1 = nb_comp-1;
	for (i=0; i<nb_idx; i += idx_inc) {
		Bool quad_done = glob_quad_done;
		GF_Vec4 *vx = &vparam.in_vertex;

		if (s3d->vert_shader) {
			vparam.prim_index = prim_index;
		}
		prim_index++;
		
#define GETVEC(_name, _idx)\
		vx->x = vertices[_idx];\
		vx->y = vertices[_idx+1];\
		if (_idx+nb_comp_1>=nb_vertices) return GF_BAD_PARAM;\
		vx->z = (nb_comp>2) ? vertices[_idx+2] : 0.0f;\
		vx->q = 1.0;\
		if (s3d->vert_shader) {\
			s3d->vert_shader(s3d->vert_shader_udta, &vparam); \
			s_##_name = vparam.out_vertex;\
		} else {\
			s_##_name.x = vx->x;\
			s_##_name.y = vx->y;\
			s_##_name.z = vx->z;\
			s_##_name.q = 1;\
			gf_mx_apply_vec_4x4(&projModeView, &s_##_name);\
		}\
		if (!evg3d_persp_divide(&s_##_name)) continue;\

		/*get vertice, apply transform and perspective divide and translate to raster*/
		if (is_strip_fan && i) {
			idx_inc=1;
			if (prim_type==GF_EVG_LINES) {
				_x1 = _x2;
				_y1 = _y2;
				s_pt1 = s_pt2;
				vidx1 = vidx2;
				vidx2 = indices[i];
				idx2 = vidx2 * nb_comp;
				vparam.vertex_idx = vidx2;
				vparam.vertex_idx_in_prim = 1;
				GETVEC(pt2, idx2)
				evg_ndc_to_raster(surf, &s_pt2, &_x2, &_y2);
			}
			//triangle strip
			else if (is_strip_fan==1) {
				/*triangle strip*/
				if (prim_type==GF_EVG_TRIANGLES) {
					/*swap v2 to v1 and v3 to v2*/
					_x1 = _x2;
					_y1 = _y2;
					s_pt1 = s_pt2;
					vidx1 = vidx2;

					_x2 = _x3;
					_y2 = _y3;
					s_pt2 = s_pt3;
					vidx2 = vidx3;

					vidx3 = indices[i];
					idx3 = vidx3 * nb_comp;
					vparam.vertex_idx = vidx3;
					vparam.vertex_idx_in_prim = 2;
					GETVEC(pt3, idx3)
					evg_ndc_to_raster(surf, &s_pt3, &_x3, &_y3);
				}
				//quad strip - since we draw [v1, v2, v3, v4] as 2 triangles [v1, v2, v3] and [v1, v3, v4]
				// we only need to restore prev v2 as v1
				else {
					_x1 = _prev_x2;
					_y1 = _prev_y2;
					s_pt1 = prev_s_pt2;
					vidx1 = prev_vidx2;

					vidx4 = indices[i];
					idx4 = vidx4 * nb_comp;
					vparam.vertex_idx = vidx4;
					vparam.vertex_idx_in_prim = 3;
					GETVEC(pt4, idx4)
					evg_ndc_to_raster(surf, &s_pt4, &_x4, &_y4);
				}
			}
			//triangle fan
			else {
				//keep center and move 3rd vertex as second
				_x2 = _x3;
				_y2 = _y3;
				s_pt2 = s_pt3;
				vidx2 = vidx3;

				vidx3 = indices[i];
				idx3 = vidx3 * nb_comp;
				vparam.vertex_idx = vidx3;
				vparam.vertex_idx_in_prim = 2;
				GETVEC(pt3, idx3)
				evg_ndc_to_raster(surf, &s_pt3, &_x3, &_y3);
			}
		} else {
			vidx1 = indices[i];
			idx1 = vidx1 * nb_comp;
			vparam.vertex_idx = vidx1;
			vparam.vertex_idx_in_prim = 0;
			GETVEC(pt1, idx1)
			evg_ndc_to_raster(surf, &s_pt1, &_x1, &_y1);
			if (prim_type>=GF_EVG_LINES) {
				vidx2 = indices[i+1];
				idx2 = vidx2 * nb_comp;
				vparam.vertex_idx = vidx2;
				vparam.vertex_idx_in_prim = 1;
				GETVEC(pt2, idx2)
				evg_ndc_to_raster(surf, &s_pt2, &_x2, &_y2);
				if (prim_type>=GF_EVG_TRIANGLES) {
					vidx3 = indices[i+2];
					idx3 = vidx3 * nb_comp;
					vparam.vertex_idx = vidx3;
					vparam.vertex_idx_in_prim = 2;
					GETVEC(pt3, idx3)
					evg_ndc_to_raster(surf, &s_pt3, &_x3, &_y3);
					if (prim_type==GF_EVG_QUADS) {
						vidx4 = indices[i+3];
						idx4 = vidx4 * nb_comp;
						vparam.vertex_idx = vidx4;
						vparam.vertex_idx_in_prim = 3;
						GETVEC(pt4, idx4)
						evg_ndc_to_raster(surf, &s_pt4, &_x4, &_y4);
					}
				}
			}
		}
#undef GETVEC

restart_quad:
		surf->first_scanline = surf->height;
		surf->ex = (int) (surf->max_ex+1);
		surf->ey = (int) (surf->max_ey+1);
		surf->cover = 0;
		surf->area = 0;


		if (prim_type>=GF_EVG_TRIANGLES) {

			if (!precompute_tri(surf, &fparam, xmin, xmax, ymin, ymax, _x1, _y1, _x2, _y2, _x3, _y3, &s_pt1, &s_pt2, &s_pt3, vidx1, vidx2, vidx3))
				continue;

			//check backcull
			if (s3d->is_ccw) {
				if (surf->tri_area<0) {
					if (s3d->backface_cull)
						continue;
				}
			} else {
				if (surf->tri_area>0) {
					if (s3d->backface_cull)
						continue;
				}
			}
		} else {
			surf->s_v1 = s_pt1;
			fparam.idx1 = vidx1;
			if (prim_type==GF_EVG_LINES) {
				GF_Vec lv;
				surf->s_v2 = s_pt2;
				gf_vec_diff(lv, s_pt2, s_pt1);
				lv.z=0;
				s3d->v1v2_length = gf_vec_len(lv);
				fparam.idx2 = vidx2;
			}
		}
		fparam.prim_index = prim_index-1;

		if (prim_type==GF_EVG_POINTS) {
			surf->idx1 = surf->idx2 = idx1;
			//draw square
			gray3d_move_to(surf, _x1-hpw, _y1-hpw);
			gray_render_line(surf, _x1+hpw, _y1-hpw);
			gray_render_line(surf, _x1+hpw, _y1+hpw);
			gray_render_line(surf, _x1-hpw, _y1+hpw);
			//and close
			gray_render_line(surf, _x1-hpw, _y1-hpw);
			gray_record_cell(surf);
		} else if (prim_type==GF_EVG_LINES) {
			surf->idx1 = idx1;
			surf->idx2 = idx2;
			gray3d_move_to(surf, _x1+hlw, _y1+hlw);
			gray_render_line(surf, _x1-hlw, _y1-hlw);
			gray_render_line(surf, _x2-hlw, _y2-hlw);
			gray_render_line(surf, _x2+hlw, _y2+hlw);
			//and close
			gray_render_line(surf, _x1+hlw, _y1+hlw);
			gray_record_cell(surf);
		} else {
			surf->idx1 = idx1;
			surf->idx2 = idx2;
			gray3d_move_to(surf, _x1, _y1);
			gray_render_line(surf, _x2, _y2);
			surf->idx1 = idx2;
			surf->idx2 = idx3;
			gray_render_line(surf, _x3, _y3);

			//and close
			surf->idx1 = idx3;
			surf->idx2 = idx1;
			gray_render_line(surf, _x1, _y1);
			gray_record_cell(surf);
		}

		GF_Err e = evg_sweep_lines(surf, size_y, GF_FALSE, GF_TRUE, &fparam);
		if (e) return e;

		if (!quad_done) {
			if (is_strip_fan) {
				_prev_x2 = _x2;
				_prev_y2 = _y2;
				prev_s_pt2 = s_pt2;
				prev_vidx2 = vidx2;
			}
			quad_done = GF_TRUE;
			_x2 = _x3;
			_y2 = _y3;
			_x3 = _x4;
			_y3 = _y4;
			s_pt2 = s_pt3;
			s_pt3 = s_pt4;
			vidx2 = vidx3;
			vidx3 = vidx4;
			surf->s_v1 = surf->s_v2;
			surf->s_v2 = surf->s_v3;
			goto restart_quad;
		}
	}

	/*flush all partial fragments*/
	span.len = 0;
	for (li=surf->first_patch; li<=surf->last_patch; li++) {
		AAScanline *sl = &surf->scanlines[li];
		Float *depth_line = &surf->ext3d->depth_buffer[li];
		for (i=0; i<sl->pnum; i++) {
			PatchPixel *pi = &sl->pixels[i];

			if (pi->cover == 0xFF) continue;
			if (!surf->ext3d->depth_test(pi->write_depth, pi->depth)) continue;

			if (surf->fill_single) {
				surf->fill_single_a(li, pi->x, pi->cover, pi->color, surf);
			} else {
				span.coverage = pi->cover;
				span.x = pi->x;
				surf->fill_spans(li, 1, &span, surf, &surf->raster_ctx);
			}
			if (surf->ext3d->run_write_depth)
				depth_line[pi->x] = pi->depth;
		}
	}
	return GF_OK;
}

GF_Err evg_raster_render3d_path(GF_EVGSurface *surf, GF_Path *path, Float z)
{

	EVG_Vector   v_start;
	int   n;         /* index of contour in outline     */
	int   first;     /* index of first point in contour */
	TPos _x, _y, _sx, _sy;
	u32 size_y, fill_rule = 0;;
	EVG_Outline *outline;
	GF_Matrix projModeView;
	GF_EVGFragmentParam fparam;
	EVG_Surface3DExt *s3d = surf->ext3d;
	u32 xmin, xmax, ymin, ymax;
	GF_Err e;
	GF_Rect rc;

	if (!surf->frag_shader) return GF_BAD_PARAM;

	e = gf_evg_surface_set_path(surf, path);
	if (e) return e;

	outline = &surf->ftoutline;

	surf->render_span = (EVG_SpanFunc) EVG3D_SpanFunc;

	/* Set up state in the raster object */
	surf->min_ex = surf->clip_xMin;
	surf->min_ey = surf->clip_yMin;
	surf->max_ex = surf->clip_xMax;
	surf->max_ey = surf->clip_yMax;
	s3d->run_write_depth = s3d->depth_buffer ? s3d->write_depth : GF_FALSE;

	size_y = (u32) (surf->max_ey - surf->min_ey);
	if (surf->max_lines < size_y) {
		surf->scanlines = (AAScanline*)gf_realloc(surf->scanlines, sizeof(AAScanline)*size_y);
		if (!surf->scanlines) return GF_OUT_OF_MEM;
		memset(&surf->scanlines[surf->max_lines], 0, sizeof(AAScanline)*(size_y-surf->max_lines) );
		surf->max_lines = size_y;
	}

	gf_mx_copy(projModeView, s3d->proj);
	gf_mx_add_matrix_4x4(&projModeView, &s3d->modelview);

	s3d->prim_type = GF_EVG_TRIANGLES;
	memset(&fparam, 0, sizeof(GF_EVGFragmentParam));
	fparam.ptype = GF_EVG_TRIANGLES;
	fparam.prim_index = 0;

	xmin = surf->min_ex * ONE_PIXEL;
	xmax = surf->max_ex * ONE_PIXEL;
	ymin = surf->min_ey * ONE_PIXEL;
	ymax = surf->max_ey * ONE_PIXEL;

	//raster coordinates of points
	TPos _x1, _y1, _x2, _y2, _x3, _y3;
	/*vertices in raster/window space: x and y are in pixel space, z is [min_depth, max_depth]*/
	GF_Vec4 s_pt1, s_pt2, s_pt3;

	/*get bounds, and compute interpolation from (top-left, top-right, bottom-right) vertices*/
	gf_path_get_bounds(path, &rc);
	s_pt1.x = rc.x; s_pt1.y = rc.y; s_pt1.z = z; s_pt1.q = 1;
	s_pt2.x = rc.x + rc.width; s_pt2.y = rc.y; s_pt2.z = z; s_pt2.q = 1;
	s_pt3.x = rc.x + rc.width; s_pt3.y = rc.y - rc.height ; s_pt3.z = z; s_pt3.q = 1;

	gf_mx_apply_vec_4x4(&projModeView, &s_pt1);
	evg3d_persp_divide(&s_pt1);
	evg_ndc_to_raster(surf, &s_pt1, &_x1, &_y1);
	gf_mx_apply_vec_4x4(&projModeView, &s_pt2);
	evg3d_persp_divide(&s_pt2);
	evg_ndc_to_raster(surf, &s_pt2, &_x2, &_y2);
	gf_mx_apply_vec_4x4(&projModeView, &s_pt3);
	evg3d_persp_divide(&s_pt3);
	evg_ndc_to_raster(surf, &s_pt3, &_x3, &_y3);

	if (!precompute_tri(surf, &fparam, xmin, xmax, ymin, ymax, _x1, _y1, _x2, _y2, _x3, _y3, &s_pt1, &s_pt2, &s_pt3, 0, 1, 2))
		return GF_OK;

	s3d->mode2d = GF_TRUE;
	first = 0;
	for ( n = 0; n < outline->n_contours; n++ ) {
		GF_Vec4 pt;
		EVG_Vector *point;
		EVG_Vector *limit;
		int  last;  /* index of last point in contour */
		last  = outline->contours[n];
		limit = outline->points + last;
		v_start = outline->points[first];
		point = outline->points + first;

		pt.x = v_start.x;
		pt.y = v_start.y;
		pt.z = z;
		pt.q = 1;
		gf_mx_apply_vec_4x4(&projModeView, &pt);
		if (!evg3d_persp_divide(&pt)) {
			continue;
		}
		evg_ndc_to_raster(surf, &pt, &_sx, &_sy);
		gray3d_move_to(surf, _sx, _sy);
		while ( point < limit ) {
			point++;

			pt.x = point->x;
			pt.y = point->y;
			pt.z = z;
			pt.q = 1;
			gf_mx_apply_vec_4x4(&projModeView, &pt);
			if (!evg3d_persp_divide(&pt)) {
				break;
			}
			evg_ndc_to_raster(surf, &pt, &_x, &_y);
			gray_render_line(surf, _x, _y);
		}
		gray_render_line(surf, _sx, _sy);

		first = last + 1;
	}

	gray_record_cell(surf);

	if (outline->flags & GF_PATH_FILL_ZERO_NONZERO) fill_rule = 1;
	else if (outline->flags & GF_PATH_FILL_EVEN) fill_rule = 2;
	e = evg_sweep_lines(surf, size_y, fill_rule, GF_FALSE, &fparam);
	surf->ext3d->mode2d = GF_FALSE;
	return e;

}

GF_EXPORT
GF_Err gf_evg_surface_clear_depth(GF_EVGSurface *surf, Float depth)
{
	u32 i, lsize;
	Float *depths;
	u8 *depth_p;
	if (!surf->ext3d) return GF_BAD_PARAM;

	depths = surf->ext3d->depth_buffer;
	if (!depths) return GF_OK; //GF_BAD_PARAM;
	//copy first line
	for (i=0; i<surf->width; i++) {
		depths[i] = depth;
	}
	//copy first line
	depth_p = (u8 *) surf->ext3d->depth_buffer;
	lsize = sizeof(Float) * surf->width;
	for (i=1; i<surf->height; i++) {
		u8 *depth_l = depth_p + i*lsize;
		memcpy(depth_l, depth_p, lsize);
	}
	return GF_OK;
}


GF_EXPORT
GF_Err gf_evg_surface_viewport(GF_EVGSurface *surf, u32 x, u32 y, u32 w, u32 h)
{
	if (!surf) return GF_BAD_PARAM;
	surf->vp_x = x;
	surf->vp_y = y;
	surf->vp_w = w;
	surf->vp_h = h;
	return GF_OK;
}

static Bool depth_test_never(Float depth_buf_value, Float frag_value)
{
	return GF_FALSE;
}
static Bool depth_test_always(Float depth_buf_value, Float frag_value)
{
	return GF_TRUE;
}
static Bool depth_test_less(Float depth_buf_value, Float frag_value)
{
	if (frag_value<depth_buf_value) return GF_TRUE;
	return GF_FALSE;
}
static Bool depth_test_less_equal(Float depth_buf_value, Float frag_value)
{
	if (frag_value<=depth_buf_value) return GF_TRUE;
	return GF_FALSE;
}
static Bool depth_test_equal(Float depth_buf_value, Float frag_value)
{
	if (frag_value==depth_buf_value) return GF_TRUE;
	return GF_FALSE;
}
static Bool depth_test_greater(Float depth_buf_value, Float frag_value)
{
	if (frag_value>depth_buf_value) return GF_TRUE;
	return GF_FALSE;
}
static Bool depth_test_greater_equal(Float depth_buf_value, Float frag_value)
{
	if (frag_value>=depth_buf_value) return GF_TRUE;
	return GF_FALSE;
}
static Bool depth_test_not_equal(Float depth_buf_value, Float frag_value)
{
	if (frag_value==depth_buf_value) return GF_FALSE;
	return GF_TRUE;
}

GF_EXPORT
GF_Err gf_evg_set_depth_test(GF_EVGSurface *surf, GF_EVGDepthTest mode)
{
	if (!surf->ext3d) return GF_BAD_PARAM;
	surf->ext3d->write_depth = GF_TRUE;
	switch (mode) {
	case GF_EVGDEPTH_NEVER:
		surf->ext3d->depth_test = depth_test_never;
		return GF_OK;
	case GF_EVGDEPTH_ALWAYS:
		surf->ext3d->depth_test = depth_test_always;
		return GF_OK;
	case GF_EVGDEPTH_EQUAL:
		surf->ext3d->depth_test = depth_test_equal;
		return GF_OK;
	case GF_EVGDEPTH_NEQUAL:
		surf->ext3d->depth_test = depth_test_not_equal;
		return GF_OK;
	case GF_EVGDEPTH_LESS:
		surf->ext3d->depth_test = depth_test_less;
		return GF_OK;
	case GF_EVGDEPTH_LESS_EQUAL:
		surf->ext3d->depth_test = depth_test_less_equal;
		return GF_OK;
	case GF_EVGDEPTH_GREATER:
		surf->ext3d->depth_test = depth_test_greater;
		return GF_OK;
	case GF_EVGDEPTH_GREATER_EQUAL:
		surf->ext3d->depth_test = depth_test_greater_equal;
		return GF_OK;
	case GF_EVGDEPTH_DISABLE:
		surf->ext3d->depth_test = depth_test_always;
		surf->ext3d->write_depth = GF_FALSE;
		return GF_OK;
	}
	return GF_BAD_PARAM;

}

GF_Err evg_3d_update_depth_range(GF_EVGSurface *surf)
{
	if (!surf || !surf->ext3d) return GF_BAD_PARAM;
	surf->ext3d->depth_range = surf->ext3d->max_depth - surf->ext3d->min_depth;
	if (surf->ext3d->clip_zero) {
		surf->ext3d->zw_factor = surf->ext3d->depth_range;
		surf->ext3d->zw_offset = surf->ext3d->min_depth;
	} else {
		surf->ext3d->zw_factor = surf->ext3d->depth_range / 2;
		surf->ext3d->zw_offset = (surf->ext3d->min_depth+surf->ext3d->max_depth)/2;

	}
	return GF_OK;
}


EVG_Surface3DExt *evg_init_3d_surface(GF_EVGSurface *surf)
{
	EVG_Surface3DExt *ext3d;
	GF_SAFEALLOC(ext3d, EVG_Surface3DExt);
	if (!ext3d) return NULL;
	gf_mx_init(ext3d->proj);
	gf_mx_init(ext3d->modelview);
	ext3d->backface_cull = GF_TRUE;
	ext3d->is_ccw = GF_TRUE;
	ext3d->min_depth = 0;
	ext3d->max_depth = FIX_ONE;
	ext3d->depth_range = FIX_ONE;
	ext3d->point_size = FIX_ONE;
	ext3d->line_size = FIX_ONE;
	ext3d->clip_zero = GF_FALSE;
	ext3d->disable_aa = GF_FALSE;
	ext3d->write_depth = GF_TRUE;
	ext3d->early_depth_test = GF_TRUE;
	ext3d->depth_test = depth_test_less;
	evg_3d_update_depth_range(surf);
	return ext3d;
}



GF_Err gf_evg_surface_set_fragment_shader(GF_EVGSurface *surf, gf_evg_fragment_shader shader, gf_evg_fragment_shader_init shader_init, void *shader_udta)
{
	if (!surf) return GF_BAD_PARAM;
	surf->frag_shader = shader;
	surf->frag_shader_init = shader ? shader_init : NULL;
	surf->frag_shader_udta = shader ? shader_udta : NULL;
	return GF_OK;
}

GF_Err gf_evg_surface_disable_early_depth(GF_EVGSurface *surf, Bool disable)
{
	if (!surf) return GF_BAD_PARAM;
	if (!surf->ext3d) return GF_OK;
	surf->ext3d->early_depth_test = !disable;
	return GF_OK;
}

GF_Err gf_evg_surface_set_vertex_shader(GF_EVGSurface *surf, gf_evg_vertex_shader shader, void *shader_udta)
{
	if (!surf || !surf->ext3d) return GF_BAD_PARAM;
	surf->ext3d->vert_shader = shader;
	surf->ext3d->vert_shader_udta = shader_udta;
	return GF_OK;
}

GF_Err gf_evg_surface_set_ccw(GF_EVGSurface *surf, Bool is_ccw)
{
	if (!surf || !surf->ext3d) return GF_BAD_PARAM;
	surf->ext3d->is_ccw = is_ccw;
	return GF_OK;
}
GF_Err gf_evg_surface_set_backcull(GF_EVGSurface *surf, Bool backcull)
{
	if (!surf || !surf->ext3d) return GF_BAD_PARAM;
	surf->ext3d->backface_cull = backcull;
	return GF_OK;
}
GF_Err gf_evg_surface_set_antialias(GF_EVGSurface *surf, Bool antialias)
{
	if (!surf || !surf->ext3d) return GF_BAD_PARAM;
	surf->ext3d->disable_aa = antialias ? GF_FALSE : GF_TRUE;
	return GF_OK;
}
GF_Err gf_evg_surface_set_min_depth(GF_EVGSurface *surf, Float min_depth)
{
	if (!surf || !surf->ext3d) return GF_BAD_PARAM;
	surf->ext3d->min_depth = min_depth;
	evg_3d_update_depth_range(surf);
	return GF_OK;
}
GF_Err gf_evg_surface_set_max_depth(GF_EVGSurface *surf, Float max_depth)
{
	if (!surf || !surf->ext3d) return GF_BAD_PARAM;
	surf->ext3d->max_depth = max_depth;
	evg_3d_update_depth_range(surf);
	return GF_OK;
}

GF_Err gf_evg_surface_set_clip_zero(GF_EVGSurface *surf, Bool clip_zero)
{
	if (!surf || !surf->ext3d) return GF_BAD_PARAM;
	surf->ext3d->clip_zero = clip_zero;
	evg_3d_update_depth_range(surf);
	return GF_OK;
}

GF_Err gf_evg_surface_set_point_size(GF_EVGSurface *surf, Float size)
{
	if (!surf || !surf->ext3d) return GF_BAD_PARAM;
	surf->ext3d->point_size = size;
	return GF_OK;
}
GF_Err gf_evg_surface_set_line_size(GF_EVGSurface *surf, Float size)
{
	if (!surf || !surf->ext3d) return GF_BAD_PARAM;
	surf->ext3d->line_size = size;
	return GF_OK;
}
GF_Err gf_evg_surface_set_point_smooth(GF_EVGSurface *surf, Bool smooth)
{
	if (!surf || !surf->ext3d) return GF_BAD_PARAM;
	surf->ext3d->smooth_points = smooth;
	return GF_OK;
}

GF_Err gf_evg_surface_write_depth(GF_EVGSurface *surf, Bool do_write)
{
	if (!surf || !surf->ext3d) return GF_BAD_PARAM;
	surf->ext3d->write_depth = do_write;
	return GF_OK;
}
GF_Err gf_evg_surface_set_depth_buffer(GF_EVGSurface *surf, Float *depth)
{
	if (!surf || !surf->ext3d) return GF_BAD_PARAM;
	surf->ext3d->depth_buffer = depth;
	return GF_OK;
}
