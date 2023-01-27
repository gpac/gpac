/***************************************************************************/
/*                                                                         */
/*  ftstroke.c                                                             */
/*                                                                         */
/*    FreeType path stroker (body).                                        */
/*                                                                         */
/*  Copyright 2002, 2003, 2004 by                                          */
/*  David Turner, Robert Wilhelm, and Werner Lemberg.                      */
/*                                                                         */
/*  This file is part of the FreeType project, and may only be used,       */
/*  modified, and distributed under the terms of the FreeType project      */
/*  license, LICENSE.TXT.  By continuing to use, modify, or distribute     */
/*  this file you indicate that you have read the license and              */
/*  understand and accept it fully.                                        */
/*                                                                         */
/***************************************************************************/


#include <gpac/path2d.h>

#ifndef GPAC_DISABLE_EVG

/***************************************************************************/
/***************************************************************************/
/*****                                                                 *****/
/*****                       BEZIER COMPUTATIONS                       *****/
/*****                                                                 *****/
/***************************************************************************/
/***************************************************************************/

#define FT_SMALL_CONIC_THRESHOLD  ( GF_PI / 6 )
#define FT_SMALL_CUBIC_THRESHOLD  ( GF_PI / 6 )

#define FT_IS_SMALL( x )  ( (x) > -FIX_EPSILON && (x) < FIX_EPSILON )

#if 0 //unused

static void ft_conic_split(GF_Point2D*  base )
{
	Fixed  a, b;

	base[4].x = base[2].x;
	b = base[1].x;
	a = base[3].x = ( base[2].x + b ) / 2;
	b = base[1].x = ( base[0].x + b ) / 2;
	base[2].x = ( a + b ) / 2;

	base[4].y = base[2].y;
	b = base[1].y;
	a = base[3].y = ( base[2].y + b ) / 2;
	b = base[1].y = ( base[0].y + b ) / 2;
	base[2].y = ( a + b ) / 2;
}



static Bool ft_conic_is_small_enough( GF_Point2D*  base, Fixed *angle_in, Fixed *angle_out)
{
	GF_Point2D  d1, d2;
	Fixed theta;
	s32 close1, close2;
	d1.x = base[1].x - base[2].x;
	d1.y = base[1].y - base[2].y;
	d2.x = base[0].x - base[1].x;
	d2.y = base[0].y - base[1].y;
	close1 = FT_IS_SMALL( d1.x ) && FT_IS_SMALL( d1.y );
	close2 = FT_IS_SMALL( d2.x ) && FT_IS_SMALL( d2.y );

	if ( close1 ) {
		if ( close2 )
			*angle_in = *angle_out = 0;
		else
			*angle_in = *angle_out = gf_atan2(d2.y, d2.x);
	}
	else if ( close2 ) {
		*angle_in = *angle_out = gf_atan2(d1.y, d1.x);
	} else {
		*angle_in  = gf_atan2(d1.y, d1.x);
		*angle_out = gf_atan2(d2.y, d2.x);
	}
	theta = ABS( gf_angle_diff(*angle_in, *angle_out));
	return ( theta < FT_SMALL_CONIC_THRESHOLD ) ? GF_TRUE : GF_FALSE;
}

static void ft_cubic_split( GF_Point2D*  base )
{
	Fixed  a, b, c, d;
	base[6].x = base[3].x;
	c = base[1].x;
	d = base[2].x;
	base[1].x = a = ( base[0].x + c ) / 2;
	base[5].x = b = ( base[3].x + d ) / 2;
	c = ( c + d ) / 2;
	base[2].x = a = ( a + c ) / 2;
	base[4].x = b = ( b + c ) / 2;
	base[3].x = ( a + b ) / 2;

	base[6].y = base[3].y;
	c = base[1].y;
	d = base[2].y;
	base[1].y = a = ( base[0].y + c ) / 2;
	base[5].y = b = ( base[3].y + d ) / 2;
	c = ( c + d ) / 2;
	base[2].y = a = ( a + c ) / 2;
	base[4].y = b = ( b + c ) / 2;
	base[3].y = ( a + b ) / 2;
}


static Bool ft_cubic_is_small_enough(GF_Point2D *base, Fixed *angle_in, Fixed *angle_mid, Fixed *angle_out)
{
	GF_Point2D  d1, d2, d3;
	Fixed theta1, theta2;
	s32 close1, close2, close3;
	d1.x = base[2].x - base[3].x;
	d1.y = base[2].y - base[3].y;
	d2.x = base[1].x - base[2].x;
	d2.y = base[1].y - base[2].y;
	d3.x = base[0].x - base[1].x;
	d3.y = base[0].y - base[1].y;

	close1 = FT_IS_SMALL( d1.x ) && FT_IS_SMALL( d1.y );
	close2 = FT_IS_SMALL( d2.x ) && FT_IS_SMALL( d2.y );
	close3 = FT_IS_SMALL( d3.x ) && FT_IS_SMALL( d3.y );

	if ( close1 || close3 ) {
		if ( close2 ) {
			/* basically a point */
			*angle_in = *angle_out = *angle_mid = 0;
		} else if ( close1 ) {
			*angle_in  = *angle_mid = gf_atan2( d2.y, d2.x);
			*angle_out = gf_atan2( d3.y, d3.x);
		}
		/* close2 */
		else {
			*angle_in  = gf_atan2(d1.y, d1.x);
			*angle_mid = *angle_out = gf_atan2(d2.y, d2.x);
		}
	}
	else if ( close2 ) {
		*angle_in  = *angle_mid = gf_atan2(d1.y, d1.x);
		*angle_out = gf_atan2(d3.y, d3.x);
	} else {
		*angle_in  = gf_atan2(d1.y, d1.x);
		*angle_mid = gf_atan2(d2.y, d2.x);
		*angle_out = gf_atan2(d3.y, d3.x);
	}
	theta1 = ABS( gf_angle_diff( *angle_in,  *angle_mid ) );
	theta2 = ABS( gf_angle_diff( *angle_mid, *angle_out ) );
	return ((theta1 < FT_SMALL_CUBIC_THRESHOLD) && (theta2 < FT_SMALL_CUBIC_THRESHOLD )) ? GF_TRUE : GF_FALSE;
}

#endif

/***************************************************************************/
/***************************************************************************/
/*****                                                                 *****/
/*****                       STROKE BORDERS                            *****/
/*****                                                                 *****/
/***************************************************************************/
/***************************************************************************/

typedef enum
{
	FT_STROKE_TAG_ON    = 1,   /* on-curve point  */
	FT_STROKE_TAG_CUBIC = 2,   /* cubic off-point */
	FT_STROKE_TAG_BEGIN = 4,   /* sub-path start  */
	FT_STROKE_TAG_END   = 8    /* sub-path end    */
} FT_StrokeTags;


typedef struct  FT_StrokeBorderRec_
{
	u32 num_points;
	u32 max_points;
	GF_Point2D*  points;
	u8 *tags;
	Bool movable;
	/* index of current sub-path start point */
	s32 start;
	Bool valid;
} FT_StrokeBorderRec, *FT_StrokeBorder;


static s32 ft_stroke_border_grow(FT_StrokeBorder  border, u32 new_points)
{
	u32 new_max = border->num_points + new_points;
	if (new_max > border->max_points) {
		u32 cur_max = new_max*2;
		border->points = (GF_Point2D *) gf_realloc(border->points, sizeof(GF_Point2D)*cur_max);
		border->tags = (u8 *) gf_realloc(border->tags, sizeof(u8)*cur_max);
		if (!border->points || !border->tags) return -1;
		border->max_points = cur_max;
	}
	return 0;
}

