/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
 *					All rights reserved
 *
 *  This file is part of GPAC / common tools sub-project
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


#include <gpac/path2d.h>

#if !defined(GPAC_DISABLE_EVG) || defined(GPAC_HAS_FREETYPE)

GF_EXPORT
GF_Path *gf_path_new()
{
	GF_Path *gp;
	GF_SAFEALLOC(gp, GF_Path);
	if (!gp) return NULL;
	gp->fineness = FIX_ONE;
	return gp;
}

GF_EXPORT
void gf_path_reset(GF_Path *gp)
{
	Fixed fineness;
	u32 flags;
	if (!gp) return;
	if (gp->contours) gf_free(gp->contours);
	if (gp->tags) gf_free(gp->tags);
	if (gp->points) gf_free(gp->points);
	fineness = gp->fineness ? gp->fineness : FIX_ONE;
	flags = gp->flags;
	memset(gp, 0, sizeof(GF_Path));
	gp->flags = flags | GF_PATH_FLATTENED | GF_PATH_BBOX_DIRTY;
	gp->fineness = fineness;
}

GF_EXPORT
GF_Path *gf_path_clone(GF_Path *gp)
{
	GF_Path *dst;
	GF_SAFEALLOC(dst, GF_Path);
	if (!dst) return NULL;
	dst->contours = (u32 *)gf_malloc(sizeof(u32)*gp->n_contours);
	if (!dst->contours) {
		gf_free(dst);
		return NULL;
	}
	dst->points = (GF_Point2D *) gf_malloc(sizeof(GF_Point2D)*gp->n_points);
	if (!dst->points) {
		gf_free(dst->contours);
		gf_free(dst);
		return NULL;
	}
	dst->tags = (u8 *) gf_malloc(sizeof(u8)*gp->n_points);
	if (!dst->tags) {
		gf_free(dst->points);
		gf_free(dst->contours);
		gf_free(dst);
		return NULL;
	}
	memcpy(dst->contours, gp->contours, sizeof(u32)*gp->n_contours);
	dst->n_contours = gp->n_contours;
	memcpy(dst->points, gp->points, sizeof(GF_Point2D)*gp->n_points);
	memcpy(dst->tags, gp->tags, sizeof(u8)*gp->n_points);
	dst->n_alloc_points = dst->n_points = gp->n_points;
	dst->flags = gp->flags;
	dst->bbox = gp->bbox;
	dst->fineness = gp->fineness;
	return dst;
}

GF_EXPORT
void gf_path_del(GF_Path *gp)
{
	if (!gp) return;
	if (gp->contours) gf_free(gp->contours);
	if (gp->tags) gf_free(gp->tags);
	if (gp->points) gf_free(gp->points);
	gf_free(gp);
}

#define GF_2D_REALLOC(_gp)	\
	if (_gp->n_alloc_points < _gp->n_points+3) {	\
		_gp->n_alloc_points = (_gp->n_alloc_points<5) ? 10 : (_gp->n_alloc_points*2);	\
		_gp->points = (GF_Point2D *)gf_realloc(_gp->points, sizeof(GF_Point2D)*(_gp->n_alloc_points));	\
		_gp->tags = (u8 *) gf_realloc(_gp->tags, sizeof(u8)*(_gp->n_alloc_points));	\
	}	\
 