static void ft_stroke_border_close( FT_StrokeBorder  border )
{
	/* don't record empty paths! */
	if ((border->start <0) || !border->num_points ) return;
	if ( border->num_points > (u32)border->start ) {
		border->tags[border->start] |= FT_STROKE_TAG_BEGIN;
		border->tags[border->num_points - 1] |= FT_STROKE_TAG_END;
	}
	border->start   = -1;
	border->movable = GF_FALSE;
}

static s32 ft_stroke_border_lineto( FT_StrokeBorder  border, GF_Point2D*       to, Bool movable )
{
	assert(border->start >= 0);

	if ( border->movable ) {
		/* move last point */
		border->points[border->num_points - 1] = *to;
	} else {
		/* add one point */
		if (ft_stroke_border_grow( border, 1 )==0) {
			GF_Point2D*  vec = border->points + border->num_points;
			u8 *tag = border->tags   + border->num_points;

			vec[0] = *to;
			tag[0] = FT_STROKE_TAG_ON;
			border->num_points += 1;
		} else {
			return -1;
		}
	}
	border->movable = movable;
	return 0;
}

#if 0 //unused
static s32 ft_stroke_border_conicto( FT_StrokeBorder  border, GF_Point2D*       control, GF_Point2D*       to )
{
	assert( border->start >= 0 );
	if (ft_stroke_border_grow( border, 2 )==0) {
		GF_Point2D*  vec = border->points + border->num_points;
		u8 *tag = border->tags   + border->num_points;

		vec[0] = *control;
		vec[1] = *to;

		tag[0] = 0;
		tag[1] = FT_STROKE_TAG_ON;

		border->num_points += 2;
	} else {
		return -1;
	}
	border->movable = GF_FALSE;
	return 0;
}
#endif

static s32 ft_stroke_border_cubicto( FT_StrokeBorder  border,
                                     GF_Point2D*       control1,
                                     GF_Point2D*       control2,
                                     GF_Point2D*       to )
{
	assert( border->start >= 0 );

	if (!ft_stroke_border_grow( border, 3 )) {
		GF_Point2D*  vec = border->points + border->num_points;
		u8*    tag = border->tags   + border->num_points;
		vec[0] = *control1;
		vec[1] = *control2;
		vec[2] = *to;

		tag[0] = FT_STROKE_TAG_CUBIC;
		tag[1] = FT_STROKE_TAG_CUBIC;
		tag[2] = FT_STROKE_TAG_ON;

		border->num_points += 3;
	} else {
		return -1;
	}
	border->movable = GF_FALSE;
	return 0;
}

#define FT_ARC_CUBIC_ANGLE  ( GF_PI / 2 )


static s32 ft_stroke_border_arcto( FT_StrokeBorder  border,
                                   GF_Point2D*       center,
                                   Fixed         radius,
                                   Fixed angle_start,
                                   Fixed angle_diff )
{
	Fixed total, angle, step, rotate, next, theta;
	GF_Point2D  a, b, a2, b2;
	Fixed length;
	/* compute start point */
	a = gf_v2d_from_polar(radius, angle_start );
	a.x += center->x;
	a.y += center->y;

	total  = angle_diff;
	angle  = angle_start;
	rotate = ( angle_diff >= 0 ) ? GF_PI2 : -GF_PI2;

	while ( total != 0 ) {
		step = total;
		if ( step > FT_ARC_CUBIC_ANGLE )
			step = FT_ARC_CUBIC_ANGLE;
		else if ( step < -FT_ARC_CUBIC_ANGLE )
			step = -FT_ARC_CUBIC_ANGLE;

		next  = angle + step;
		theta = step;
		if ( theta < 0 )
			theta = -theta;

#ifdef GPAC_FIXED_POINT
		theta >>= 1;
#else
		theta /= 2;
#endif

		/* compute end point */
		b = gf_v2d_from_polar(radius, next );
		b.x += center->x;
		b.y += center->y;

		/* compute first and second control points */
		length = gf_muldiv( radius, gf_sin( theta ) * 4, ( FIX_ONE + gf_cos( theta ) ) * 3 );

		a2 = gf_v2d_from_polar(length, angle + rotate );
		a2.x += a.x;
		a2.y += a.y;

		b2 = gf_v2d_from_polar(length, next - rotate );
		b2.x += b.x;
		b2.y += b.y;

		/* add cubic arc */
		if (ft_stroke_border_cubicto( border, &a2, &b2, &b ) != 0) return -1;

		/* process the rest of the arc ?? */
		a      = b;
		total -= step;
		angle  = next;
	}
	return 0;
}


static s32 ft_stroke_border_moveto(FT_StrokeBorder  border, GF_Point2D*       to )
{
	/* close current open path if any ? */
	if ( border->start >= 0 )
		ft_stroke_border_close( border );

	border->start   = border->num_points;
	border->movable = GF_FALSE;
	return ft_stroke_border_lineto(border, to, GF_FALSE);
}


static s32 ft_stroke_border_get_counts(FT_StrokeBorder  border,
                                       u32 *anum_points,
                                       u32 *anum_contours )
{
	s32 error        = 0;
	u32 num_points   = 0;
	u32 num_contours = 0;
	u32 count      = border->num_points;
	GF_Point2D *point      = border->points;
	u8 *tags       = border->tags;
	s32 in_contour = 0;

	for ( ; count > 0; count--, num_points++, point++, tags++ ) {
		if ( tags[0] & FT_STROKE_TAG_BEGIN ) {
			if ( in_contour != 0 ) goto Fail;

			in_contour = 1;
		} else if ( in_contour == 0 )
			goto Fail;

		if ( tags[0] & FT_STROKE_TAG_END ) {
			if ( in_contour == 0 )
				goto Fail;
			in_contour = 0;
			num_contours++;
		}
	}

	if ( in_contour != 0 )
		goto Fail;

	border->valid = GF_TRUE;

Exit:
	*anum_points   = num_points;
	*anum_contours = num_contours;
	return error;

Fail:
	num_points   = 0;
	num_contours = 0;
	error = -1;
	goto Exit;
}


static void ft_stroke_border_export( FT_StrokeBorder  border, GF_Path*      outline )
{
	if (!border->num_points) return;

	/* copy point locations */
	memcpy(outline->points + outline->n_points, border->points, sizeof(GF_Point2D)*border->num_points);

	/* copy tags */
	{
		u32 count = border->num_points;
		u8*  read  = border->tags;
		u8*  write = (u8*)outline->tags + outline->n_points;

		for ( ; count > 0; count--, read++, write++ ) {
			if ( *read & FT_STROKE_TAG_ON )
				*write = GF_PATH_CURVE_ON;
			else if ( *read & FT_STROKE_TAG_CUBIC )
				*write = GF_PATH_CURVE_CUBIC;
			else
				*write = GF_PATH_CURVE_CONIC;
		}
	}

	/* copy contours */
	{
		u32 count = border->num_points;
		u8 *tags  = border->tags;
		u32 *write = outline->contours + outline->n_contours;
		u32 idx = outline->n_points;

		for ( ; count > 0; count--, tags++, idx++ ) {
			if ( *tags & FT_STROKE_TAG_END ) {
				*write++ = idx;
				outline->n_contours++;
			}
		}
	}
	outline->n_points = outline->n_points + border->num_points;
}


/***************************************************************************/
/***************************************************************************/
/*****                                                                 *****/
/*****                           STROKER                               *****/
/*****                                                                 *****/
/***************************************************************************/
/***************************************************************************/

#define FT_SIDE_TO_ROTATE( s )   ( GF_PI2 - (s) * GF_PI )

typedef struct  FT_StrokerRec_
{
	Fixed angle_in;
	Fixed angle_out;
	GF_Point2D            center;
	Bool              first_point;
	Fixed subpath_angle;
	GF_Point2D            subpath_start;

	u32 line_cap;
	u32 line_join;
	Fixed miter_limit;
	Fixed radius;
	Bool              valid;
	Bool              closing;
	FT_StrokeBorderRec   borders[2];
} FT_StrokerRec, FT_Stroker;


/* creates a circular arc at a corner or cap */
static s32 ft_stroker_arcto( FT_Stroker  *stroker, s32 side )
{
	Fixed total, rotate;
	Fixed         radius = stroker->radius;
	s32 error  = 0;
	FT_StrokeBorder  border = stroker->borders + side;
	rotate = FT_SIDE_TO_ROTATE( side );
	total = gf_angle_diff( stroker->angle_in, stroker->angle_out);
	if ( total == GF_PI ) total = -rotate * 2;
	error = ft_stroke_border_arcto( border,
	                                &stroker->center,
	                                radius,
	                                stroker->angle_in + rotate,
	                                total );
	border->movable = GF_FALSE;
	return error;
}

/* adds a cap at the end of an opened path */
static s32 ft_stroker_cap(FT_Stroker  *stroker, Fixed angle, s32 side)
{
	s32 error  = 0;
	if ( stroker->line_cap == GF_LINE_CAP_ROUND) {
		/* OK we cheat a bit here compared to FT original code, and use a rough cubic cap instead of
		a circle to deal with arbitrary orientation of regular paths where arc cap is not always properly oriented.
		Rather than computing orientation we simply approximate to conic - btw this takes less memory than
		exact circle cap since cubics are natively supported - we don't use conic since result is not so good looking*/
		GF_Point2D delta, delta2, ctl1, ctl2, end;
		Fixed rotate = FT_SIDE_TO_ROTATE( side );
		Fixed radius = stroker->radius;
		FT_StrokeBorder  border = stroker->borders + side;


		delta = gf_v2d_from_polar(radius, angle);
		delta.x = 4*delta.x/3;
		delta.y = 4*delta.y/3;

		delta2 = gf_v2d_from_polar(radius, angle + rotate);
		ctl1.x = delta.x + stroker->center.x + delta2.x;
		ctl1.y = delta.y + stroker->center.y + delta2.y;

		delta2 = gf_v2d_from_polar(radius, angle - rotate);
		ctl2.x = delta.x + delta2.x + stroker->center.x;
		ctl2.y = delta.y + delta2.y + stroker->center.y;

		end.x = delta2.x + stroker->center.x;
		end.y = delta2.y + stroker->center.y;

		error = ft_stroke_border_cubicto( border, &ctl1, &ctl2, &end);
	} else if ( stroker->line_cap == GF_LINE_CAP_SQUARE) {
		/* add a square cap */
		GF_Point2D        delta, delta2;
		Fixed rotate = FT_SIDE_TO_ROTATE( side );
		Fixed radius = stroker->radius;
		FT_StrokeBorder  border = stroker->borders + side;


		delta2 = gf_v2d_from_polar(radius, angle + rotate);
		delta = gf_v2d_from_polar(radius, angle);

		delta.x += stroker->center.x + delta2.x;
		delta.y += stroker->center.y + delta2.y;

		error = ft_stroke_border_lineto(border, &delta, GF_FALSE);
		if ( error )
			goto Exit;

		delta2 = gf_v2d_from_polar(radius, angle - rotate);
		delta = gf_v2d_from_polar(radius, angle);

		delta.x += delta2.x + stroker->center.x;
		delta.y += delta2.y + stroker->center.y;

		error = ft_stroke_border_lineto(border, &delta, GF_FALSE);
	} else if ( stroker->line_cap == GF_LINE_CAP_TRIANGLE) {
		/* add a triangle cap */
		GF_Point2D delta;
		Fixed radius = stroker->radius;
		FT_StrokeBorder  border = stroker->borders + side;
		border->movable = GF_FALSE;
		delta = gf_v2d_from_polar(radius, angle);
		delta.x += stroker->center.x;
		delta.y += stroker->center.y;
		error = ft_stroke_border_lineto(border, &delta, GF_FALSE);
	}

Exit:
	return error;
}


/* process an inside corner, i.e. compute intersection */
static s32 ft_stroker_inside(FT_Stroker *stroker, s32 side)
{
	FT_StrokeBorder  border = stroker->borders + side;
	Fixed phi, theta, rotate;
	Fixed length, thcos, sigma;
	GF_Point2D        delta;
	s32 error = 0;

	rotate = FT_SIDE_TO_ROTATE( side );

	/* compute median angle */
	theta = gf_angle_diff( stroker->angle_in, stroker->angle_out );
	if ( theta == GF_PI )
		theta = rotate;
	else
		theta = theta / 2;

	phi = stroker->angle_in + theta;

	thcos  = gf_cos( theta );
	sigma  = gf_mulfix( stroker->miter_limit, thcos );

	if ( sigma < FIX_ONE ) {
		delta = gf_v2d_from_polar(stroker->radius, stroker->angle_out + rotate );
		delta.x += stroker->center.x;
		delta.y += stroker->center.y;
		if (!stroker->closing) border->movable = GF_FALSE;
	} else {
		length = gf_divfix( stroker->radius, thcos );
		delta = gf_v2d_from_polar(length, phi + rotate );
		delta.x += stroker->center.x;
		delta.y += stroker->center.y;
	}
	error = ft_stroke_border_lineto(border, &delta, GF_FALSE);
	return error;
}