GF_EXPORT
GF_Err gf_path_add_move_to(GF_Path *gp, Fixed x, Fixed y)
{
	if (!gp) return GF_BAD_PARAM;

#if 0
	/*skip empty paths*/
	if ((gp->n_contours>=2) && (gp->contours[gp->n_contours-2]+1==gp->contours[gp->n_contours-1])) {
		gp->points[gp->n_points].x = x;
		gp->points[gp->n_points].y = y;
		return GF_OK;
	}
#endif

	gp->contours = (u32 *) gf_realloc(gp->contours, sizeof(u32)*(gp->n_contours+1));
	GF_2D_REALLOC(gp)

	gp->points[gp->n_points].x = x;
	gp->points[gp->n_points].y = y;
	gp->tags[gp->n_points] = 1;
	/*set end point*/
	gp->contours[gp->n_contours] = gp->n_points;
	/*new contour*/
	gp->n_contours++;
	gp->n_points++;
	gp->flags |= GF_PATH_BBOX_DIRTY;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_path_add_move_to_vec(GF_Path *gp, GF_Point2D *pt) {
	return gf_path_add_move_to(gp, pt->x, pt->y);
}

GF_EXPORT
GF_Err gf_path_add_line_to(GF_Path *gp, Fixed x, Fixed y)
{
	if (!gp || !gp->n_contours) return GF_BAD_PARAM;
	/*we allow line to same point as move (seen in SVG sequences) - striking will make a point*/

	GF_2D_REALLOC(gp)
	gp->points[gp->n_points].x = x;
	gp->points[gp->n_points].y = y;
	gp->tags[gp->n_points] = 1;
	/*set end point*/
	gp->contours[gp->n_contours-1] = gp->n_points;
	gp->n_points++;
	gp->flags |= GF_PATH_BBOX_DIRTY;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_path_add_line_to_vec(GF_Path *gp, GF_Point2D *pt) {
	return gf_path_add_line_to(gp, pt->x, pt->y);
}

GF_EXPORT
GF_Err gf_path_close(GF_Path *gp)
{
	Fixed diff;
	GF_Point2D start, end;
	if (!gp || !gp->n_contours) return GF_BAD_PARAM;

	if (gp->n_contours<=1) start = gp->points[0];
	else start = gp->points[gp->contours[gp->n_contours-2] + 1];
	end = gp->points[gp->n_points-1];
	end.x -= start.x;
	end.y -= start.y;
	diff = gf_mulfix(end.x, end.x) + gf_mulfix(end.y, end.y);
	if (ABS(diff) > FIX_ONE/1000) {
		GF_Err e = gf_path_add_line_to(gp, start.x, start.y);
		if (e) return e;
	}
	gp->tags[gp->n_points-1] = GF_PATH_CLOSE;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_path_add_cubic_to(GF_Path *gp, Fixed c1_x, Fixed c1_y, Fixed c2_x, Fixed c2_y, Fixed x, Fixed y)
{
	if (!gp || !gp->n_contours) return GF_BAD_PARAM;
	GF_2D_REALLOC(gp)
	gp->points[gp->n_points].x = c1_x;
	gp->points[gp->n_points].y = c1_y;
	gp->tags[gp->n_points] = GF_PATH_CURVE_CUBIC;
	gp->n_points++;
	gp->points[gp->n_points].x = c2_x;
	gp->points[gp->n_points].y = c2_y;
	gp->tags[gp->n_points] = GF_PATH_CURVE_CUBIC;
	gp->n_points++;
	gp->points[gp->n_points].x = x;
	gp->points[gp->n_points].y = y;
	gp->tags[gp->n_points] = GF_PATH_CURVE_ON;
	/*set end point*/
	gp->contours[gp->n_contours-1] = gp->n_points;
	gp->n_points++;
	gp->flags |= GF_PATH_BBOX_DIRTY;
	gp->flags &= ~GF_PATH_FLATTENED;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_path_add_cubic_to_vec(GF_Path *gp, GF_Point2D *c1, GF_Point2D *c2, GF_Point2D *pt)
{
	return gf_path_add_cubic_to(gp, c1->x, c1->y, c2->x, c2->y, pt->x, pt->y);
}


GF_EXPORT
GF_Err gf_path_add_quadratic_to(GF_Path *gp, Fixed c_x, Fixed c_y, Fixed x, Fixed y)
{
	if (!gp || !gp->n_contours) return GF_BAD_PARAM;
	GF_2D_REALLOC(gp)
	gp->points[gp->n_points].x = c_x;
	gp->points[gp->n_points].y = c_y;
	gp->tags[gp->n_points] = GF_PATH_CURVE_CONIC;
	gp->n_points++;
	gp->points[gp->n_points].x = x;
	gp->points[gp->n_points].y = y;
	gp->tags[gp->n_points] = GF_PATH_CURVE_ON;
	/*set end point*/
	gp->contours[gp->n_contours-1] = gp->n_points;
	gp->n_points++;
	gp->flags |= GF_PATH_BBOX_DIRTY;
	gp->flags &= ~GF_PATH_FLATTENED;
	return GF_OK;
}
GF_EXPORT
GF_Err gf_path_add_quadratic_to_vec(GF_Path *gp, GF_Point2D *c, GF_Point2D *pt)
{
	return gf_path_add_quadratic_to(gp, c->x, c->y, pt->x, pt->y);
}

/*adds rectangle centered on cx, cy*/
GF_EXPORT
GF_Err gf_path_add_rect_center(GF_Path *gp, Fixed cx, Fixed cy, Fixed w, Fixed h)
{
	GF_Err e = gf_path_add_move_to(gp, cx - w/2, cy - h/2);
	if (e) return e;
	e = gf_path_add_line_to(gp, cx + w/2, cy - h/2);
	if (e) return e;
	e = gf_path_add_line_to(gp, cx + w/2, cy + h/2);
	if (e) return e;
	e = gf_path_add_line_to(gp, cx - w/2, cy + h/2);
	if (e) return e;
	return gf_path_close(gp);
}

GF_EXPORT
GF_Err gf_path_add_rect(GF_Path *gp, Fixed ox, Fixed oy, Fixed w, Fixed h)
{
	GF_Err e = gf_path_add_move_to(gp, ox, oy);
	if (e) return e;
	e = gf_path_add_line_to(gp, ox + w, oy);
	if (e) return e;
	e = gf_path_add_line_to(gp, ox+w, oy-h);
	if (e) return e;
	e = gf_path_add_line_to(gp, ox, oy-h);
	if (e) return e;
	return gf_path_close(gp);
}

#define GF_2D_DEFAULT_RES	64

GF_EXPORT
GF_Err gf_path_add_ellipse(GF_Path *gp, Fixed cx, Fixed cy, Fixed a_axis, Fixed b_axis)
{
	GF_Err e;
	Fixed _vx, _vy, cur;
	u32 i;
	a_axis /= 2;
	b_axis /= 2;
	e = gf_path_add_move_to(gp, cx+a_axis, cy);
	if (e) return e;
	for (i=1; i<GF_2D_DEFAULT_RES; i++) {
		cur = GF_2PI*i/GF_2D_DEFAULT_RES;
		_vx = gf_mulfix(a_axis, gf_cos(cur) );
		_vy = gf_mulfix(b_axis, gf_sin(cur) );
		e = gf_path_add_line_to(gp, _vx + cx, _vy + cy);
		if (e) return e;
	}
	return gf_path_close(gp);
}

GF_Err gf_path_add_subpath(GF_Path *gp, GF_Path *src, GF_Matrix2D *mx)
{
	u32 i;
	if (!src) return GF_OK;
	gp->contours = (u32*)gf_realloc(gp->contours, sizeof(u32) * (gp->n_contours + src->n_contours));
	if (!gp->contours) return GF_OUT_OF_MEM;
	for (i=0; i<src->n_contours; i++) {
		gp->contours[i+gp->n_contours] = src->contours[i] + gp->n_points;
	}
	gp->n_contours += src->n_contours;
	gp->n_alloc_points += src->n_alloc_points;
	gp->points = (GF_Point2D*)gf_realloc(gp->points, sizeof(GF_Point2D)*gp->n_alloc_points);
	if (!gp->points) return GF_OUT_OF_MEM;
	gp->tags = (u8*)gf_realloc(gp->tags, sizeof(u8)*gp->n_alloc_points);
	if (!gp->tags) return GF_OUT_OF_MEM;
	if (src->n_points) {
		memcpy(gp->points + gp->n_points, src->points, sizeof(GF_Point2D)*src->n_points);
		if (mx) {
			for (i=0; i<src->n_points; i++) {
				gf_mx2d_apply_coords(mx, &gp->points[i+gp->n_points].x, &gp->points[i+gp->n_points].y);
			}
		}
		memcpy(gp->tags + gp->n_points, src->tags, sizeof(u8)*src->n_points);
		gp->n_points += src->n_points;
	}
	gf_rect_union(&gp->bbox, &src->bbox);
	if (!(src->flags & GF_PATH_FLATTENED)) gp->flags &= ~GF_PATH_FLATTENED;
	if (src->flags & GF_PATH_BBOX_DIRTY) gp->flags |= GF_PATH_BBOX_DIRTY;
	return GF_OK;
}

/*generic N-bezier*/
static void NBezier(GF_Point2D *pts, s32 n, Double mu, GF_Point2D *pt_out)
{
	s32 k,kn,nn,nkn;
	Double blend, muk, munk;
	pt_out->x = pt_out->y = 0;

	muk = 1;
	munk = pow(1-mu,(Double)n);
	for (k=0; k<=n; k++) {
		nn = n;
		kn = k;
		nkn = n - k;
		blend = muk * munk;
		muk *= mu;
		munk /= (1-mu);
		while (nn >= 1) {
			blend *= nn;
			nn--;
			if (kn > 1) {
				blend /= (double)kn;
				kn--;
			}
			if (nkn > 1) {
				blend /= (double)nkn;
				nkn--;
			}
		}
		pt_out->x += gf_mulfix(pts[k].x, FLT2FIX(blend));
		pt_out->y += gf_mulfix(pts[k].y, FLT2FIX(blend));
	}
}

static void gf_add_n_bezier(GF_Path *gp, GF_Point2D *newpts, u32 nbPoints)
{
	Double mu;
	u32 numPoints, i;
	GF_Point2D end;
	numPoints = (u32) FIX2INT(GF_2D_DEFAULT_RES * gp->fineness);
	mu = 0.0;
	if (numPoints) mu = 1/(Double)numPoints;
	for (i=1; i<numPoints; i++) {
		NBezier(newpts, nbPoints - 1, i*mu, &end);
		gf_path_add_line_to(gp, end.x, end.y);
	}
	gf_path_add_line_to(gp, newpts[nbPoints-1].x, newpts[nbPoints-1].y);
}

GF_EXPORT
GF_Err gf_path_add_bezier(GF_Path *gp, GF_Point2D *pts, u32 nbPoints)
{
	GF_Point2D *newpts;
	if (!gp->n_points) return GF_BAD_PARAM;

	newpts = (GF_Point2D *) gf_malloc(sizeof(GF_Point2D) * (nbPoints+1));
	newpts[0] = gp->points[gp->n_points-1];
	memcpy(&newpts[1], pts, sizeof(GF_Point2D) * nbPoints);

	gf_add_n_bezier(gp, newpts, nbPoints + 1);

	gf_free(newpts);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_path_add_arc_to(GF_Path *gp, Fixed end_x, Fixed end_y, Fixed fa_x, Fixed fa_y, Fixed fb_x, Fixed fb_y, Bool cw)
{
	GF_Matrix2D mat, inv;
	Fixed angle, start_angle, end_angle, sweep, axis_w, axis_h, tmp, cx, cy, _vx, _vy, start_x, start_y;
	s32 i, num_steps;

	if (!gp->n_points) return GF_BAD_PARAM;

	start_x = gp->points[gp->n_points-1].x;
	start_y = gp->points[gp->n_points-1].y;

	cx = (fb_x + fa_x)/2;
	cy = (fb_y + fa_y)/2;

	angle = gf_atan2(fb_y-fa_y, fb_x-fa_x);
	gf_mx2d_init(mat);
	gf_mx2d_add_rotation(&mat, 0, 0, angle);
	gf_mx2d_add_translation(&mat, cx, cy);

	gf_mx2d_copy(inv, mat);
	gf_mx2d_inverse(&inv);
	gf_mx2d_apply_coords(&inv, &start_x, &start_y);
	gf_mx2d_apply_coords(&inv, &end_x, &end_y);
	gf_mx2d_apply_coords(&inv, &fa_x, &fa_y);
	gf_mx2d_apply_coords(&inv, &fb_x, &fb_y);

	//start angle and end angle
	start_angle = gf_atan2(start_y, start_x);
	end_angle = gf_atan2(end_y, end_x);
	tmp = gf_mulfix((start_x - fa_x), (start_x - fa_x)) + gf_mulfix((start_y - fa_y), (start_y - fa_y));
	axis_w = gf_sqrt(tmp);
	tmp = gf_mulfix((start_x - fb_x) , (start_x - fb_x)) + gf_mulfix((start_y - fb_y), (start_y - fb_y));
	axis_w += gf_sqrt(tmp);
	axis_w /= 2;
	axis_h = gf_sqrt(gf_mulfix(axis_w, axis_w) - gf_mulfix(fa_x,fa_x));
	sweep = end_angle - start_angle;

	if (cw) {
		if (sweep>0) sweep -= 2*GF_PI;
	} else {
		if (sweep<0) sweep += 2*GF_PI;
	}
	num_steps = GF_2D_DEFAULT_RES/2;
	for (i=1; i<=num_steps; i++) {
		angle = start_angle + sweep*i/num_steps;
		_vx = gf_mulfix(axis_w, gf_cos(angle));
		_vy = gf_mulfix(axis_h, gf_sin(angle));
		/*re-invert*/
		gf_mx2d_apply_coords(&mat, &_vx, &_vy);
		gf_path_add_line_to(gp, _vx, _vy);
	}
	return GF_OK;
}



GF_EXPORT
GF_Err gf_path_add_svg_arc_to(GF_Path *gp, Fixed end_x, Fixed end_y, Fixed r_x, Fixed r_y, Fixed x_axis_rotation, Bool large_arc_flag, Bool sweep_flag)
{
	Fixed start_x, start_y;
	Fixed xmid,ymid;
	Fixed xmidp,ymidp;
	Fixed xmidpsq,ymidpsq;
	Fixed phi, cos_phi, sin_phi;
	Fixed c_x, c_y;
	Fixed cxp, cyp;
	Fixed scale;
	Fixed rxsq, rysq;
	Fixed start_angle, sweep_angle;
	Fixed radius_scale;
	Fixed vx, vy, normv;
	Fixed ux, uy, normu;
	Fixed sign;
	u32 i, num_steps;

	if (!gp->n_points) return GF_BAD_PARAM;

	if (!r_x || !r_y) {
		gf_path_add_line_to(gp, end_x, end_y);
		return GF_OK;
	}

	if (r_x < 0) r_x = -r_x;
	if (r_y < 0) r_y = -r_y;

	start_x = gp->points[gp->n_points-1].x;
	start_y = gp->points[gp->n_points-1].y;

	phi = gf_mulfix(x_axis_rotation/180, GF_PI);
	cos_phi = gf_cos(phi);
	sin_phi = gf_sin(phi);
	xmid = (start_x - end_x)/2;
	ymid = (start_y - end_y)/2;
	if (!xmid && !ymid) {
		gf_path_add_line_to(gp, end_x, end_y);
		return GF_OK;
	}

	xmidp = gf_mulfix(cos_phi, xmid) + gf_mulfix(sin_phi, ymid);
	ymidp = gf_mulfix(-sin_phi, xmid) + gf_mulfix(cos_phi, ymid);
	xmidpsq = gf_mulfix(xmidp, xmidp);
	ymidpsq = gf_mulfix(ymidp, ymidp);

	rxsq = gf_mulfix(r_x, r_x);
	rysq = gf_mulfix(r_y, r_y);
	assert(rxsq && rysq);

	radius_scale = gf_divfix(xmidpsq, rxsq) + gf_divfix(ymidpsq, rysq);
	if (radius_scale > FIX_ONE) {
		r_x = gf_mulfix(gf_sqrt(radius_scale), r_x);
		r_y = gf_mulfix(gf_sqrt(radius_scale), r_y);
		rxsq = gf_mulfix(r_x, r_x);
		rysq = gf_mulfix(r_y, r_y);
	}

#if 0
	/* Old code with overflow problems in fixed point,
	   sign was sometimes negative (cf tango SVG icons appointment-new.svg)*/

	sign = gf_mulfix(rxsq,ymidpsq) + gf_mulfix(rysq, xmidpsq);
	scale = FIX_ONE;
	/*FIXME - what if scale is 0 ??*/
	if (sign) scale = gf_divfix(
		                      (gf_mulfix(rxsq,rysq) - gf_mulfix(rxsq, ymidpsq) - gf_mulfix(rysq,xmidpsq)),
		                      sign
		                  );
#else
	/* New code: the sign variable computation is split into simpler cases and
	   the expression is divided by rxsq to reduce the range */
	if ((rxsq == 0 || ymidpsq ==0) && (rysq == 0 || xmidpsq == 0)) {
		scale = FIX_ONE;
	} else if (rxsq == 0 || ymidpsq ==0) {
		scale = gf_divfix(rxsq,xmidpsq) - FIX_ONE;
	} else if (rysq == 0 || xmidpsq == 0) {
		scale = gf_divfix(rysq,ymidpsq) - FIX_ONE;
	} else {
		Fixed tmp;
		tmp = gf_mulfix(gf_divfix(rysq, rxsq), xmidpsq);
		sign = ymidpsq + tmp;
		scale = gf_divfix((rysq - ymidpsq - tmp),sign);
	}
#endif
	/* precision problem may lead to negative value around zero, we need to take care of it before sqrt */
	scale = gf_sqrt(ABS(scale));

	cxp = gf_mulfix(scale, gf_divfix(gf_mulfix(r_x, ymidp),r_y));
	cyp = gf_mulfix(scale, -gf_divfix(gf_mulfix(r_y, xmidp),r_x));
	cxp = (large_arc_flag == sweep_flag ? - cxp : cxp);
	cyp = (large_arc_flag == sweep_flag ? - cyp : cyp);

	c_x = gf_mulfix(cos_phi, cxp) - gf_mulfix(sin_phi, cyp) + (start_x + end_x)/2;
	c_y = gf_mulfix(sin_phi, cxp) + gf_mulfix(cos_phi, cyp) + (start_y + end_y)/2;

	vx = gf_divfix(xmidp-cxp,r_x);
	vy = gf_divfix(ymidp-cyp,r_y);
	normv = gf_sqrt(gf_mulfix(vx, vx) + gf_mulfix(vy,vy));

	sign = vy;
	start_angle = gf_acos(gf_divfix(vx,normv));
	start_angle = (sign > 0 ? start_angle: -start_angle);

	ux = vx;
	uy = vy;

	vx = gf_divfix(-xmidp-cxp,r_x);
	vy = gf_divfix(-ymidp-cyp,r_y);
	normu = gf_sqrt(gf_mulfix(ux, ux) + gf_mulfix(uy,uy));

	sign = gf_mulfix(ux, vy) - gf_mulfix(uy, vx);
	sweep_angle = gf_divfix( gf_mulfix(ux,vx) + gf_mulfix(uy, vy), gf_mulfix(normu, normv) );
	/*numerical stability safety*/
	sweep_angle = MAX(-FIX_ONE, MIN(sweep_angle, FIX_ONE));
	sweep_angle = gf_acos(sweep_angle);
	sweep_angle = (sign > 0 ? sweep_angle: -sweep_angle);
	if (sweep_flag == 0) {
		if (sweep_angle > 0) sweep_angle -= GF_2PI;
	} else {
		if (sweep_angle < 0) sweep_angle += GF_2PI;
	}

	num_steps = GF_2D_DEFAULT_RES/2;
	for (i=1; i<=num_steps; i++) {
		Fixed _vx, _vy;
		Fixed _vxp, _vyp;
		Fixed angle = start_angle + sweep_angle*(s32)i/(s32)num_steps;
		_vx = gf_mulfix(r_x, gf_cos(angle));
		_vy = gf_mulfix(r_y, gf_sin(angle));
		_vxp = gf_mulfix(cos_phi, _vx) - gf_mulfix(sin_phi, _vy) + c_x;
		_vyp = gf_mulfix(sin_phi, _vx) + gf_mulfix(cos_phi, _vy) + c_y;
		gf_path_add_line_to(gp, _vxp, _vyp);
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_path_add_arc(GF_Path *gp, Fixed radius, Fixed start_angle, Fixed end_angle, GF_Path2DArcCloseType close_type)
{
	GF_Err e;
	Fixed _vx, _vy, step, cur;
	s32 i, do_run;

	step = (end_angle - start_angle) / (GF_2D_DEFAULT_RES);
	radius *= 2;

	/*pie*/
	i=0;
	if (close_type==GF_PATH2D_ARC_PIE) {
		gf_path_add_move_to(gp, 0, 0);
		i=1;
	}
	do_run = 1;
	cur=start_angle;
	while (do_run) {
		if (cur>=end_angle) {
			do_run = 0;
			cur = end_angle;
		}
		_vx = gf_mulfix(radius, gf_cos(cur));
		_vy = gf_mulfix(radius, gf_sin(cur));
		if (!i) {
			e = gf_path_add_move_to(gp, _vx, _vy);
			i = 1;
		} else {
			e = gf_path_add_line_to(gp, _vx, _vy);
		}
		if (e) return e;
		cur+=step;
	}
	if (close_type!=GF_PATH2D_ARC_OPEN) e = gf_path_close(gp);
	return e;
}


GF_EXPORT
GF_Err gf_path_get_control_bounds(GF_Path *gp, GF_Rect *rc)
{
	GF_Point2D *pt, *end;
	Fixed xMin, xMax, yMin, yMax;
	if (!gp || !rc) return GF_BAD_PARAM;

	if (!gp->n_points) {
		rc->x = rc->y = rc->width = rc->height = 0;
		return GF_OK;
	}
	pt = gp->points;
	end = pt + gp->n_points;
	xMin = xMax = pt->x;
	yMin = yMax = pt->y;
	pt++;
	for ( ; pt < end; pt++ ) {
		Fixed v;
		v = pt->x;
		if (v < xMin) xMin = v;
		if (v > xMax) xMax = v;
		v = pt->y;
		if (v < yMin) yMin = v;
		if (v > yMax) yMax = v;
	}
	rc->x = xMin;
	rc->y = yMax;
	rc->width = xMax - xMin;
	rc->height = yMax - yMin;

#if 0
	/*take care of straight line path by adding a default width if height and vice-versa*/
	if (rc->height && !rc->width) {
		rc->width = 2*FIX_ONE;
		rc->x -= FIX_ONE;
	}
	else if (!rc->height && rc->width) {
		rc->height = 2*FIX_ONE;
		rc->y += FIX_ONE;
	}
#endif
	return GF_OK;
}

/*
 *	conic bbox computing taken from freetype
 *		Copyright 1996-2001, 2002, 2004 by
 *		David Turner, Robert Wilhelm, and Werner Lemberg.
 *	License: FTL or GPL
 */
static void gf_conic_check(Fixed y1, Fixed y2, Fixed y3, Fixed *min, Fixed *max)
{
	/* flat arc */
	if ((y1 <= y3) && (y2 == y1)) goto Suite;

	if ( y1 < y3 ) {
		/* ascending arc */
		if ((y2 >= y1) && (y2 <= y3)) goto Suite;
	} else {
		/* descending arc */
		if ((y2 >= y3) && (y2 <= y1)) {
			y2 = y1;
			y1 = y3;
			y3 = y2;
			goto Suite;
		}
	}
	y1 = y3 = y1 - gf_muldiv(y2 - y1, y2 - y1, y1 - 2*y2 + y3);

Suite:
	if ( y1 < *min ) *min = y1;
	if ( y3 > *max ) *max = y3;
}


/*
 *	cubic bbox computing taken from freetype
 *		Copyright 1996-2001, 2002, 2004 by
 *		David Turner, Robert Wilhelm, and Werner Lemberg.
 *	License: FTL or GPL

 *	Note: we're using the decomposition method, not the equation one which is not usable with Floats (only 16.16 fixed)
*/
static void gf_cubic_check(Fixed p1, Fixed p2, Fixed p3, Fixed p4, Fixed *min, Fixed *max)
{
	Fixed stack[32*3 + 1], *arc;
	arc = stack;
	arc[0] = p1;
	arc[1] = p2;
	arc[2] = p3;
	arc[3] = p4;

	do {
		Fixed y1 = arc[0];
		Fixed y2 = arc[1];
		Fixed y3 = arc[2];
		Fixed y4 = arc[3];

		if (ABS(y1)<FIX_EPSILON) arc[0] = y1 = 0;
		if (ABS(y2)<FIX_EPSILON) arc[1] = y2 = 0;
		if (ABS(y3)<FIX_EPSILON) arc[2] = y3 = 0;
		if (ABS(y4)<FIX_EPSILON) arc[3] = y4 = 0;

		if ( y1 == y4 ) {
			/* flat */
			if ((y1 == y2) && (y1 == y3)) goto Test;
		}
		else if ( y1 < y4 ) {
			/* ascending */
			if ((y2 >= y1) && (y2 <= y4) && (y3 >= y1) && (y3 <= y4)) goto Test;
		} else {
			/* descending */
			if ((y2 >= y4) && (y2 <= y1) && (y3 >= y4) && (y3 <= y1)) {
				y2 = y1;
				y1 = y4;
				y4 = y2;
				goto Test;
			}
		}
		/* unknown direction -- split the arc in two */
		arc[6] = y4;
		arc[1] = y1 = ( y1 + y2 ) / 2;
		arc[5] = y4 = ( y4 + y3 ) / 2;
		y2 = ( y2 + y3 ) / 2;
		arc[2] = y1 = ( y1 + y2 ) / 2;
		arc[4] = y4 = ( y4 + y2 ) / 2;
		arc[3] = ( y1 + y4 ) / 2;

		arc += 3;
		goto Suite;

Test:
		if ( y1 < *min ) *min = y1;
		if ( y4 > *max ) *max = y4;
		arc -= 3;
Suite:
		;
	}
	while ( arc >= stack );
}


GF_EXPORT
GF_Err gf_path_get_bounds(GF_Path *gp, GF_Rect *rc)
{
	u32 i;
	GF_Point2D *pt, *end;
	Fixed xMin, xMax, yMin, yMax, cxMin, cxMax, cyMin, cyMax;
	if (!gp || !rc) return GF_BAD_PARAM;

	if (!(gp->flags & GF_PATH_BBOX_DIRTY)) {
		*rc = gp->bbox;
		return GF_OK;
	}
	/*no curves in path*/
	if (gp->flags & GF_PATH_FLATTENED) {
		GF_Err e;
		gp->flags &= ~GF_PATH_BBOX_DIRTY;
		e = gf_path_get_control_bounds(gp, &gp->bbox);
		*rc = gp->bbox;
		return e;
	}

	gp->flags &= ~GF_PATH_BBOX_DIRTY;

	if (!gp->n_points) {
		gp->bbox.x = gp->bbox.y = gp->bbox.width = gp->bbox.height = 0;
		*rc = gp->bbox;
		return GF_OK;
	}
	pt = gp->points;
	xMin = xMax = cxMin = cxMax = pt->x;
	yMin = yMax = cyMin = cyMax = pt->y;
	pt++;
	for (i=1 ; i < gp->n_points; i++ ) {
		Fixed x, y;
		x = pt->x;
		y = pt->y;
		if (x < cxMin) cxMin = x;
		if (x > cxMax) cxMax = x;
		if (y < cyMin) cyMin = y;
		if (y > cyMax) cyMax = y;
		/*point on curve, update*/
		if (gp->tags[i] & GF_PATH_CURVE_ON) {
			if (x < xMin) xMin = x;
			if (x > xMax) xMax = x;
			if (y < yMin) yMin = y;
			if (y > yMax) yMax = y;
		}
		pt++;
	}

	/*control box is bigger than box , decompose curves*/
	if ((cxMin < xMin) || (cxMax > xMax) || (cyMin < yMin) || (cyMax > yMax)) {
		GF_Point2D *ctrl1, *ctrl2;
		/*decompose all control points*/
		pt = gp->points;
		for (i=1 ; i < gp->n_points; ) {
			switch (gp->tags[i]) {
			case GF_PATH_CURVE_ON:
			case GF_PATH_CLOSE:
				pt = &gp->points[i];
				i++;
				break;
			case GF_PATH_CURVE_CONIC:
				/*decompose*/
				ctrl1 = &gp->points[i];
				end = &gp->points[i+1];
				if ((ctrl1->x < xMin) || (ctrl1->x > xMax))
					gf_conic_check(pt->x, ctrl1->x, end->x, &xMin, &xMax);

				if ((ctrl1->y < yMin) || (ctrl1->y > yMax))
					gf_conic_check(pt->y, ctrl1->y, end->y, &yMin, &yMax);

				/*and move*/
				pt = end;
				i+=2;
				break;
			case GF_PATH_CURVE_CUBIC:
				/*decompose*/
				ctrl1 = &gp->points[i];
				ctrl2 = &gp->points[i+1];
				end = &gp->points[i+2];
				if ((ctrl1->x < xMin) || (ctrl1->x > xMax) || (ctrl2->x < xMin) || (ctrl2->x > xMax))
					gf_cubic_check(pt->x, ctrl1->x, ctrl2->x, end->x, &xMin, &xMax);

				if ((ctrl1->y < yMin) || (ctrl1->y > yMax) || (ctrl2->y < yMin) || (ctrl2->y > yMax))
					gf_cubic_check(pt->y, ctrl1->y, ctrl2->y, end->y, &yMin, &yMax);

				/*and move*/
				pt = end;
				i+=3;
				break;
			}
		}
	}
	gp->bbox.x = xMin;
	gp->bbox.y = yMax;
	gp->bbox.width = xMax - xMin;
	gp->bbox.height = yMax - yMin;
	*rc = gp->bbox;
	return GF_OK;
}

/*flattening algo taken from libart but passed to sqrt tests for line distance to avoid 16.16 fixed overflow*/
static GF_Err gf_subdivide_cubic(GF_Path *gp, Fixed x0, Fixed y0, Fixed x1, Fixed y1, Fixed x2, Fixed y2, Fixed x3, Fixed y3, Fixed fineness)
{
	GF_Point2D pt;
	Fixed x3_0, y3_0, z3_0, z1_0, z1_dot, z2_dot, z1_perp, z2_perp;
	Fixed max_perp;
	Fixed x_m, y_m, xa1, ya1, xa2, ya2, xb1, yb1, xb2, yb2;
	GF_Err e;

	pt.x = x3_0 = x3 - x0;
	pt.y = y3_0 = y3 - y0;

	/*z3_0 is dist z0-z3*/
	z3_0 = gf_v2d_len(&pt);

	pt.x = x1 - x0;
	pt.y = y1 - y0;
	z1_0 = gf_v2d_len(&pt);

	if ((z3_0*100 < FIX_ONE) && (z1_0*100 < FIX_ONE))
		goto nosubdivide;

	/* perp is distance from line, multiplied by dist z0-z3 */
	max_perp = gf_mulfix(fineness, z3_0);

	z1_perp = gf_mulfix((y1 - y0), x3_0) - gf_mulfix((x1 - x0), y3_0);
	if (ABS(z1_perp) > max_perp)
		goto subdivide;

	z2_perp = gf_mulfix((y3 - y2), x3_0) - gf_mulfix((x3 - x2), y3_0);
	if (ABS(z2_perp) > max_perp)
		goto subdivide;

	z1_dot = gf_mulfix((x1 - x0), x3_0) + gf_mulfix((y1 - y0), y3_0);
	if ((z1_dot < 0) && (ABS(z1_dot) > max_perp))
		goto subdivide;

	z2_dot = gf_mulfix((x3 - x2), x3_0) + gf_mulfix((y3 - y2), y3_0);
	if ((z2_dot < 0) && (ABS(z2_dot) > max_perp))
		goto subdivide;

	if (gf_divfix(z1_dot + z1_dot, z3_0) > z3_0)
		goto subdivide;

	if (gf_divfix(z2_dot + z2_dot, z3_0) > z3_0)
		goto subdivide;

nosubdivide:
	/* don't subdivide */
	return gf_path_add_line_to(gp, x3, y3);

subdivide:
	xa1 = (x0 + x1) / 2;
	ya1 = (y0 + y1) / 2;
	xa2 = (x0 + 2 * x1 + x2) / 4;
	ya2 = (y0 + 2 * y1 + y2) / 4;
	xb1 = (x1 + 2 * x2 + x3) / 4;
	yb1 = (y1 + 2 * y2 + y3) / 4;
	xb2 = (x2 + x3) / 2;
	yb2 = (y2 + y3) / 2;
	x_m = (xa2 + xb1) / 2;
	y_m = (ya2 + yb1) / 2;
	/*safeguard for numerical stability*/
	if ( (ABS(x_m-x0) < FIX_EPSILON) && (ABS(y_m-y0) < FIX_EPSILON))
		return gf_path_add_line_to(gp, x3, y3);
	if ( (ABS(x3-x_m) < FIX_EPSILON) && (ABS(y3-y_m) < FIX_EPSILON))
		return gf_path_add_line_to(gp, x3, y3);

	e = gf_subdivide_cubic(gp, x0, y0, xa1, ya1, xa2, ya2, x_m, y_m, fineness);
	if (e) return e;
	return gf_subdivide_cubic(gp, x_m, y_m, xb1, yb1, xb2, yb2, x3, y3, fineness);
}

GF_EXPORT
GF_Path *gf_path_get_flatten(GF_Path *gp)
{
	GF_Path *ngp;
	Fixed fineness;
	u32 i, *countour;
	GF_Point2D *pt;
	if (!gp || !gp->n_points) return NULL;

	if (gp->flags & GF_PATH_FLATTENED) return gf_path_clone(gp);

	/*avoid too high precision */
	fineness = MAX(FIX_ONE - gp->fineness, FIX_ONE / 100);

	ngp = gf_path_new();
	pt = &gp->points[0];
	gf_path_add_move_to_vec(ngp, pt);
	countour = gp->contours;
	for (i=1; i<gp->n_points; ) {
		switch (gp->tags[i]) {
		case GF_PATH_CURVE_ON:
		case GF_PATH_CLOSE:
			pt = &gp->points[i];
			if (*countour == i-1) {
				gf_path_add_move_to_vec(ngp, pt);
				countour++;
			} else {
				gf_path_add_line_to_vec(ngp, pt);
			}
			if (gp->tags[i]==GF_PATH_CLOSE) gf_path_close(ngp);
			i++;
			break;
		case GF_PATH_CURVE_CONIC:
		{
			GF_Point2D *ctl, *end, c1, c2;
			ctl = &gp->points[i];
			end = &gp->points[i+1];
			c1.x = pt->x + 2*(ctl->x - pt->x)/3;
			c1.y = pt->y + 2*(ctl->y - pt->y)/3;
			c2.x = c1.x + (end->x - pt->x) / 3;
			c2.y = c1.y + (end->y - pt->y) / 3;

			gf_subdivide_cubic(ngp, pt->x, pt->y, c1.x, c1.y, c2.x, c2.y, end->x, end->y, fineness);
			pt = end;
			if (gp->tags[i+1]==GF_PATH_CLOSE) gf_path_close(ngp);
			i+=2;
		}
		break;
		case GF_PATH_CURVE_CUBIC:
			gf_subdivide_cubic(ngp, pt->x, pt->y, gp->points[i].x, gp->points[i].y, gp->points[i+1].x, gp->points[i+1].y, gp->points[i+2].x, gp->points[i+2].y, fineness);
			pt = &gp->points[i+2];
			if (gp->tags[i+2]==GF_PATH_CLOSE) gf_path_close(ngp);
			i+=3;
			break;
		}
	}
	if (gp->flags & GF_PATH_FILL_ZERO_NONZERO) ngp->flags |= GF_PATH_FILL_ZERO_NONZERO;
	ngp->flags |= (GF_PATH_BBOX_DIRTY | GF_PATH_FLATTENED);
	return ngp;
}

GF_EXPORT
void gf_path_flatten(GF_Path *gp)
{
	GF_Path *res;
	u32 flags = gp->flags;
	if (gp->flags & GF_PATH_FLATTENED) return;
	if (!gp->n_points) return;
	res = gf_path_get_flatten(gp);
	if (gp->contours) gf_free(gp->contours);
	if (gp->points) gf_free(gp->points);
	if (gp->tags) gf_free(gp->tags);
	memcpy(gp, res, sizeof(GF_Path));
	gp->flags |= (flags & (GF_PATH_FILL_ZERO_NONZERO|GF_PATH_FILL_EVEN));
	gf_free(res);
}



#define isLeft(P0, P1, P2) \
	( gf_mulfix((P1.x - P0.x), (P2.y - P0.y)) - gf_mulfix((P2.x - P0.x), (P1.y - P0.y)) )


static void gf_subdivide_cubic_hit_test(Fixed h_x, Fixed h_y, Fixed x0, Fixed y0, Fixed x1, Fixed y1, Fixed x2, Fixed y2, Fixed x3, Fixed y3, s32 *wn)
{
	GF_Point2D s, e, pt;
	Fixed x_m, y_m, xa1, ya1, xa2, ya2, xb1, yb1, xb2, yb2, y_min, y_max;

	/*if hit line doesn't intersects control box abort*/
	y_min = MIN(y0, MIN(y1, MIN(y2, y3)));
	y_max = MAX(y0, MAX(y1, MAX(y2, y3)));
	if ((h_y<y_min) || (h_y>y_max) ) return;

	/*if vert diff between end points larger than 1 pixels, subdivide (we need pixel accuracy for is_over)*/
	if (y_max - y_min > FIX_ONE) {
		xa1 = (x0 + x1) / 2;
		ya1 = (y0 + y1) / 2;
		xa2 = (x0 + 2 * x1 + x2) / 4;
		ya2 = (y0 + 2 * y1 + y2) / 4;
		xb1 = (x1 + 2 * x2 + x3) / 4;
		yb1 = (y1 + 2 * y2 + y3) / 4;
		xb2 = (x2 + x3) / 2;
		yb2 = (y2 + y3) / 2;
		x_m = (xa2 + xb1) / 2;
		y_m = (ya2 + yb1) / 2;

		gf_subdivide_cubic_hit_test(h_x, h_y, x0, y0, xa1, ya1, xa2, ya2, x_m, y_m, wn);
		gf_subdivide_cubic_hit_test(h_x, h_y, x_m, y_m, xb1, yb1, xb2, yb2, x3, y3, wn);
		return;
	}

	s.x = x0;
	s.y = y0;
	e.x = x3;
	e.y = y3;
	pt.x = h_x;
	pt.y= h_y;

	if (s.y<=pt.y) {
		if (e.y>pt.y) {
			if (isLeft(s, e, pt) > 0)
				(*wn)++;
		}
	}
	else if (e.y<=pt.y) {
		if (isLeft(s, e, pt) < 0)
			(*wn)--;
	}
}

GF_EXPORT
Bool gf_path_point_over(GF_Path *gp, Fixed x, Fixed y)
{
	u32 i, *contour, start_idx;
	s32 wn;
	GF_Point2D start, s, e, pt;
	GF_Rect rc;
	GF_Err err;

	/*check if not in bounds*/
	err = gf_path_get_bounds(gp, &rc);
	if (err) return GF_FALSE;
	if ((x<rc.x) || (y>rc.y) || (x>rc.x+rc.width) || (y<rc.y-rc.height)) return GF_FALSE;

	if (!gp || (gp->n_points<2)) return GF_FALSE;

	pt.x = x;
	pt.y = y;
	wn = 0;
	s = start = gp->points[0];
	start_idx = 0;
	contour = gp->contours;
	for (i=1; i<gp->n_points; ) {
		switch (gp->tags[i]) {
		case GF_PATH_CURVE_ON:
		case GF_PATH_CLOSE:
			e = gp->points[i];
			if (s.y<=pt.y) {
				if (e.y>pt.y) {
					if (isLeft(s, e, pt) > 0) wn++;
				}
			}
			else if (e.y<=pt.y) {
				if (isLeft(s, e, pt) < 0) wn--;
			}
			s = e;
			i++;
			break;
		case GF_PATH_CURVE_CONIC:
		{
			GF_Point2D *ctl, *end, c1, c2;
			ctl = &gp->points[i];
			end = &gp->points[i+1];
			c1.x = s.x + 2*(ctl->x - s.x) / 3;
			c1.y = s.y + 2*(ctl->y - s.y) / 3;
			c2.x = c1.x + (end->x - s.x) / 3;
			c2.y = c1.y + (end->y - s.y) / 3;
			gf_subdivide_cubic_hit_test(x, y, s.x, s.y, c1.x, c1.y, c2.x, c2.y, end->x, end->y, &wn);
			s = *end;
		}
		i+=2;
		break;
		case GF_PATH_CURVE_CUBIC:
			gf_subdivide_cubic_hit_test(x, y, s.x, s.y, gp->points[i].x, gp->points[i].y, gp->points[i+1].x, gp->points[i+1].y, gp->points[i+2].x, gp->points[i+2].y, &wn);
			s = gp->points[i+2];
			i+=3;
			break;
		}
		/*end of subpath*/
		if (*contour==i-1) {
			/*close path if needed, but don't test for lines...*/
			if ((i-start_idx > 2) && (pt.y<s.y)) {
				if ((start.x != s.x) || (start.y != s.y)) {
					e = start;
					if (s.x<=pt.x) {
						if (e.y>pt.y) {
							if (isLeft(s, e, pt) > 0) wn++;
						}
					}
					else if (e.y<=pt.y) {
						if (isLeft(s, e, pt) < 0) wn--;
					}
				}
			}
			if ( i < gp->n_points )
				s = start = gp->points[i];
			i++;
		}
	}
	if (gp->flags & GF_PATH_FILL_ZERO_NONZERO) return wn ? GF_TRUE : GF_FALSE;
	return (wn%2) ? GF_TRUE : GF_FALSE;
}

GF_EXPORT
Bool gf_path_is_empty(GF_Path *gp)
{
	if (gp && gp->contours) return GF_FALSE;
	return GF_TRUE;
}

/*iteration info*/
typedef struct
{
	Fixed len;
	Fixed dx, dy;
	Fixed start_x, start_y;
} IterInfo;

struct _path_iterator
{
	u32 num_seg;
	IterInfo *seg;
	Fixed length;
};

GF_EXPORT
GF_PathIterator *gf_path_iterator_new(GF_Path *gp)
{
	GF_Path *flat;
	GF_PathIterator *it;
	u32 i, j, cur;
	GF_Point2D start, end;

	GF_SAFEALLOC(it, GF_PathIterator);
	if (!it) return NULL;
	flat = gf_path_get_flatten(gp);
	if (!flat) {
		gf_free(it);
		return NULL;
	}
	it->seg = (IterInfo *) gf_malloc(sizeof(IterInfo) * flat->n_points);
	it->num_seg = 0;
	it->length = 0;
	cur = 0;
	for (i=0; i<flat->n_contours; i++) {
		Fixed dx, dy;
		u32 nb_pts = 1+flat->contours[i]-cur;
		start = flat->points[cur];
		for (j=1; j<nb_pts; j++) {
			end = flat->points[cur+j];
			it->seg[it->num_seg].start_x = start.x;
			it->seg[it->num_seg].start_y = start.y;
			dx = it->seg[it->num_seg].dx = end.x - start.x;
			dy = it->seg[it->num_seg].dy = end.y - start.y;
			it->seg[it->num_seg].len = gf_sqrt(gf_mulfix(dx, dx) + gf_mulfix(dy, dy));
			it->length += it->seg[it->num_seg].len;
			start = end;
			it->num_seg++;
		}
		cur += nb_pts;
	}
	gf_path_del(flat);
	return it;
}

GF_EXPORT
Fixed gf_path_iterator_get_length(GF_PathIterator *it)
{
	return it ? it->length : 0;
}

GF_EXPORT
Bool gf_path_iterator_get_transform(GF_PathIterator *path, Fixed offset, Bool follow_tangent, GF_Matrix2D *mat, Bool smooth_edges, Fixed length_after_point)
{
	GF_Matrix2D final, rot;
	Bool tang = GF_FALSE;
	Fixed res, angle, angleNext;
	u32 i;
	Fixed curLen = 0;
	if (!path) return GF_FALSE;

	for (i=0; i<path->num_seg; i++) {
		if (curLen + path->seg[i].len >= offset) goto found;
		curLen += path->seg[i].len;
	}
	if (!follow_tangent) return GF_FALSE;
	tang = GF_TRUE;
	i--;

found:
	gf_mx2d_init(final);

	res = gf_divfix(offset - curLen, path->seg[i].len);
	if (tang) res += 1;

	/*move to current point*/
	gf_mx2d_add_translation(&final, path->seg[i].start_x + gf_mulfix(path->seg[i].dx, res), path->seg[i].start_y + gf_mulfix(path->seg[i].dy, res));

	if (!path->seg[i].dx) {
		angle = GF_PI2;
	} else {
		angle = gf_acos( gf_divfix(path->seg[i].dx , path->seg[i].len) );
	}
	if (path->seg[i].dy<0) angle *= -1;

	if (smooth_edges) {
		if (offset + length_after_point > curLen + path->seg[i].len) {
			Fixed ratio = gf_divfix(curLen + path->seg[i].len-offset, length_after_point);
			if (i < path->num_seg - 1) {
				if (!path->seg[i+1].dx) {
					angleNext = GF_PI2;
				} else {
					angleNext = gf_acos( gf_divfix(path->seg[i+1].dx, path->seg[i+1].len) );
				}
				if (path->seg[i+1].dy<0) angleNext *= -1;

				if (angle<0 && angleNext>0) {
					angle = gf_mulfix(FIX_ONE-ratio, angleNext) - gf_mulfix(ratio, angle);
				} else {
					angle = gf_mulfix(ratio, angle) + gf_mulfix(FIX_ONE-ratio, angleNext);
				}
			}
		}
	}
	/*handle res=0 case for rotation (point on line join)*/
	else if (res==1) {
		if (i < path->num_seg - 1) {
			if (!path->seg[i+1].dx) {
				angleNext = GF_PI2;
			} else {
				angleNext = gf_acos( gf_divfix(path->seg[i+1].dx, path->seg[i+1].len) );
			}
			if (path->seg[i+1].dy<0) angleNext *= -1;
			angle = ( angle + angleNext) / 2;
		}
	}

	gf_mx2d_init(rot);
	gf_mx2d_add_rotation(&rot, 0, 0, angle);
	gf_mx2d_add_matrix(mat, &rot);
	gf_mx2d_add_matrix(mat, &final);
	return GF_TRUE;
}

GF_EXPORT
void gf_path_iterator_del(GF_PathIterator *it)
{
	if (it->seg) gf_free(it->seg);
	gf_free(it);
}


#define ConvexCompare(delta)	\
    ( (delta.x > 0) ? -1 :		\
      (delta.x < 0) ?	1 :		\
      (delta.y > 0) ? -1 :		\
      (delta.y < 0) ?	1 :	\
      0 )

#define ConvexGetPointDelta(delta, pprev, pcur )			\
    /* Given a previous point 'pprev', read a new point into 'pcur' */	\
    /* and return delta in 'delta'.				    */	\
    pcur = pts[iread++];						\
    delta.x = pcur.x - pprev.x;					\
    delta.y = pcur.y - pprev.y;					\
 
#define ConvexCross(p, q) gf_mulfix(p.x,q.y) - gf_mulfix(p.y,q.x);

#define ConvexCheckTriple						\
    if ( (thisDir = ConvexCompare(dcur)) == -curDir ) {			\
	  ++dirChanges;							\
	  /* if ( dirChanges > 2 ) return NotConvex;		     */ \
    }									\
    curDir = thisDir;							\
    cross = ConvexCross(dprev, dcur);					\
    if ( cross > 0 ) { \
		if ( angleSign == -1 ) return GF_POLYGON_COMPLEX_CW;		\
		angleSign = 1;					\
	}							\
    else if (cross < 0) {	\
		if (angleSign == 1) return GF_POLYGON_COMPLEX_CCW;		\
		angleSign = -1;				\
	}						\
    pSecond = pThird;		\
    dprev.x = dcur.x;		\
    dprev.y = dcur.y;							\
 
GF_EXPORT
u32 gf_polygone2d_get_convexity(GF_Point2D *pts, u32 len)
{
	s32 curDir, thisDir = 0, dirChanges = 0, angleSign = 0;
	u32 iread;
	Fixed cross;
	GF_Point2D pSecond, pThird, pSaveSecond;
	GF_Point2D dprev, dcur;

	/* Get different point, return if less than 3 diff points. */
	if (len < 3 ) return GF_POLYGON_CONVEX_LINE;
	iread = 1;
	ConvexGetPointDelta(dprev, (pts[0]), pSecond);
	pSaveSecond = pSecond;
	/*initial direction */
	curDir = ConvexCompare(dprev);
	while ( iread < len) {
		/* Get different point, break if no more points */
		ConvexGetPointDelta(dcur, pSecond, pThird );
		if ( (dcur.x == 0) && (dcur.y == 0) ) continue;
		/* Check current three points */
		ConvexCheckTriple;
	}

	/* Must check for direction changes from last vertex back to first */
	/* Prepare for 'ConvexCheckTriple' */
	pThird = pts[0];
	dcur.x = pThird.x - pSecond.x;
	dcur.y = pThird.y - pSecond.y;
	if ( ConvexCompare(dcur) ) {
		ConvexCheckTriple;
	}
	/* and check for direction changes back to second vertex */
	dcur.x = pSaveSecond.x - pSecond.x;
	dcur.y = pSaveSecond.y - pSecond.y;
	/* Don't care about 'pThird' now */
	ConvexCheckTriple;

	/* Decide on polygon type given accumulated status */
	if ( dirChanges > 2 ) return GF_POLYGON_COMPLEX;
	if ( angleSign > 0 ) return GF_POLYGON_CONVEX_CCW;
	if ( angleSign < 0 ) return GF_POLYGON_CONVEX_CW;
	return GF_POLYGON_CONVEX_LINE;
}

#endif // !defined(GPAC_DISABLE_EVG) || defined(GPAC_HAS_FREETYPE)