/* process an outside corner, i.e. compute bevel/miter/round */
static s32 ft_stroker_outside( FT_Stroker *stroker, s32 side )
{
	FT_StrokeBorder  border = stroker->borders + side;
	s32 error;
	Fixed rotate;
	u32 join = stroker->line_join;

	if ( join == GF_LINE_JOIN_MITER_SVG ) {
		Fixed sin_theta, inv_sin_theta;
		join = GF_LINE_JOIN_MITER;
		sin_theta = gf_sin(gf_angle_diff( stroker->angle_out - GF_PI, stroker->angle_in) / 2 );
		if (sin_theta) {
			inv_sin_theta = gf_invfix(sin_theta);
			if (inv_sin_theta > stroker->miter_limit) join = GF_LINE_JOIN_BEVEL;
		} else {
			join = GF_LINE_JOIN_BEVEL;
		}
	}

	if ( join == GF_LINE_JOIN_ROUND ) {
		error = ft_stroker_arcto( stroker, side );
	} else if (join == GF_LINE_JOIN_BEVEL) {
		GF_Point2D  delta;
		rotate = FT_SIDE_TO_ROTATE( side );
		delta = gf_v2d_from_polar(stroker->radius, stroker->angle_out + rotate );
		delta.x += stroker->center.x;
		delta.y += stroker->center.y;
		/*prevent moving current point*/
		border->movable = GF_FALSE;
		/*and add un-movable end point*/
		error = ft_stroke_border_lineto( border, &delta, GF_FALSE);
	} else {
		/* this is a mitered or beveled corner */
		Fixed  sigma, radius = stroker->radius;
		Fixed theta, phi;
		Fixed thcos;
		Bool  miter = GF_TRUE;

		rotate = FT_SIDE_TO_ROTATE( side );

		theta  = gf_angle_diff( stroker->angle_in, stroker->angle_out );
		if ( theta == GF_PI ) {
			theta = rotate;
			phi   = stroker->angle_in;
		} else {
			theta = theta / 2;
			phi   = stroker->angle_in + theta + rotate;
		}

		thcos = gf_cos( theta );
		sigma = gf_mulfix( stroker->miter_limit, thcos );

		if ( sigma >= FIX_ONE ) {
			miter = GF_FALSE;
		}

		/* this is a miter (broken angle) */
		if ( miter ) {
			GF_Point2D  middle, delta;
			Fixed   length;

			/* compute middle point */
			middle = gf_v2d_from_polar(gf_mulfix(radius, stroker->miter_limit), phi);
			middle.x += stroker->center.x;
			middle.y += stroker->center.y;

			/* compute first angle point */
			length = gf_mulfix(radius, gf_divfix( FIX_ONE - sigma, ABS( gf_sin( theta ) ) ) );

			delta = gf_v2d_from_polar(length, phi + rotate );
			delta.x += middle.x;
			delta.y += middle.y;

			error = ft_stroke_border_lineto( border, &delta, GF_FALSE );
			if ( error )
				goto Exit;

			/* compute second angle point */
			delta = gf_v2d_from_polar(length, phi - rotate);
			delta.x += middle.x;
			delta.y += middle.y;

			error = ft_stroke_border_lineto( border, &delta, GF_FALSE );
			if ( error )
				goto Exit;

			/* finally, add a movable end point */
			delta = gf_v2d_from_polar(radius, stroker->angle_out + rotate );
			delta.x += stroker->center.x;
			delta.y += stroker->center.y;

			error = ft_stroke_border_lineto( border, &delta, GF_TRUE);
		}
		/* this is a bevel (intersection) */
		else {
			Fixed   length;
			GF_Point2D  delta;


			length = gf_divfix( stroker->radius, thcos );

			delta = gf_v2d_from_polar(length, phi );
			delta.x += stroker->center.x;
			delta.y += stroker->center.y;

			error = ft_stroke_border_lineto( border, &delta, GF_FALSE );
			if (error) goto Exit;

			/* now add end point */
			delta = gf_v2d_from_polar(stroker->radius, stroker->angle_out + rotate );
			delta.x += stroker->center.x;
			delta.y += stroker->center.y;

			error = ft_stroke_border_lineto( border, &delta, GF_TRUE );
		}
	}
Exit:
	return error;
}



static s32 ft_stroker_process_corner(FT_Stroker *stroker )
{
	s32 error = 0;
	Fixed turn;
	s32 inside_side;
	turn = gf_angle_diff( stroker->angle_in, stroker->angle_out );

	/* no specific corner processing is required if the turn is 0 */
	if ( turn == 0 )
		goto Exit;

	/* when we turn to the right, the inside side is 0 */
	inside_side = 0;
	/* otherwise, the inside side is 1 */
	if (turn < 0 )
		inside_side = 1;

	/* process the inside side */
	error = ft_stroker_inside( stroker, inside_side );
	if ( error ) goto Exit;

	/* process the outside side */
	error = ft_stroker_outside( stroker, 1 - inside_side );

Exit:
	return error;
}


/* add two points to the left and right borders corresponding to the */
/* start of the subpath..                                            */
static s32 ft_stroker_subpath_start( FT_Stroker *stroker, Fixed start_angle )
{
	GF_Point2D        delta;
	GF_Point2D        point;
	s32 error;
	FT_StrokeBorder  border;

	delta = gf_v2d_from_polar(stroker->radius, start_angle + GF_PI2 );

	point.x = stroker->center.x + delta.x;
	point.y = stroker->center.y + delta.y;

	border = stroker->borders;
	error = ft_stroke_border_moveto( border, &point );
	if ( error )
		goto Exit;

	point.x = stroker->center.x - delta.x;
	point.y = stroker->center.y - delta.y;

	border++;
	error = ft_stroke_border_moveto( border, &point );

	/* save angle for last cap */
	stroker->subpath_angle = start_angle;
	stroker->first_point   = GF_FALSE;

Exit:
	return error;
}


static s32 FT_Stroker_LineTo( FT_Stroker *stroker, GF_Point2D*  to, Bool is_last)
{
	s32 error = 0;
	FT_StrokeBorder  border;
	GF_Point2D        delta;
	Fixed angle;
	s32 side;

	delta.x = to->x - stroker->center.x;
	delta.y = to->y - stroker->center.y;
	if (!is_last && !delta.x && !delta.y) return 0;

	angle = gf_atan2( delta.y, delta.x);
	delta = gf_v2d_from_polar(stroker->radius, angle + GF_PI2 );

	/* process corner if necessary */
	if ( stroker->first_point ) {
		/* This is the first segment of a subpath.  We need to     */
		/* add a point to each border at their respective starting */
		/* point locations.                                        */
		error = ft_stroker_subpath_start( stroker, angle );
		if ( error )
			goto Exit;
	} else {
		/* process the current corner */
		stroker->angle_out = angle;
		error = ft_stroker_process_corner( stroker );
		if ( error )
			goto Exit;
	}

	/* now add a line segment to both the "inside" and "outside" paths */
	for ( border = stroker->borders, side = 1; side >= 0; side--, border++ ) {
		GF_Point2D  point;
		point.x = to->x + delta.x;
		point.y = to->y + delta.y;

		error = ft_stroke_border_lineto( border, &point, GF_TRUE );
		if ( error )
			goto Exit;

		delta.x = -delta.x;
		delta.y = -delta.y;
	}
	stroker->angle_in = angle;
	stroker->center   = *to;

Exit:
	return error;
}

#if 0 //unused
static s32 FT_Stroker_ConicTo(FT_Stroker *stroker, GF_Point2D*  control, GF_Point2D * to)
{
	s32 error = 0;
	GF_Point2D   bez_stack[34];
	GF_Point2D*  arc;
	GF_Point2D*  limit = bez_stack + 30;
	Fixed start_angle;
	Bool first_arc = GF_TRUE;


	arc    = bez_stack;
	arc[0] = *to;
	arc[1] = *control;
	arc[2] = stroker->center;

	while ( arc >= bez_stack ) {
		Fixed angle_in, angle_out;
		angle_in = angle_out = 0;  /* remove compiler warnings */

		if ( arc < limit                                             &&
		        !ft_conic_is_small_enough( arc, &angle_in, &angle_out ) )
		{
			ft_conic_split( arc );
			arc += 2;
			continue;
		}

		if ( first_arc ) {
			first_arc = GF_FALSE;

			start_angle = angle_in;

			/* process corner if necessary */
			if ( stroker->first_point )
				error = ft_stroker_subpath_start( stroker, start_angle );
			else {
				stroker->angle_out = start_angle;
				error = ft_stroker_process_corner( stroker );
			}
		}

		/* the arc's angle is small enough; we can add it directly to each */
		/* border                                                          */
		{
			GF_Point2D  ctrl, end;
			Fixed theta, phi, rotate;
			Fixed length;
			s32 side;

			theta  = gf_angle_diff( angle_in, angle_out ) / 2;
			phi    = angle_in + theta;
			length = gf_divfix( stroker->radius, gf_cos( theta ) );

			for ( side = 0; side <= 1; side++ ) {
				rotate = FT_SIDE_TO_ROTATE( side );

				/* compute control point */
				ctrl = gf_v2d_from_polar(length, phi + rotate );
				ctrl.x += arc[1].x;
				ctrl.y += arc[1].y;

				/* compute end point */
				end = gf_v2d_from_polar(stroker->radius, angle_out + rotate );
				end.x += arc[0].x;
				end.y += arc[0].y;

				error = ft_stroke_border_conicto( stroker->borders + side, &ctrl, &end );
				if ( error )
					goto Exit;
			}
		}

		arc -= 2;

		if ( arc < bez_stack )
			stroker->angle_in = angle_out;
	}

	stroker->center = *to;
Exit:
	return error;
}


static s32 FT_Stroker_CubicTo(FT_Stroker *stroker,
                              GF_Point2D*  control1,
                              GF_Point2D*  control2,
                              GF_Point2D*  to )
{
	s32 error = 0;
	GF_Point2D   bez_stack[37];
	GF_Point2D*  arc;
	GF_Point2D*  limit = bez_stack + 32;
	Fixed start_angle;
	Bool     first_arc = GF_TRUE;

	arc    = bez_stack;
	arc[0] = *to;
	arc[1] = *control2;
	arc[2] = *control1;
	arc[3] = stroker->center;

	while ( arc >= bez_stack ) {
		Fixed angle_in, angle_mid, angle_out;
		/* remove compiler warnings */
		angle_in = angle_out = angle_mid = 0;

		if (arc < limit &&
		        !ft_cubic_is_small_enough( arc, &angle_in, &angle_mid, &angle_out ) )
		{
			ft_cubic_split( arc );
			arc += 3;
			continue;
		}

		if ( first_arc ) {
			first_arc = GF_FALSE;

			/* process corner if necessary */
			start_angle = angle_in;

			if ( stroker->first_point )
				error = ft_stroker_subpath_start( stroker, start_angle );
			else {
				stroker->angle_out = start_angle;
				error = ft_stroker_process_corner( stroker );
			}
			if ( error )
				goto Exit;
		}

		/* the arc's angle is small enough; we can add it directly to each */
		/* border                                                          */
		{
			GF_Point2D  ctrl1, ctrl2, end;
			Fixed theta1, phi1, theta2, phi2, rotate;
			Fixed length1, length2;
			s32 side;


			theta1  = ABS( angle_mid - angle_in ) / 2;
			theta2  = ABS( angle_out - angle_mid ) / 2;
			phi1    = (angle_mid + angle_in ) / 2;
			phi2    = (angle_mid + angle_out ) / 2;
			length1 = gf_divfix( stroker->radius, gf_cos( theta1 ) );
			length2 = gf_divfix( stroker->radius, gf_cos(theta2) );

			for ( side = 0; side <= 1; side++ ) {
				rotate = FT_SIDE_TO_ROTATE( side );

				/* compute control points */
				ctrl1 = gf_v2d_from_polar(length1, phi1 + rotate );
				ctrl1.x += arc[2].x;
				ctrl1.y += arc[2].y;

				ctrl2 = gf_v2d_from_polar(length2, phi2 + rotate );
				ctrl2.x += arc[1].x;
				ctrl2.y += arc[1].y;

				/* compute end point */
				end = gf_v2d_from_polar(stroker->radius, angle_out + rotate );
				end.x += arc[0].x;
				end.y += arc[0].y;

				error = ft_stroke_border_cubicto( stroker->borders + side,
				                                  &ctrl1, &ctrl2, &end );
				if ( error )
					goto Exit;
			}
		}

		arc -= 3;
		if ( arc < bez_stack )
			stroker->angle_in = angle_out;
	}

	stroker->center = *to;

Exit:
	return error;
}
#endif



static s32 FT_Stroker_BeginSubPath(FT_Stroker *stroker, GF_Point2D*  to)
{
	/* We cannot process the first point, because there is not enough      */
	/* information regarding its corner/cap.  The latter will be processed */
	/* in the "end_subpath" routine.                                       */
	/*                                                                     */
	stroker->first_point   = GF_TRUE;
	stroker->center        = *to;

	/* record the subpath start point index for each border */
	stroker->subpath_start = *to;
	return 0;
}

static s32 ft_stroker_add_reverse_left( FT_Stroker *stroker, Bool     open )
{
	FT_StrokeBorder  right  = stroker->borders + 0;
	FT_StrokeBorder  left   = stroker->borders + 1;
	s32 new_points;
	s32 error  = 0;

	if (!left->num_points) return 0;

	assert( left->start >= 0 );
	new_points = left->num_points - left->start;
	if ( new_points > 0 ) {
		error = ft_stroke_border_grow( right, (u32)new_points );
		if ( error )
			goto Exit;

		{
			GF_Point2D*  dst_point = right->points + right->num_points;
			u8*    dst_tag   = right->tags   + right->num_points;
			GF_Point2D*  src_point = left->points  + left->num_points - 1;
			u8*    src_tag   = left->tags    + left->num_points - 1;

			while ( src_point >= left->points + left->start ) {
				*dst_point = *src_point;
				*dst_tag   = *src_tag;

				if ( open )
					dst_tag[0] &= ~( FT_STROKE_TAG_BEGIN | FT_STROKE_TAG_END );
				else {
					/* switch begin/end tags if necessary.. */
					if ( dst_tag[0] & ( FT_STROKE_TAG_BEGIN | FT_STROKE_TAG_END ) )
						dst_tag[0] ^= ( FT_STROKE_TAG_BEGIN | FT_STROKE_TAG_END );
				}

				src_point--;
				src_tag--;
				dst_point++;
				dst_tag++;
			}
		}

		left->num_points   = left->start;
		right->num_points += new_points;

		right->movable = GF_FALSE;
		left->movable  = GF_FALSE;
	}

Exit:
	return error;
}

/* there's a lot of magic in this function! */
static s32 FT_Stroker_EndSubPath( FT_Stroker *stroker, Bool do_close)
{
	s32  error  = 0;
	FT_StrokeBorder  right = stroker->borders;
	if (do_close) {
		Fixed turn;
		s32 inside_side;

		/* process the corner */
		stroker->angle_out = stroker->subpath_angle;
		turn               = gf_angle_diff(stroker->angle_in, stroker->angle_out );

		/* no specific corner processing is required if the turn is 0 */
		if ( turn != 0 ) {
			/* when we turn to the right, the inside side is 0 */
			inside_side = 0;

			/* otherwise, the inside side is 1 */
			if ( turn < 0 ) inside_side = 1;

			/* IMPORTANT: WE DO NOT PROCESS THE INSIDE BORDER HERE! */
			/* process the inside side                              */
			/* error = ft_stroker_inside( stroker, inside_side );   */
			/* if ( error )                                         */
			/*   goto Exit;                                         */

			/* process the outside side */
			error = ft_stroker_outside( stroker, 1 - inside_side );
			if ( error )
				goto Exit;
		}

		ft_stroker_add_reverse_left(stroker, GF_FALSE);
		/* then end our two subpaths */
		ft_stroke_border_close( stroker->borders + 0 );
		ft_stroke_border_close( stroker->borders + 1 );
	} else {
		/* All right, this is an opened path, we need to add a cap between */
		/* right & left, add the reverse of left, then add a final cap     */
		/* between left & right.                                           */
		error = ft_stroker_cap( stroker, stroker->angle_in, 0 );
		if ( error ) goto Exit;

		/* add reversed points from "left" to "right" */
		error = ft_stroker_add_reverse_left( stroker, GF_TRUE );
		if ( error ) goto Exit;

		/* now add the final cap */
		stroker->center = stroker->subpath_start;
		error = ft_stroker_cap( stroker,
		                        stroker->subpath_angle + GF_PI, 0 );
		if ( error )
			goto Exit;

		/* Now end the right subpath accordingly.  The left one is */
		/* rewind and doesn't need further processing.             */
		ft_stroke_border_close( right );
	}

Exit:
	return error;
}


static s32 FT_Stroker_GetCounts( FT_Stroker *stroker, u32 *anum_points, u32 *anum_contours )
{
	u32 count1, count2, num_points   = 0;
	u32 count3, count4, num_contours = 0;
	s32 error;

	error = ft_stroke_border_get_counts( stroker->borders + 0, &count1, &count2 );
	if ( error ) goto Exit;
	error = ft_stroke_border_get_counts( stroker->borders + 1, &count3, &count4 );
	if ( error ) goto Exit;
	num_points   = count1 + count3;
	num_contours = count2 + count4;

Exit:
	*anum_points   = num_points;
	*anum_contours = num_contours;
	return error;
}

/*
*  The following is very similar to FT_Outline_Decompose, except
*  that we do support opened paths, and do not scale the outline.
*/
static s32 FT_Stroker_ParseOutline(FT_Stroker *stroker, GF_Path*  outline)
{
	GF_Point2D   v_last;
#if 0 //unused
	GF_Point2D   v_control;
#endif
	GF_Point2D   v_start;
	GF_Point2D*  point;
	GF_Point2D*  limit;
	u8 *tags;
	s32 error;
	u32 n;         /* index of contour in outline     */
	u32 first;     /* index of first point in contour */
	s32 tag;       /* current point's state           */

	if ( !outline || !stroker )
		return -1;

	first = 0;

	for ( n = 0; n < outline->n_contours; n++ ) {
		s32 closed_subpath;
		s32 last;  /* index of last point in contour */

		last  = outline->contours[n];
		limit = outline->points + last;

		v_start = outline->points[first];
		v_last  = outline->points[last];

#if 0 //unused
		v_control = v_start;
#endif

		point = outline->points + first;
		tags  = outline->tags  + first;
		tag = tags[0];

		/* A contour cannot start with a cubic control point! */
		if ( tag == GF_PATH_CURVE_CUBIC )
			goto Invalid_Outline;

		/* check first point to determine origin */
		if ( tag == GF_PATH_CURVE_CONIC ) {
			/* First point is conic control.  Yes, this happens. */
			if ( outline->tags[last] & GF_PATH_CURVE_ON ) {
				/* start at last point if it is on the curve */
				v_start = v_last;
				limit--;
			} else {
				/* if both first and last points are conic,         */
				/* start at their middle and record its position    */
				/* for closure                                      */
				v_start.x = ( v_start.x + v_last.x ) / 2;
				v_start.y = ( v_start.y + v_last.y ) / 2;
			}
			point--;
			tags--;
		}
		closed_subpath = (outline->tags[outline->contours[n]]==GF_PATH_CLOSE) ? 1 : 0;

		error = FT_Stroker_BeginSubPath(stroker, &v_start);
		if ( error )
			goto Exit;

		/*subpath is a single point, force a lineTo to start for the stroker to compute lineCap*/
		if (point==limit) {
			error = FT_Stroker_LineTo(stroker, &v_start, GF_TRUE);
			closed_subpath = 0;
			goto Close;
		}

		while ( point < limit ) {
			point++;
			tags++;

			tag = tags[0];
			switch ( tag ) {
			case GF_PATH_CURVE_ON:  /* emit a single line_to */
			case GF_PATH_CLOSE:  /* emit a single line_to */
			{
				GF_Point2D  vec;
				vec.x = point->x;
				vec.y = point->y;

				error = FT_Stroker_LineTo( stroker, &vec, (point == limit) ? GF_TRUE : GF_FALSE );
				if ( error )
					goto Exit;
				continue;
			}


#if 0 //unused
			case GF_PATH_CURVE_CONIC:  /* consume conic arcs */
				v_control.x = point->x;
				v_control.y = point->y;

Do_Conic:
				if ( point < limit ) {
					GF_Point2D  vec;
					GF_Point2D  v_middle;


					point++;
					tags++;
					tag = tags[0];

					vec = point[0];

					if ( tag & GF_PATH_CURVE_ON) {

						error = FT_Stroker_ConicTo( stroker, &v_control, &vec );
						if ( error )
							goto Exit;
						continue;
					}

					if ( tag != GF_PATH_CURVE_CONIC )
						goto Invalid_Outline;

					v_middle.x = ( v_control.x + vec.x ) / 2;
					v_middle.y = ( v_control.y + vec.y ) / 2;

					error = FT_Stroker_ConicTo( stroker, &v_control, &v_middle );
					if ( error )
						goto Exit;

					v_control = vec;
					goto Do_Conic;
				}
				error = FT_Stroker_ConicTo( stroker, &v_control, &v_start );
				goto Close;

			default:  /* GF_PATH_CURVE_CUBIC */
			{
				GF_Point2D  vec1, vec2;

				if ( point + 1 > limit                             ||
				        tags[1] != GF_PATH_CURVE_CUBIC )
					goto Invalid_Outline;

				point += 2;
				tags  += 2;

				vec1 = point[-2];
				vec2 = point[-1];

				if ( point <= limit ) {
					GF_Point2D  vec;
					vec = point[0];

					error = FT_Stroker_CubicTo( stroker, &vec1, &vec2, &vec );
					if ( error )
						goto Exit;
					continue;
				}
				error = FT_Stroker_CubicTo( stroker, &vec1, &vec2, &v_start );
				goto Close;
			}
			break;
#else
			default:  /* GF_PATH_CURVE_CUBIC */
				GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Path2DStroke] Path is not flatten, cannot strike cubic and quadratic !\n"));
#endif

			}
		}

Close:
		if ( error ) goto Exit;

		error = FT_Stroker_EndSubPath(stroker, closed_subpath);
		if ( error )
			goto Exit;

		first = last + 1;
	}
	return 0;

Exit:
	return error;

Invalid_Outline:
	return -1;
}






#define GF_PATH_DOT_LEN		1
#define GF_PATH_DOT_SPACE	2
#define GF_PATH_DASH_LEN	3

static Fixed gf_path_get_dash(GF_PenSettings *pen, u32 dash_slot, u32 *next_slot)
{
	Fixed ret = 0;
	switch (pen->dash) {
	case GF_DASH_STYLE_DOT:
		if (dash_slot==0) ret = GF_PATH_DOT_LEN;
		else if (dash_slot==1) ret = GF_PATH_DOT_SPACE;
		*next_slot = (dash_slot + 1) % 2;
		return ret * pen->width;
	case GF_DASH_STYLE_DASH:
		if (dash_slot==0) ret = GF_PATH_DASH_LEN;
		else if (dash_slot==1) ret = GF_PATH_DOT_SPACE;
		*next_slot = (dash_slot + 1) % 2;
		return ret * pen->width;
	case GF_DASH_STYLE_DASH_DOT:
		if (dash_slot==0) ret = GF_PATH_DASH_LEN;
		else if (dash_slot==1) ret = GF_PATH_DOT_SPACE;
		else if (dash_slot==2) ret = GF_PATH_DOT_LEN;
		else if (dash_slot==3) ret = GF_PATH_DOT_SPACE;
		*next_slot = (dash_slot + 1) % 4;
		return ret * pen->width;
	case GF_DASH_STYLE_DASH_DASH_DOT:
		if (dash_slot==0) ret = GF_PATH_DASH_LEN;
		else if (dash_slot==1) ret = GF_PATH_DOT_SPACE;
		else if (dash_slot==2) ret = GF_PATH_DASH_LEN;
		else if (dash_slot==3) ret = GF_PATH_DOT_SPACE;
		else if (dash_slot==4) ret = GF_PATH_DOT_LEN;
		else if (dash_slot==5) ret = GF_PATH_DOT_SPACE;
		*next_slot = (dash_slot + 1) % 6;
		return ret * pen->width;
	case GF_DASH_STYLE_DASH_DOT_DOT:
		if (dash_slot==0) ret = GF_PATH_DASH_LEN;
		else if (dash_slot==1) ret = GF_PATH_DOT_SPACE;
		else if (dash_slot==2) ret = GF_PATH_DOT_LEN;
		else if (dash_slot==3) ret = GF_PATH_DOT_SPACE;
		else if (dash_slot==4) ret = GF_PATH_DOT_LEN;
		else if (dash_slot==5) ret = GF_PATH_DOT_SPACE;
		*next_slot = (dash_slot + 1) % 6;
		return ret * pen->width;
	case GF_DASH_STYLE_CUSTOM:
	case GF_DASH_STYLE_SVG:
		if (!pen->dash_set || !pen->dash_set->num_dash) return 0;
		if (dash_slot>=pen->dash_set->num_dash) dash_slot = 0;
		ret = pen->dash_set->dashes[dash_slot];
		*next_slot = (1 + dash_slot) % pen->dash_set->num_dash;
		if (pen->dash==GF_DASH_STYLE_SVG) return ret;
		/*custom dashes are of type Fixed !!*/
		return gf_mulfix(ret, pen->width);

	default:
	case GF_DASH_STYLE_PLAIN:
		*next_slot = 0;
		return 0;
	}
}


/* Credits go to Raph Levien for libart / art_vpath_dash */

/* FIXEME - NOT DONE - Merge first and last subpaths when first and last dash segment are joined a closepath. */
static GF_Err gf_path_mergedashes(GF_Path *gp, u32 start_contour_index)
{
	u32 i, dash_first_pt, dash_nb_pts;
	if (start_contour_index) {
		dash_nb_pts = gp->contours[start_contour_index] - gp->contours[start_contour_index-1];
		dash_first_pt = gp->contours[start_contour_index-1]+1;
	} else {
		dash_nb_pts = gp->contours[start_contour_index]+1;
		dash_first_pt = 0;
	}
	/*skip first point of first dash in subpath (same as last point of last dash)*/
	for (i=1; i<dash_nb_pts; i++) {
		GF_Err e = gf_path_add_line_to_vec(gp, &gp->points[dash_first_pt + i]);
		if (e) return e;
	}
	/*remove initial dash*/
	gp->n_points -= dash_nb_pts;
	memmove(gp->points + dash_first_pt, gp->points + dash_first_pt + dash_nb_pts, sizeof(GF_Point2D)*(gp->n_points - dash_first_pt));
	memmove(gp->tags + dash_first_pt, gp->tags + dash_first_pt + dash_nb_pts, sizeof(u8)*(gp->n_points - dash_first_pt));

	for (i=start_contour_index; i<gp->n_contours-1; i++) {
		gp->contours[i] = gp->contours[i+1] - dash_nb_pts;
	}
	gp->n_contours--;
	gp->contours = (u32 *)gf_realloc(gp->contours, sizeof(u32)*gp->n_contours);

	/*
		gp->points = gf_realloc(gp->points, sizeof(GF_Point2D)*gp->n_points);
		gp->tags = gf_realloc(gp->tags, sizeof(u8)*gp->n_points);
		gp->n_alloc_points = gp->n_points;
	*/
	return GF_OK;
}

static GF_Err evg_dash_subpath(GF_Path *dashed, GF_Point2D *pts, u32 nb_pts, GF_PenSettings *pen, Fixed length_scale)
{
	Fixed *dists;
	Fixed totaldist;
	Fixed dash;
	Fixed dist;
	Fixed dash_dist;
	s32 offsetinit;
	u32 next_offset=0;
	s32 toggleinit;
	s32 firstindex;
	Bool toggle_check;
	GF_Err e;
	u32 i, start_ind;
	Fixed phase;
	s32 offset, toggle;

	dists = (Fixed *)gf_malloc(sizeof (Fixed) * nb_pts);
	if (dists == NULL) return GF_OUT_OF_MEM;

	/* initial values */
	toggleinit = 1;
	offsetinit = 0;
	dash_dist = 0;

	dash = gf_path_get_dash(pen, offsetinit, &next_offset);
	if (length_scale) dash = gf_mulfix(dash, length_scale);
	firstindex = -1;
	toggle_check = GF_FALSE;

	start_ind = 0;
	dist = 0;

	/*SVG dashing is different from BIFS one*/
	if (pen->dash==GF_DASH_STYLE_SVG) {
		while (pen->dash_offset>0) {
			if (pen->dash_offset - dash < 0) {
				dash -= pen->dash_offset;
				pen->dash_offset = 0;
				break;
			}
			pen->dash_offset -= dash;
			offsetinit = next_offset;
			toggleinit = !toggleinit;
			dash = gf_path_get_dash(pen, offsetinit, &next_offset);
			if (length_scale) dash = gf_mulfix(dash, length_scale);
		}
		if ((pen->dash_offset<0) && pen->dash_set && pen->dash_set->num_dash) {
			offsetinit = pen->dash_set->num_dash-1;
			dash = gf_path_get_dash(pen, offsetinit, &next_offset);
			if (length_scale) dash = gf_mulfix(dash, length_scale);
			while (pen->dash_offset<0) {
				toggleinit = !toggleinit;
				if (pen->dash_offset + dash > 0) {
					dash_dist = dash+pen->dash_offset;
					pen->dash_offset = 0;
					break;
				}
				pen->dash_offset += dash;
				if (offsetinit) offsetinit --;
				else offsetinit = pen->dash_set->num_dash-1;
				dash = gf_path_get_dash(pen, offsetinit, &next_offset);
				if (length_scale) dash = gf_mulfix(dash, length_scale);
			}
		}
	}

	/* calculate line lengths and update offset*/
	totaldist = 0;
	for (i = 0; i < nb_pts - 1; i++) {
		GF_Point2D diff;
		diff.x = pts[i+1].x - pts[i].x;
		diff.y = pts[i+1].y - pts[i].y;
		dists[i] = gf_v2d_len(&diff);

		if (pen->dash_offset > dists[i]) {
			pen->dash_offset -= dists[i];
			dists[i] = 0;
		}
		else if (pen->dash_offset) {
			Fixed a, x, y, dx, dy;

			a = gf_divfix(pen->dash_offset, dists[i]);
			dx = pts[i + 1].x - pts[i].x;
			dy = pts[i + 1].y - pts[i].y;
			x = pts[i].x + gf_mulfix(a, dx);
			y = pts[i].y + gf_mulfix(a, dy);
			e = gf_path_add_move_to(dashed, x, y);
			if (e) goto err_exit;
			totaldist += dists[i];
			dist = pen->dash_offset;
			pen->dash_offset = 0;
			start_ind = i;
		} else {
			totaldist += dists[i];
		}
	}
	dash -= dash_dist;

	/* subpath fits within first dash and no offset*/
	if (!dist && totaldist <= dash) {
		if (toggleinit) {
			gf_path_add_move_to_vec(dashed, &pts[0]);
			for (i=1; i<nb_pts; i++) {
				gf_path_add_line_to_vec(dashed, &pts[i]);
			}
		}
		gf_free(dists);
		return GF_OK;
	}

	/* subpath is composed of at least one dash */
	phase = 0;
	toggle = toggleinit;
	i = start_ind;

	if (toggle && !dist) {
		e = gf_path_add_move_to_vec(dashed, &pts[i]);
		if (e) goto err_exit;
		firstindex = dashed->n_contours - 1;
	}

	while (i < nb_pts - 1) {
		/* dash boundary is next */
		if (dists[i] - dist > dash - phase) {
			Fixed a, x, y, dx, dy;
			dist += dash - phase;
			a = gf_divfix(dist, dists[i]);
			dx = pts[i + 1].x - pts[i].x;
			dy = pts[i + 1].y - pts[i].y;
			x = pts[i].x + gf_mulfix(a, dx);
			y = pts[i].y + gf_mulfix(a, dy);

			if (!toggle_check || ((x != pts[i].x) || (y != pts[i].y))) {
				if (toggle) {
					e = gf_path_add_line_to(dashed, x, y);
					if (e) goto err_exit;
				}
				else {
					e = gf_path_add_move_to(dashed, x, y);
					if (e) goto err_exit;
				}
			}

			/* advance to next dash */
			toggle = !toggle;
			phase = 0;
			offset = next_offset;
			dash = gf_path_get_dash(pen, offset, &next_offset);
			if (length_scale) dash = gf_mulfix(dash, length_scale);
		}
		/* end of line in subpath is next */
		else {
			phase += dists[i] - dist;
			i ++;
			toggle_check = GF_FALSE;
			dist = 0;
			if (toggle) {
				e = gf_path_add_line_to_vec(dashed, &pts[i]);
				if (e) goto err_exit;
				toggle_check = GF_TRUE;

				if ( (firstindex>=0) && (i == (nb_pts - 1) && ((firstindex + 1) != (s32) start_ind ) ))  {
					/*merge if closed path*/
					if ((pts[0].x==pts[nb_pts-1].x) && (pts[0].y==pts[nb_pts-1].y)) {
						e = gf_path_mergedashes(dashed, firstindex);
						if (e) goto err_exit;
					}
				}
			}
		}
	}

err_exit:
//	pen->dash_offset = dist;
	gf_free(dists);
	return GF_OK;
}

static GF_Path *gf_path_dash(GF_Path *path, GF_PenSettings *pen)
{
	u32 i, j, nb_pts;
	GF_Point2D *pts;
	Fixed length_scale = 0;
	Fixed dash_off = pen->dash_offset;
	GF_Path *dashed = gf_path_new();


	/* calculate line lengths and update offset*/
	if (pen->path_length>0) {
		Fixed totaldist = 0;
		nb_pts = 0;
		for (i=0; i<path->n_contours; i++) {
			pts = &path->points[nb_pts];
			nb_pts = 1+path->contours[i] - nb_pts;

			for (j=0; j<nb_pts-1; j++) {
				GF_Point2D diff;
				diff.x = pts[j+1].x - pts[j].x;
				diff.y = pts[j+1].y - pts[j].y;
				totaldist += gf_v2d_len(&diff);
			}
			nb_pts = 1+path->contours[i];
		}
		length_scale = gf_divfix(totaldist, pen->path_length);
		pen->dash_offset = gf_mulfix(pen->dash_offset, length_scale);
	}

	nb_pts = 0;
	for (i=0; i<path->n_contours; i++) {
		pts = &path->points[nb_pts];
		nb_pts = 1+path->contours[i] - nb_pts;
		evg_dash_subpath(dashed, pts, nb_pts, pen, length_scale);
		nb_pts = 1+path->contours[i];
//		if (length_scale) pen->dash_offset = gf_mulfix(pen->dash_offset, length_scale);
	}
	pen->dash_offset = dash_off;
	dashed->flags |= GF_PATH_FILL_ZERO_NONZERO;
	return dashed;
}

GF_EXPORT
GF_Path *gf_path_get_outline(GF_Path *path, GF_PenSettings pen)
{
	s32 error;
	GF_Path *outline;
	GF_Path *dashed;
	GF_Path *scaled;
	FT_Stroker stroker;
	if (!path || !pen.width) return NULL;

	memset(&stroker, 0, sizeof(stroker));
	stroker.borders[0].start = -1;
	stroker.borders[1].start = -1;
	stroker.line_cap = pen.cap;
	stroker.line_join = pen.join;
	stroker.miter_limit = pen.miterLimit;
	stroker.radius = pen.width/2;

	gf_path_flatten(path);

	scaled = NULL;
	/*if not centered, simply scale path...*/
	if (pen.align) {
		Fixed sx, sy;
		GF_Rect bounds;
		gf_path_get_bounds(path, &bounds);
		if (pen.align==GF_PATH_LINE_OUTSIDE) {
			sx = gf_divfix(bounds.width+pen.width, bounds.width);
			sy = gf_divfix(bounds.height+pen.width, bounds.height);
		} else {
			/*note: this may result in negative scaling, not our pb but the author's one*/
			sx = gf_divfix(bounds.width-pen.width, bounds.width);
			sy = gf_divfix(bounds.height-pen.width, bounds.height);
		}
		if (sx && sy) {
			u32 i;
			scaled = gf_path_clone(path);
			for (i=0; i<scaled->n_points; i++) {
				scaled->points[i].x = gf_mulfix(scaled->points[i].x, sx);
				scaled->points[i].y = gf_mulfix(scaled->points[i].y, sy);
			}
			path = scaled;
		}
	}

	/*if dashing, first flatten path then dash all segments*/
	dashed = NULL;
	/*security, seen in some SVG files*/
	if (pen.dash_set && (pen.dash_set->num_dash==1) && (pen.dash_set->dashes[0]==0)) pen.dash = GF_DASH_STYLE_PLAIN;
	if (pen.dash) {
		GF_Path *flat;
		flat = gf_path_get_flatten(path);
		if (!flat) return NULL;
		dashed = gf_path_dash(flat, &pen);
		gf_path_del(flat);
		if (!dashed) return NULL;
		path = dashed;
	}

	outline = NULL;
	error = FT_Stroker_ParseOutline(&stroker, path);
	if (!error) {
		u32 nb_pt, nb_cnt;
		error = FT_Stroker_GetCounts(&stroker, &nb_pt, &nb_cnt);
		if (!error) {
			outline = gf_path_new();
			if (nb_pt) {
				FT_StrokeBorder sborder;
				outline->points = (GF_Point2D *) gf_malloc(sizeof(GF_Point2D)*nb_pt);
				outline->tags = (u8 *) gf_malloc(sizeof(u8)*nb_pt);
				outline->contours = (u32 *) gf_malloc(sizeof(u32)*nb_cnt);
				outline->n_alloc_points = nb_pt;
				sborder = &stroker.borders[0];
				if (sborder->valid ) ft_stroke_border_export(sborder, outline);
				sborder = &stroker.borders[1];
				/*if left border is valid this is a closed path, used odd/even rule - we will have issues at recovering
				segments...*/
				if (sborder->valid && sborder->num_points) {
					ft_stroke_border_export(sborder, outline);
				}
				/*otherwise this is an open path, use zero/non-zero*/
				else {
					outline->flags |= GF_PATH_FILL_ZERO_NONZERO;
				}
			}
			outline->flags |= GF_PATH_BBOX_DIRTY;

			/*our caps are cubic bezier!!*/
			if ( (path->flags & GF_PATH_FLATTENED) && (pen.cap!=GF_LINE_CAP_ROUND) && (pen.join!=GF_LINE_JOIN_ROUND) )
				outline->flags |= GF_PATH_FLATTENED;
		}
	}

	if (stroker.borders[0].points) gf_free(stroker.borders[0].points);
	if (stroker.borders[0].tags) gf_free(stroker.borders[0].tags);
	if (stroker.borders[1].points) gf_free(stroker.borders[1].points);
	if (stroker.borders[1].tags) gf_free(stroker.borders[1].tags);

	if (dashed) gf_path_del(dashed);
	if (scaled) gf_path_del(scaled);

	return outline;
}

#endif //GPAC_DISABLE_EVG
