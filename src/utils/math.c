/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
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
 *
 *	fixed-point trigo routines taken from freetype
 *		Copyright 1996-2001, 2002, 2004 by                                   
 *		David Turner, Robert Wilhelm, and Werner Lemberg.
 *	License: FTL or GPL
 *
 */

#include <gpac/math.h>

#ifdef GPAC_FIXED_POINT

Fixed gf_divfix(Fixed a, Fixed b)
{
	s32 s;
	u32 q;

    s = 1;
    if ( a < 0 ) { a = -a; s = -1; }
    if ( b < 0 ) { b = -b; s = -s; }

    if ( b == 0 )
		/* check for division by 0 */
		q = 0x7FFFFFFFL;
    else
		/* compute result directly */
		q = (u32)( ( ( (s64)a << 16 ) + ( b >> 1 ) ) / b );
    return ( s < 0 ? -(s32)q : (s32)q );
}

Fixed gf_mulfix(Fixed a, Fixed b)
{
	Fixed  s;
	u32 ua, ub;
	if ( !a || (b == 0x10000L)) return a;
	if (!b) return 0;

	s  = a; a = ABS(a);
	s ^= b; b = ABS(b);

	ua = (u32)a;
	ub = (u32)b;

	if ((ua <= 2048) && (ub <= 1048576L)) {
		ua = ( ua * ub + 0x8000L ) >> 16;
	} else {
		u32 al = ua & 0xFFFFL;
		ua = ( ua >> 16 ) * ub +  al * ( ub >> 16 ) + ( ( al * ( ub & 0xFFFFL ) + 0x8000L ) >> 16 );
	}
	if (ua & 0x80000000) s=-s;
	return ( s < 0 ? -(Fixed)(s32)ua : (Fixed)ua );
}

Fixed gf_muldiv(Fixed a, Fixed b, Fixed c)
{
    s32 s, d;
	if (!b || !a) return 0;
    s = 1;
    if ( a < 0 ) { a = -a; s = -1; }
    if ( b < 0 ) { b = -b; s = -s; }
    if ( c < 0 ) { c = -c; s = -s; }

    d = (s32)( c > 0 ? ( (s64)a * b + ( c >> 1 ) ) / c : 0x7FFFFFFFL);
    return (Fixed) (( s > 0 ) ? d : -d);
}

Fixed gf_sqrt(Fixed x)
{
	u32 root, rem_hi, rem_lo, test_div;
	s32 count;
	root = 0;
	if ( x > 0 ) {
		rem_hi = 0;
		rem_lo = x;
		count  = 24;
		do {
			rem_hi   = ( rem_hi << 2 ) | ( rem_lo >> 30 );
			rem_lo <<= 2;
			root   <<= 1;
			test_div = ( root << 1 ) + 1;

			if ( rem_hi >= test_div ) {
				rem_hi -= test_div;
				root   += 1;
			}
		} while ( --count );
	}

	return (Fixed) root;
}

Fixed gf_ceil(Fixed a)
{
	return (a >= 0) ? (a + 0xFFFFL) & ~0xFFFFL : -((-a + 0xFFFFL) & ~0xFFFFL);
}

Fixed gf_floor(Fixed a)
{
    return (a >= 0) ? (a & ~0xFFFFL) : -((-a) & ~0xFFFFL);
}




/* the following is 0.2715717684432231 * 2^30 */
#define GF_TRIG_COSCALE  0x11616E8EUL

/* this table was generated for degrees */
#define GF_TRIG_MAX_ITERS  23

static const Fixed gf_trig_arctan_table[24] =
{
	4157273L, 2949120L, 1740967L, 919879L, 466945L, 234379L, 117304L,
	58666L, 29335L, 14668L, 7334L, 3667L, 1833L, 917L, 458L, 229L, 115L,
	57L, 29L, 14L, 7L, 4L, 2L, 1L
};

/* the Cordic shrink factor, multiplied by 2^32 */
#define GF_TRIG_SCALE    1166391785UL  /* 0x4585BA38UL */

#define GF_ANGLE_PI		(180<<16)
#define GF_ANGLE_PI2	(90<<16)
#define GF_ANGLE_2PI	(360<<16)

#define GF_PAD_FLOOR( x, n )  ( (x) & ~((n)-1) )
#define GF_PAD_ROUND( x, n )  GF_PAD_FLOOR( (x) + ((n)/2), n )
#define GF_PAD_CEIL( x, n )   GF_PAD_FLOOR( (x) + ((n)-1), n )

/* multiply a given value by the CORDIC shrink factor */
static Fixed gf_trig_downscale(Fixed  val)
{
	Fixed  s;
	s64 v;
	s   = val;
	val = ( val >= 0 ) ? val : -val;
	v   = ( val * (s64)GF_TRIG_SCALE ) + 0x100000000UL;
	val = (Fixed)( v >> 32 );
	return ( s >= 0 ) ? val : -val;
}

static s32 gf_trig_prenorm(GF_Point2D *vec)
{
	Fixed  x, y, z;
	s32 shift;
	x = vec->x;
	y = vec->y;
	z     = ( ( x >= 0 ) ? x : - x ) | ( (y >= 0) ? y : -y );
	shift = 0;
	if ( z < ( 1L << 27 ) ) {
      do {
		  shift++;
		  z <<= 1;
      }
	  while ( z < ( 1L << 27 ) );

      vec->x = x << shift;
      vec->y = y << shift;
    }    
	else if ( z > ( 1L << 28 ) ) {
		do {
			shift++;
			z >>= 1;
		} while ( z > ( 1L << 28 ) );

		vec->x = x >> shift;
		vec->y = y >> shift;
		shift  = -shift;
    }
    return shift;
}

#define ANGLE_RAD_TO_DEG(_th) ((s32) ( (((s64)_th)*5729582)/100000))
#define ANGLE_DEG_TO_RAD(_th) ((s32) ( (((s64)_th)*100000)/5729582))

static void gf_trig_pseudo_rotate(GF_Point2D*  vec, Fixed theta)
{
    s32 i;
    Fixed x, y, xtemp;
    const Fixed  *arctanptr;

	/*trig funcs are in degrees*/
	theta = ANGLE_RAD_TO_DEG(theta);

    x = vec->x;
    y = vec->y;

    /* Get angle between -90 and 90 degrees */
    while ( theta <= -GF_ANGLE_PI2 ) {
		x = -x;
		y = -y;
		theta += GF_ANGLE_PI;
    }

    while ( theta > GF_ANGLE_PI2 ) {
		x = -x;
		y = -y;
		theta -= GF_ANGLE_PI;
    }

    /* Initial pseudorotation, with left shift */
    arctanptr = gf_trig_arctan_table;
    if ( theta < 0 ) {
		xtemp  = x + ( y << 1 );
		y      = y - ( x << 1 );
		x      = xtemp;
		theta += *arctanptr++;
    } else {
		xtemp  = x - ( y << 1 );
		y      = y + ( x << 1 );
		x      = xtemp;
		theta -= *arctanptr++;
    }
    /* Subsequent pseudorotations, with right shifts */
    i = 0;
    do {
		if ( theta < 0 ) {
			xtemp  = x + ( y >> i );
			y      = y - ( x >> i );
			x      = xtemp;
			theta += *arctanptr++;
		} else {
			xtemp  = x - ( y >> i );
			y      = y + ( x >> i );
			x      = xtemp;
			theta -= *arctanptr++;
		}
    } while ( ++i < GF_TRIG_MAX_ITERS );

    vec->x = x;
    vec->y = y;
}


static void gf_trig_pseudo_polarize(GF_Point2D *vec)
{
	Fixed         theta;
	Fixed         yi, i;
	Fixed         x, y;
	const Fixed  *arctanptr;

	x = vec->x;
	y = vec->y;

	/* Get the vector into the right half plane */
	theta = 0;
	if ( x < 0 ) {
		x = -x;
		y = -y;
		theta = 2 * GF_ANGLE_PI2;
	}

	if ( y > 0 )
		theta = - theta;

	arctanptr = gf_trig_arctan_table;

	if ( y < 0 ) {
		/* Rotate positive */
		yi     = y + ( x << 1 );
		x      = x - ( y << 1 );
		y      = yi;
		theta -= *arctanptr++;  /* Subtract angle */
	} else {
		/* Rotate negative */
		yi     = y - ( x << 1 );
		x      = x + ( y << 1 );
		y      = yi;
		theta += *arctanptr++;  /* Add angle */
	}

	i = 0;
	do {
		if ( y < 0 ) {
			/* Rotate positive */
			yi     = y + ( x >> i );
			x      = x - ( y >> i );
			y      = yi;
			theta -= *arctanptr++;
		} else {
			/* Rotate negative */
			yi     = y - ( x >> i );
			x      = x + ( y >> i );
			y      = yi;
			theta += *arctanptr++;
		}
	}
	while ( ++i < GF_TRIG_MAX_ITERS );

	/* round theta */
	if ( theta >= 0 ) 
		theta = GF_PAD_ROUND( theta, 32 );
	else
		theta = - GF_PAD_ROUND( -theta, 32 );

	vec->x = x;
	vec->y = ANGLE_DEG_TO_RAD(theta);
}

Fixed gf_cos(Fixed angle)
{
	GF_Point2D v;
	v.x = GF_TRIG_COSCALE >> 2;
	v.y = 0;
	gf_trig_pseudo_rotate( &v, angle );
	return v.x / ( 1 << 12 );
}

Fixed gf_sin(Fixed angle)
{
	return gf_cos( GF_PI2 - angle );
}


Fixed gf_tan(Fixed angle)
{
	GF_Point2D v;
    v.x = GF_TRIG_COSCALE >> 2;
    v.y = 0;
    gf_trig_pseudo_rotate( &v, angle );
    return gf_divfix( v.y, v.x );
}

Fixed gf_atan2(Fixed dy, Fixed dx)
{
	GF_Point2D v;
	if ( dx == 0 && dy == 0 ) return 0;
	v.x = dx;
	v.y = dy;
	gf_trig_prenorm( &v );
	gf_trig_pseudo_polarize( &v );
	return v.y;
}

/*FIXME*/
Fixed gf_acos(Fixed angle)
{
	return FLT2FIX( (Float) acos(FIX2FLT(angle)));
}

/*FIXME*/
Fixed gf_asin(Fixed angle)
{
	return FLT2FIX( (Float) asin(FIX2FLT(angle)));
}


/* these macros return 0 for positive numbers, and -1 for negative ones */
#define GF_SIGN_LONG( x )   ( (x) >> ( 32 - 1 ) )
#define GF_SIGN_INT( x )    ( (x) >> ( 32 - 1 ) )
#define GF_SIGN_INT32( x )  ( (x) >> 31 )
#define GF_SIGN_INT16( x )  ( (x) >> 15 )

static void gf_v2d_rotate(GF_Point2D *vec, Fixed angle)
{
    s32 shift;
    GF_Point2D v;
    
	v.x   = vec->x;
    v.y   = vec->y;
	
    if ( angle && ( v.x != 0 || v.y != 0 ) ) {
		shift = gf_trig_prenorm( &v );
		gf_trig_pseudo_rotate( &v, angle );
		v.x = gf_trig_downscale( v.x );
		v.y = gf_trig_downscale( v.y );

		if ( shift > 0 ) {
			s32 half = 1L << ( shift - 1 );

			vec->x = ( v.x + half + GF_SIGN_LONG( v.x ) ) >> shift;
			vec->y = ( v.y + half + GF_SIGN_LONG( v.y ) ) >> shift;
		} else {
			shift  = -shift;
			vec->x = v.x << shift;
			vec->y = v.y << shift;
		}
    }
}

Fixed gf_v2d_len(GF_Point2D *vec)
{
    s32 shift;
	GF_Point2D v;
    v = *vec;

    /* handle trivial cases */
    if ( v.x == 0 ) {
		return ( v.y >= 0 ) ? v.y : -v.y;
    } else if ( v.y == 0 ) {
		return ( v.x >= 0 ) ? v.x : -v.x;
    }

    /* general case */
    shift = gf_trig_prenorm( &v );
    gf_trig_pseudo_polarize( &v );
    v.x = gf_trig_downscale( v.x );
    if ( shift > 0 )
		return ( v.x + ( 1 << ( shift - 1 ) ) ) >> shift;

    return v.x << -shift;
}


void gf_v2d_polarize(GF_Point2D *vec, Fixed *length, Fixed *angle)
{
    s32 shift;
	GF_Point2D v;
    v = *vec;
    if ( v.x == 0 && v.y == 0 )
		return;

    shift = gf_trig_prenorm( &v );
    gf_trig_pseudo_polarize( &v );
    v.x = gf_trig_downscale( v.x );
    *length = ( shift >= 0 ) ? ( v.x >> shift ) : ( v.x << -shift );
    *angle  = v.y;
}

GF_Point2D gf_v2d_from_polar(Fixed length, Fixed angle)
{
	GF_Point2D vec;
    vec.x = length;
    vec.y = 0;
    gf_v2d_rotate(&vec, angle);
	return vec;
}

#else


GF_Point2D gf_v2d_from_polar(Fixed length, Fixed angle)
{
	GF_Point2D vec;
    vec.x = length*(Float) cos(angle);
    vec.y = length*(Float) sin(angle);
	return vec;
}

Fixed gf_v2d_len(GF_Point2D *vec)
{
	if (!vec->x) return ABS(vec->y);
	if (!vec->y) return ABS(vec->x);
	return (Fixed) sqrt(vec->x*vec->x + vec->y*vec->y);
}

/*

Fixed gf_cos(_a) { return (Float) cos(_a); }
Fixed gf_sin(_a) { return (Float) sin(_a); }
Fixed gf_tan(_a) { return (Float) tan(_a); }
Fixed gf_atan2(_y, _x) { return (Float) atan2(_y, _x); }
Fixed gf_sqrt(_x) { return (Float) sqrt(_x); }
Fixed gf_ceil(_a) { return (Float) ceil(_a); }
Fixed gf_floor(_a) { return (Float) floor(_a); }
Fixed gf_acos(_a) { return (Float) acos(_a); }
Fixed gf_asin(_a) { return (Float) asin(_a); }

*/

#endif


Fixed gf_angle_diff(Fixed angle1, Fixed angle2)
{
    Fixed delta = angle2 - angle1;
#ifdef GPAC_FIXED_POINT
    delta %= GF_2PI;
	if (delta < 0) delta += GF_2PI;
    if (delta > GF_PI) delta -= GF_2PI;
#else
	while (delta < 0) delta += GF_2PI;
    while (delta > GF_PI) delta -= GF_2PI;
#endif
    return delta;
}



/*
	2D MATRIX TOOLS
 */

void gf_mx2d_add_matrix(GF_Matrix2D *_this, GF_Matrix2D *from)
{
	GF_Matrix2D bck;
	if (!_this || !from) return;

	if (gf_mx2d_is_identity(*from)) return;
	else if (gf_mx2d_is_identity(*_this)) {
		gf_mx2d_copy(*_this, *from);
		return;
	}
	gf_mx2d_copy(bck, *_this);
	_this->m[0] = gf_mulfix(from->m[0], bck.m[0]) + gf_mulfix(from->m[1], bck.m[3]);
	_this->m[1] = gf_mulfix(from->m[0], bck.m[1]) + gf_mulfix(from->m[1], bck.m[4]);
	_this->m[2] = gf_mulfix(from->m[0], bck.m[2]) + gf_mulfix(from->m[1], bck.m[5]) + from->m[2];
	_this->m[3] = gf_mulfix(from->m[3], bck.m[0]) + gf_mulfix(from->m[4], bck.m[3]);
	_this->m[4] = gf_mulfix(from->m[3], bck.m[1]) + gf_mulfix(from->m[4], bck.m[4]);
	_this->m[5] = gf_mulfix(from->m[3], bck.m[2]) + gf_mulfix(from->m[4], bck.m[5]) + from->m[5];
}

void gf_mx2d_add_translation(GF_Matrix2D *_this, Fixed cx, Fixed cy)
{
	GF_Matrix2D tmp;
	if (!_this || (!cx && !cy) ) return;
	gf_mx2d_init(tmp);
	tmp.m[2] = cx;
	tmp.m[5] = cy;
	gf_mx2d_add_matrix(_this, &tmp);
}


void gf_mx2d_add_rotation(GF_Matrix2D *_this, Fixed cx, Fixed cy, Fixed angle)
{
	GF_Matrix2D tmp;
	if (!_this) return;
	gf_mx2d_init(tmp);

	gf_mx2d_add_translation(_this, -cx, -cy);
	
	tmp.m[0] = gf_cos(angle);
	tmp.m[4] = tmp.m[0];
	tmp.m[3] = gf_sin(angle);
	tmp.m[1] = -1 * tmp.m[3];
	gf_mx2d_add_matrix(_this, &tmp);
	gf_mx2d_add_translation(_this, cx, cy);
}

void gf_mx2d_add_scale(GF_Matrix2D *_this, Fixed scale_x, Fixed scale_y)
{
	GF_Matrix2D tmp;
	if (!_this || ((scale_x==FIX_ONE) && (scale_y==FIX_ONE)) ) return;
	gf_mx2d_init(tmp);
	tmp.m[0] = scale_x;
	tmp.m[4] = scale_y;
	gf_mx2d_add_matrix(_this, &tmp);
}

void gf_mx2d_add_scale_at(GF_Matrix2D *_this, Fixed scale_x, Fixed scale_y, Fixed cx, Fixed cy, Fixed angle)
{
	GF_Matrix2D tmp;
	if (!_this) return;
	gf_mx2d_init(tmp);
	if (angle) {
		gf_mx2d_add_rotation(_this, cx, cy, -angle);
	}
	tmp.m[0] = scale_x;
	tmp.m[4] = scale_y;
	gf_mx2d_add_matrix(_this, &tmp);
	if (angle) gf_mx2d_add_rotation(_this, cx, cy, angle);
}

void gf_mx2d_add_skew(GF_Matrix2D *_this, Fixed skew_x, Fixed skew_y)
{
	GF_Matrix2D tmp;
	if (!_this || (!skew_x && !skew_y) ) return;
	gf_mx2d_init(tmp);
	tmp.m[1] = skew_x;
	tmp.m[3] = skew_y;
	gf_mx2d_add_matrix(_this, &tmp);
}

void gf_mx2d_add_skew_x(GF_Matrix2D *_this, Fixed angle)
{
	GF_Matrix2D tmp;
	if (!_this) return;
	gf_mx2d_init(tmp);
	tmp.m[1] = 0;
	tmp.m[3] = gf_tan(angle);
	gf_mx2d_add_matrix(_this, &tmp);
}

void gf_mx2d_add_skew_y(GF_Matrix2D *_this, Fixed angle)
{
	GF_Matrix2D tmp;
	if (!_this) return;
	gf_mx2d_init(tmp);
	tmp.m[1] = gf_tan(angle);
	tmp.m[3] = 0;
	gf_mx2d_add_matrix(_this, &tmp);
}

Fixed gf_mx2d_get_determinent(GF_Matrix2D *_this)
{
	if (_this)
		return gf_mulfix(_this->m[0], _this->m[4]) - gf_mulfix(_this->m[1], _this->m[3]);
	return 0;
}

void gf_mx2d_inverse(GF_Matrix2D *_this)
{
	Fixed det;
	GF_Matrix2D tmp;
	if(!_this) return;
	if (gf_mx2d_is_identity(*_this)) return;
	det = gf_mx2d_get_determinent(_this);
	if (!det) {
		gf_mx2d_init(*_this);
		return;
	}
	tmp.m[0] = gf_divfix(_this->m[4], det);
	tmp.m[1] = -1 *  gf_divfix(_this->m[1], det);
	tmp.m[2] = gf_mulfix(_this->m[1], _this->m[5]) - gf_mulfix(_this->m[4], _this->m[2]);
	tmp.m[2] = gf_divfix(tmp.m[2], det);
	tmp.m[3] = -1 * gf_divfix(_this->m[3], det);
	tmp.m[4] = gf_divfix(_this->m[0], det);
	tmp.m[5] = gf_mulfix(_this->m[3], _this->m[2]) - gf_mulfix(_this->m[0],_this->m[5]);
	tmp.m[5] = gf_divfix(tmp.m[5], det);
	gf_mx2d_copy(*_this, tmp);
}

void gf_mx2d_apply_coords(GF_Matrix2D *_this, Fixed *x, Fixed *y)
{
	Fixed _x, _y;
	if (!_this || !x || !y) return;
	_x = gf_mulfix(*x, _this->m[0]) + gf_mulfix(*y, _this->m[1]) + _this->m[2];
	_y = gf_mulfix(*x, _this->m[3]) + gf_mulfix(*y, _this->m[4]) + _this->m[5];
	*x = _x;
	*y = _y;
}

void gf_mx2d_apply_point(GF_Matrix2D *_this, GF_Point2D *pt)
{
	gf_mx2d_apply_coords(_this, &pt->x, &pt->y);
}

void gf_mx2d_apply_rect(GF_Matrix2D *_this, GF_Rect *rc)
{
	GF_Point2D c1, c2, c3, c4;
	c1.x = c2.x = rc->x;
	c3.x = c4.x = rc->x + rc->width;
	c1.y = c3.y = rc->y;
	c2.y = c4.y = rc->y - rc->height;
	gf_mx2d_apply_point(_this, &c1);
	gf_mx2d_apply_point(_this, &c2);
	gf_mx2d_apply_point(_this, &c3);
	gf_mx2d_apply_point(_this, &c4);
	rc->x = MIN(c1.x, MIN(c2.x, MIN(c3.x, c4.x)));
	rc->width = MAX(c1.x, MAX(c2.x, MAX(c3.x, c4.x))) - rc->x;
	rc->height = MIN(c1.y, MIN(c2.y, MIN(c3.y, c4.y)));
	rc->y = MAX(c1.y, MAX(c2.y, MAX(c3.y, c4.y)));
	rc->height = rc->y - rc->height;
	assert(rc->height>=0);
	assert(rc->width>=0);
}


/*
	COLOR MATRIX TOOLS
 */

void gf_cmx_init(GF_ColorMatrix *_this)
{
	if (!_this) return;
	memset(_this->m, 0, sizeof(Fixed)*20);
	_this->m[0] = _this->m[6] = _this->m[12] = _this->m[18] = FIX_ONE;
	_this->identity = 1;
}


static void gf_cmx_identity(GF_ColorMatrix *_this)
{
	GF_ColorMatrix mat;
	gf_cmx_init(&mat);
	_this->identity = memcmp(_this->m, mat.m, sizeof(Fixed)*20) ? 0 : 1;
}

void gf_cmx_set_all(GF_ColorMatrix *_this, Fixed *coefs)
{
	if (!_this || !coefs) return;
}

void gf_cmx_set(GF_ColorMatrix *_this, 
				 Fixed c1, Fixed c2, Fixed c3, Fixed c4, Fixed c5,
				 Fixed c6, Fixed c7, Fixed c8, Fixed c9, Fixed c10,
				 Fixed c11, Fixed c12, Fixed c13, Fixed c14, Fixed c15,
				 Fixed c16, Fixed c17, Fixed c18, Fixed c19, Fixed c20)
{
	if (!_this) return;
	_this->m[0] = c1; _this->m[1] = c2; _this->m[2] = c3; _this->m[3] = c4; _this->m[4] = c5;
	_this->m[5] = c6; _this->m[6] = c7; _this->m[7] = c8; _this->m[8] = c9; _this->m[9] = c10;
	_this->m[10] = c11; _this->m[11] = c12; _this->m[12] = c13; _this->m[13] = c14; _this->m[14] = c15;
	_this->m[15] = c16; _this->m[16] = c17; _this->m[17] = c18; _this->m[18] = c19; _this->m[19] = c20;
	gf_cmx_identity(_this);
}

void gf_cmx_copy(GF_ColorMatrix *_this, GF_ColorMatrix *from)
{
	if (!_this || !from) return;
	memcpy(_this->m, from->m, sizeof(Fixed)*20);
	gf_cmx_identity(_this);
}


void gf_cmx_multiply(GF_ColorMatrix *_this, GF_ColorMatrix *w)
{
	Fixed res[20];
	if (!_this || !w || w->identity) return;
	if (_this->identity) {
		gf_cmx_copy(_this, w);
		return;
	}

	res[0] = gf_mulfix(_this->m[0], w->m[0]) + gf_mulfix(_this->m[1], w->m[5]) + gf_mulfix(_this->m[2], w->m[10]) + gf_mulfix(_this->m[3], w->m[15]);
	res[1] = gf_mulfix(_this->m[0], w->m[1]) + gf_mulfix(_this->m[1], w->m[6]) + gf_mulfix(_this->m[2], w->m[11]) + gf_mulfix(_this->m[3], w->m[16]);
	res[2] = gf_mulfix(_this->m[0], w->m[2]) + gf_mulfix(_this->m[1], w->m[7]) + gf_mulfix(_this->m[2], w->m[12]) + gf_mulfix(_this->m[3], w->m[17]);
	res[3] = gf_mulfix(_this->m[0], w->m[3]) + gf_mulfix(_this->m[1], w->m[8]) + gf_mulfix(_this->m[2], w->m[13]) + gf_mulfix(_this->m[3], w->m[18]);
	res[4] = gf_mulfix(_this->m[0], w->m[4]) + gf_mulfix(_this->m[1], w->m[9]) + gf_mulfix(_this->m[2], w->m[14]) + gf_mulfix(_this->m[3], w->m[19]) + _this->m[4];
	
	res[5] = gf_mulfix(_this->m[5], w->m[0]) + gf_mulfix(_this->m[6], w->m[5]) + gf_mulfix(_this->m[7], w->m[10]) + gf_mulfix(_this->m[8], w->m[15]);
	res[6] = gf_mulfix(_this->m[5], w->m[1]) + gf_mulfix(_this->m[6], w->m[6]) + gf_mulfix(_this->m[7], w->m[11]) + gf_mulfix(_this->m[8], w->m[16]);
	res[7] = gf_mulfix(_this->m[5], w->m[2]) + gf_mulfix(_this->m[6], w->m[7]) + gf_mulfix(_this->m[7], w->m[12]) + gf_mulfix(_this->m[8], w->m[17]);
	res[8] = gf_mulfix(_this->m[5], w->m[3]) + gf_mulfix(_this->m[6], w->m[8]) + gf_mulfix(_this->m[7], w->m[13]) + gf_mulfix(_this->m[8], w->m[18]);
	res[9] = gf_mulfix(_this->m[5], w->m[4]) + gf_mulfix(_this->m[6], w->m[9]) + gf_mulfix(_this->m[7], w->m[14]) + gf_mulfix(_this->m[8], w->m[19]) + _this->m[9];
	
	res[10] = gf_mulfix(_this->m[10], w->m[0]) + gf_mulfix(_this->m[11], w->m[5]) + gf_mulfix(_this->m[12], w->m[10]) + gf_mulfix(_this->m[13], w->m[15]);
	res[11] = gf_mulfix(_this->m[10], w->m[1]) + gf_mulfix(_this->m[11], w->m[6]) + gf_mulfix(_this->m[12], w->m[11]) + gf_mulfix(_this->m[13], w->m[16]);
	res[12] = gf_mulfix(_this->m[10], w->m[2]) + gf_mulfix(_this->m[11], w->m[7]) + gf_mulfix(_this->m[12], w->m[12]) + gf_mulfix(_this->m[13], w->m[17]);
	res[13] = gf_mulfix(_this->m[10], w->m[3]) + gf_mulfix(_this->m[11], w->m[8]) + gf_mulfix(_this->m[12], w->m[13]) + gf_mulfix(_this->m[13], w->m[18]);
	res[14] = gf_mulfix(_this->m[10], w->m[4]) + gf_mulfix(_this->m[11], w->m[9]) + gf_mulfix(_this->m[12], w->m[14]) + gf_mulfix(_this->m[13], w->m[19]) + _this->m[14];
	
	res[15] = gf_mulfix(_this->m[15], w->m[0]) + gf_mulfix(_this->m[16], w->m[5]) + gf_mulfix(_this->m[17], w->m[10]) + gf_mulfix(_this->m[18], w->m[15]);
	res[16] = gf_mulfix(_this->m[15], w->m[1]) + gf_mulfix(_this->m[16], w->m[6]) + gf_mulfix(_this->m[17], w->m[11]) + gf_mulfix(_this->m[18], w->m[16]);
	res[17] = gf_mulfix(_this->m[15], w->m[2]) + gf_mulfix(_this->m[16], w->m[7]) + gf_mulfix(_this->m[17], w->m[12]) + gf_mulfix(_this->m[18], w->m[17]);
	res[18] = gf_mulfix(_this->m[15], w->m[3]) + gf_mulfix(_this->m[16], w->m[8]) + gf_mulfix(_this->m[17], w->m[13]) + gf_mulfix(_this->m[18], w->m[18]);
	res[19] = gf_mulfix(_this->m[15], w->m[4]) + gf_mulfix(_this->m[16], w->m[9]) + gf_mulfix(_this->m[17], w->m[14]) + gf_mulfix(_this->m[18], w->m[19]) + _this->m[19];
	memcpy(_this->m, res, sizeof(Fixed)*20);
	gf_cmx_identity(_this);
}

#define CLIP_COMP(val)	{ if (val<0) { val=0; } else if (val>FIX_ONE) { val=FIX_ONE;} }


GF_Color gf_cmx_apply(GF_ColorMatrix *_this, GF_Color col)
{
	Fixed _a, _r, _g, _b, a, r, g, b;
	if (!_this || _this->identity) return col;

	a = INT2FIX(col>>24); a /= 255;
	r = INT2FIX((col>>16)&0xFF); r /= 255;
	g = INT2FIX((col>>8)&0xFF); g /= 255;
	b = INT2FIX((col)&0xFF); b /= 255;
	_r = gf_mulfix(r, _this->m[0]) + gf_mulfix(g, _this->m[1]) + gf_mulfix(b, _this->m[2]) + gf_mulfix(a, _this->m[3]) + _this->m[4];
	_g = gf_mulfix(r, _this->m[5]) + gf_mulfix(g, _this->m[6]) + gf_mulfix(b, _this->m[7]) + gf_mulfix(a, _this->m[8]) + _this->m[9];
	_b = gf_mulfix(r, _this->m[10]) + gf_mulfix(g, _this->m[11]) + gf_mulfix(b, _this->m[12]) + gf_mulfix(a, _this->m[13]) + _this->m[14];
	_a = gf_mulfix(r, _this->m[15]) + gf_mulfix(g, _this->m[16]) + gf_mulfix(b, _this->m[17]) + gf_mulfix(a, _this->m[18]) + _this->m[19];
	CLIP_COMP(_a);
	CLIP_COMP(_r);
	CLIP_COMP(_g);
	CLIP_COMP(_b);
	return GF_COL_ARGB(FIX2INT(_a*255),FIX2INT(_r*255),FIX2INT(_g*255),FIX2INT(_b*255));
}

void gf_cmx_apply_fixed(GF_ColorMatrix *_this, Fixed *a, Fixed *r, Fixed *g, Fixed *b)
{
	u32 col = GF_COL_ARGB_FIXED(*a, *r, *g, *b);
	col = gf_cmx_apply(_this, col);
	*a = INT2FIX(GF_COL_A(col)) / 255;
	*r = INT2FIX(GF_COL_R(col)) / 255;
	*g = INT2FIX(GF_COL_G(col)) / 255;
	*b = INT2FIX(GF_COL_B(col)) / 255;
}

/*
	RECTANGLE TOOLS
 */

/*transform rect to smallest covering integer pixels rect - this is needed to make sure clearing
of screen is correctly handled, otherwise we have troubles with bitmap hardware blitting (always integer)*/
GF_IRect gf_rect_pixelize(GF_Rect *r)
{
	GF_IRect rc;
	rc.x = FIX2INT(gf_floor(r->x));
	rc.y = FIX2INT(gf_ceil(r->y));
	rc.width = FIX2INT(gf_ceil(r->x + r->width)) - rc.x;
	rc.height = rc.y - FIX2INT(gf_floor(r->y - r->height));
	return rc;
}


/*adds @rc2 to @rc1 - the new @rc1 contains the old @rc1 and @rc2*/
void gf_rect_union(GF_Rect *rc1, GF_Rect *rc2) 
{
	if (!rc1->width || !rc1->height) {*rc1=*rc2; return;}
	if (rc2->x < rc1->x) { rc1->width += rc1->x - rc2->x; rc1->x = rc2->x; }
	if (rc2->x + rc2->width > rc1->x+rc1->width) rc1->width = rc2->x + rc2->width - rc1->x;
	if (rc2->y > rc1->y) { rc1->height += rc2->y - rc1->y; rc1->y = rc2->y; }
	if (rc2->y - rc2->height < rc1->y - rc1->height) rc1->height = rc1->y - rc2->y + rc2->height;
}

GF_Rect gf_rect_center(Fixed w, Fixed h)
{
	GF_Rect rc;
	rc.x=-w/2; rc.y=h/2; rc.width=w; rc.height=h;
	return rc;
}

Bool gf_rect_overlaps(GF_Rect rc1, GF_Rect rc2)
{
	if (! rc2.height || !rc2.width || !rc1.height || !rc1.width) return 0;
	if (rc2.x+rc2.width<=rc1.x) return 0;
	if (rc2.x>=rc1.x+rc1.width) return 0;
	if (rc2.y-rc2.height>=rc1.y) return 0;
	if (rc2.y<=rc1.y-rc1.height) return 0;
	return 1;
}

Bool gf_rect_equal(GF_Rect rc1, GF_Rect rc2) 
{ 
	if ( (rc1.x == rc2.x)  && (rc1.y == rc2.y) && (rc1.width == rc2.width) && (rc1.height == rc2.height) )
		return 1;
	return 0;
}

#ifdef GPAC_FIXED_POINT

/* check if dimension is larger than FIX_ONE*/
#define IS_HIGH_DIM(_v)	((_v > FIX_ONE) || (_v < (s32)0xFFFF0000))
/* check if any vector dimension is larger than FIX_ONE*/
#define VEC_HIGH_MAG(_v)	(IS_HIGH_DIM(_v.x) || IS_HIGH_DIM(_v.y) || IS_HIGH_DIM(_v.z) )

Fixed gf_vec_len(GF_Vec v)
{
#if 1
	/*high-magnitude vector, use low precision on frac part to avoid overflow*/
	if (VEC_HIGH_MAG(v)) {
		v.x>>=8;
		v.y>>=8;
		v.z>>=8;
		return gf_sqrt( gf_mulfix(v.x, v.x) + gf_mulfix(v.y, v.y) + gf_mulfix(v.z, v.z) ) << 8;
	}
	/*low-res vector*/
	else {
		return gf_sqrt( gf_mulfix(v.x, v.x) + gf_mulfix(v.y, v.y) + gf_mulfix(v.z, v.z) );
	}
#else
	Float x, y, z;
	x = FIX2FLT(v.x);
	y = FIX2FLT(v.y);
	z = FIX2FLT(v.z);
	return FLT2FIX(sqrt(x*x + y*y + z*z));
#endif
}

Fixed gf_vec_lensq(GF_Vec v)
{
	/*high-magnitude vector, use low precision on frac part to avoid overflow*/
	if (VEC_HIGH_MAG(v)) {
		v.x>>=8;
		v.y>>=8;
		v.z>>=8;
		return ( gf_mulfix(v.x, v.x) + gf_mulfix(v.y, v.y) + gf_mulfix(v.z, v.z) ) << 16;
	} else {
		return gf_mulfix(v.x, v.x) + gf_mulfix(v.y, v.y) + gf_mulfix(v.z, v.z);
	}
}

Fixed gf_vec_dot(GF_Vec v1, GF_Vec v2)
{
#if 1
	/*both high-magnitude vectors, use low precision on frac part to avoid overflow
	if only one is, the dot product should still be in proper range*/
	if (VEC_HIGH_MAG(v1) && VEC_HIGH_MAG(v2)) {
		v1.x>>=8; v1.y>>=8; v1.z>>=8;
		v2.x>>=8; v2.y>>=8; v2.z>>=8;
		return ( gf_mulfix(v1.x, v2.x) + gf_mulfix(v1.y, v2.y) + gf_mulfix(v1.z, v2.z) ) << 16;
	} else {
		return gf_mulfix(v1.x, v2.x) + gf_mulfix(v1.y, v2.y) + gf_mulfix(v1.z, v2.z);
	}
#else
	Float v1x, v1y, v1z, v2x, v2y, v2z;
	v1x = FIX2FLT(v1.x);
	v1y = FIX2FLT(v1.y);
	v1z = FIX2FLT(v1.z);
	v2x = FIX2FLT(v2.x);
	v2y = FIX2FLT(v2.y);
	v2z = FIX2FLT(v2.z);
	return FLT2FIX(v1x*v2x + v1y*v2y + v1z*v2z);
#endif
}

void gf_vec_norm(GF_Vec *v)
{
	Fixed __res = gf_vec_len(*v);
	v->x = gf_divfix(v->x, __res); 
	v->y = gf_divfix(v->y, __res);
	v->z = gf_divfix(v->z, __res);
}

GF_Vec gf_vec_scale(GF_Vec v, Fixed f)
{
	GF_Vec res;
	res.x = gf_mulfix(v.x, f); 
	res.y = gf_mulfix(v.y, f); 
	res.z = gf_mulfix(v.z, f);
	return res;
}

GF_Vec gf_vec_cross(GF_Vec v1, GF_Vec v2)
{
	GF_Vec res;
#if 1
	/*both high-magnitude vectors, use low precision on frac part to avoid overflow
	if only one is, the cross product should still be in proper range*/
	if (VEC_HIGH_MAG(v1) && VEC_HIGH_MAG(v2)) {
		v1.x>>=8; v1.y>>=8; v1.z>>=8;
		v2.x>>=8; v2.y>>=8; v2.z>>=8;
		res.x = gf_mulfix(v1.y, v2.z) - gf_mulfix(v2.y, v1.z);
		res.y = gf_mulfix(v2.x, v1.z) - gf_mulfix(v1.x, v2.z);
		res.z = gf_mulfix(v1.x, v2.y) - gf_mulfix(v2.x, v1.y);
		res.x<<=16;
		res.y<<=16;
		res.z<<=16;
	} else {
		res.x = gf_mulfix(v1.y, v2.z) - gf_mulfix(v2.y, v1.z);
		res.y = gf_mulfix(v2.x, v1.z) - gf_mulfix(v1.x, v2.z);
		res.z = gf_mulfix(v1.x, v2.y) - gf_mulfix(v2.x, v1.y);
	}
#else
	Float v1x, v1y, v1z, v2x, v2y, v2z;
	v1x = FIX2FLT(v1.x);
	v1y = FIX2FLT(v1.y);
	v1z = FIX2FLT(v1.z);
	v2x = FIX2FLT(v2.x);
	v2y = FIX2FLT(v2.y);
	v2z = FIX2FLT(v2.z);
	res.x = FLT2FIX( v1y*v2z - v2y*v1z);
	res.y = FLT2FIX( v2x*v1z - v1x*v2z);
	res.z = FLT2FIX( v1x*v2y - v2x*v1y);
#endif
	return res;
}

#else

Fixed gf_vec_len(GF_Vec v) { return gf_sqrt(v.x*v.x + v.y*v.y + v.z*v.z); }
Fixed gf_vec_lensq(GF_Vec v) { return v.x*v.x + v.y*v.y + v.z*v.z; }
Fixed gf_vec_dot(GF_Vec v1, GF_Vec v2) { return v1.x*v2.x + v1.y*v2.y + v1.z*v2.z; }
void gf_vec_norm(GF_Vec *v)
{
	Fixed __res = gf_vec_len(*v);
	if (__res ) __res = 1.0f/__res ;
	v->x *= __res; 
	v->y *= __res;
	v->z *= __res;
}

GF_Vec gf_vec_scale(GF_Vec v, Fixed f)
{
	GF_Vec res = v;
	res.x *= f; 
	res.y *= f; 
	res.z *= f;
	return res;
}

GF_Vec gf_vec_cross(GF_Vec v1, GF_Vec v2)
{
	GF_Vec res;
	res.x = v1.y*v2.z - v2.y*v1.z;
	res.y = v2.x*v1.z - v1.x*v2.z;
	res.z = v1.x*v2.y - v2.x*v1.y;
	return res;
}

#endif


void gf_mx2d_from_mx(GF_Matrix2D *mat2D, GF_Matrix *mat)
{
	gf_mx2d_init(*mat2D);
	mat2D->m[0] = mat->m[0];
	mat2D->m[1] = mat->m[4];
	mat2D->m[2] = mat->m[12];
	mat2D->m[3] = mat->m[1];
	mat2D->m[4] = mat->m[5];
	mat2D->m[5] = mat->m[13];
}

void gf_mx_apply_rect(GF_Matrix *mat, GF_Rect *rc)
{
	GF_Matrix2D mat2D;
	gf_mx2d_from_mx(&mat2D, mat);
	gf_mx2d_apply_rect(&mat2D, rc);
}

void gf_mx_add_matrix(GF_Matrix *mat, GF_Matrix *mul)
{
    GF_Matrix tmp;
	gf_mx_init(tmp);

    tmp.m[0] = gf_mulfix(mat->m[0],mul->m[0]) + gf_mulfix(mat->m[4],mul->m[1]) + gf_mulfix(mat->m[8],mul->m[2]);
    tmp.m[4] = gf_mulfix(mat->m[0],mul->m[4]) + gf_mulfix(mat->m[4],mul->m[5]) + gf_mulfix(mat->m[8],mul->m[6]);
    tmp.m[8] = gf_mulfix(mat->m[0],mul->m[8]) + gf_mulfix(mat->m[4],mul->m[9]) + gf_mulfix(mat->m[8],mul->m[10]);
    tmp.m[12]= gf_mulfix(mat->m[0],mul->m[12]) + gf_mulfix(mat->m[4],mul->m[13]) + gf_mulfix(mat->m[8],mul->m[14]) + mat->m[12];
    tmp.m[1] = gf_mulfix(mat->m[1],mul->m[0]) + gf_mulfix(mat->m[5],mul->m[1]) + gf_mulfix(mat->m[9],mul->m[2]);
    tmp.m[5] = gf_mulfix(mat->m[1],mul->m[4]) + gf_mulfix(mat->m[5],mul->m[5]) + gf_mulfix(mat->m[9],mul->m[6]);
    tmp.m[9] = gf_mulfix(mat->m[1],mul->m[8]) + gf_mulfix(mat->m[5],mul->m[9]) + gf_mulfix(mat->m[9],mul->m[10]);
    tmp.m[13]= gf_mulfix(mat->m[1],mul->m[12]) + gf_mulfix(mat->m[5],mul->m[13]) + gf_mulfix(mat->m[9],mul->m[14]) + mat->m[13];
    tmp.m[2] = gf_mulfix(mat->m[2],mul->m[0]) + gf_mulfix(mat->m[6],mul->m[1]) + gf_mulfix(mat->m[10],mul->m[2]);
    tmp.m[6] = gf_mulfix(mat->m[2],mul->m[4]) + gf_mulfix(mat->m[6],mul->m[5]) + gf_mulfix(mat->m[10],mul->m[6]);
    tmp.m[10]= gf_mulfix(mat->m[2],mul->m[8]) + gf_mulfix(mat->m[6],mul->m[9]) + gf_mulfix(mat->m[10],mul->m[10]);
    tmp.m[14]= gf_mulfix(mat->m[2],mul->m[12]) + gf_mulfix(mat->m[6],mul->m[13]) + gf_mulfix(mat->m[10],mul->m[14]) + mat->m[14];
	memcpy(mat->m, tmp.m, sizeof(Fixed)*16);
}

void gf_mx_add_translation(GF_Matrix *mat, Fixed tx, Fixed ty, Fixed tz)
{
	Fixed tmp[3];
	u32 i;
	tmp[0] = mat->m[12];
	tmp[1] = mat->m[13];
	tmp[2] = mat->m[14];
	for (i=0; i<3; i++)
		tmp[i] += (gf_mulfix(tx,mat->m[i]) + gf_mulfix(ty,mat->m[i+4]) + gf_mulfix(tz, mat->m[i + 8]));
	mat->m[12] = tmp[0];
	mat->m[13] = tmp[1];
	mat->m[14] = tmp[2];
}

void gf_mx_add_scale(GF_Matrix *mat, Fixed sx, Fixed sy, Fixed sz)
{
	Fixed tmp[3];
	u32 i, j;

	tmp[0] = sx; 
	tmp[1] = sy; 
	tmp[2] = sz; 

	for (i=0; i<3; i++) {
		for (j=0; j<3; j++) {
			mat->m[i*4 + j] = gf_mulfix(mat->m[j+4 * i], tmp[i]);
		}
	}
}

void gf_mx_add_rotation(GF_Matrix *mat, Fixed angle, Fixed x, Fixed y, Fixed z)
{
	GF_Matrix tmp;
	Fixed xx, yy, zz, xy, xz, yz;
	Fixed nor = gf_sqrt(gf_mulfix(x,x) + gf_mulfix(y,y) + gf_mulfix(z,z));
	Fixed cos_a = gf_cos(angle);
	Fixed sin_a = gf_sin(angle);
	Fixed icos_a = FIX_ONE - cos_a;

	if (nor && (nor!=FIX_ONE)) { 
		x = gf_divfix(x, nor); 
		y = gf_divfix(y, nor); 
		z = gf_divfix(z, nor); 
	}
	xx = gf_mulfix(x,x); yy = gf_mulfix(y,y); zz = gf_mulfix(z,z); xy = gf_mulfix(x,y); xz = gf_mulfix(x,z); yz = gf_mulfix(y,z);
	gf_mx_init(tmp);
    tmp.m[0] = gf_mulfix(icos_a,xx) + cos_a;
    tmp.m[1] = gf_mulfix(xy,icos_a) + gf_mulfix(z,sin_a);
    tmp.m[2] = gf_mulfix(xz,icos_a) - gf_mulfix(y,sin_a);
    
	tmp.m[4] = gf_mulfix(xy,icos_a) - gf_mulfix(z,sin_a);
    tmp.m[5] = gf_mulfix(icos_a,yy) + cos_a;
    tmp.m[6] = gf_mulfix(yz,icos_a) + gf_mulfix(x,sin_a);

	tmp.m[8] = gf_mulfix(xz,icos_a) + gf_mulfix(y,sin_a);
    tmp.m[9] = gf_mulfix(yz,icos_a) - gf_mulfix(x,sin_a);
    tmp.m[10]= gf_mulfix(icos_a,zz) + cos_a;

	gf_mx_add_matrix(mat, &tmp);
}

void gf_mx_from_mx2d(GF_Matrix *mat, GF_Matrix2D *mat2D)
{
	gf_mx_init(*mat);
	mat->m[0] = mat2D->m[0];
	mat->m[4] = mat2D->m[1];
	mat->m[12] = mat2D->m[2];
	mat->m[1] = mat2D->m[3];
	mat->m[5] = mat2D->m[4];
	mat->m[13] = mat2D->m[5];
}

Bool gf_mx_equal(GF_Matrix *mx1, GF_Matrix *mx2)
{
	if (mx1->m[0] != mx2->m[0]) return 0;
	if (mx1->m[1] != mx2->m[1]) return 0;
	if (mx1->m[2] != mx2->m[2]) return 0;
	if (mx1->m[4] != mx2->m[4]) return 0;
	if (mx1->m[5] != mx2->m[5]) return 0;
	if (mx1->m[6] != mx2->m[6]) return 0;
	if (mx1->m[8] != mx2->m[8]) return 0;
	if (mx1->m[9] != mx2->m[9]) return 0;
	if (mx1->m[10] != mx2->m[10]) return 0;
	if (mx1->m[12] != mx2->m[12]) return 0;
	if (mx1->m[13] != mx2->m[13]) return 0;
	if (mx1->m[14] != mx2->m[14]) return 0;
	return 1;
}


void gf_mx_inverse(GF_Matrix *mx)
{
    Fixed det;
	GF_Matrix rev;
	gf_mx_init(rev);

	assert(! ((mx->m[3] != 0) || (mx->m[7] != 0) || (mx->m[11] != 0) || (mx->m[15] != FIX_ONE)) );

	det = gf_mulfix(gf_mulfix(mx->m[0], mx->m[5]) , mx->m[10]);
	det += gf_mulfix(gf_mulfix(mx->m[1], mx->m[6]) , mx->m[8]);
	det += gf_mulfix(gf_mulfix(mx->m[2], mx->m[4]) , mx->m[9]);
	det -= gf_mulfix(gf_mulfix(mx->m[2], mx->m[5]) , mx->m[8]);
	det -= gf_mulfix(gf_mulfix(mx->m[1], mx->m[4]) , mx->m[10]);
	det -= gf_mulfix(gf_mulfix(mx->m[0], mx->m[6]) , mx->m[9]);

//	if (gf_mulfix(det) < FIX_EPSILON) return;

	/* Calculate inverse(A) = adj(A) / det(A) */
	rev.m[0] =  gf_muldiv(mx->m[5], mx->m[10], det) - gf_muldiv(mx->m[6], mx->m[9], det);
	rev.m[4] = -gf_muldiv(mx->m[4], mx->m[10], det) + gf_muldiv(mx->m[6], mx->m[8], det);
	rev.m[8] =  gf_muldiv(mx->m[4], mx->m[9], det) - gf_muldiv(mx->m[5], mx->m[8], det);
	rev.m[1] = -gf_muldiv(mx->m[1], mx->m[10], det) + gf_muldiv(mx->m[2], mx->m[9], det);
	rev.m[5] =  gf_muldiv(mx->m[0], mx->m[10], det) - gf_muldiv(mx->m[2], mx->m[8], det);
	rev.m[9] = -gf_muldiv(mx->m[0], mx->m[9], det) + gf_muldiv(mx->m[1], mx->m[8], det);
	rev.m[2] =  gf_muldiv(mx->m[1], mx->m[6], det) - gf_muldiv(mx->m[2], mx->m[5], det);
	rev.m[6] = -gf_muldiv(mx->m[0], mx->m[6], det) + gf_muldiv(mx->m[2], mx->m[4], det);
	rev.m[10] = gf_muldiv(mx->m[0], mx->m[5], det) - gf_muldiv(mx->m[1], mx->m[4], det);

	/* do translation part*/
	rev.m[12] = -( gf_mulfix(mx->m[12], rev.m[0]) + gf_mulfix(mx->m[13], rev.m[4]) + gf_mulfix(mx->m[14], rev.m[8]) );
	rev.m[13] = -( gf_mulfix(mx->m[12], rev.m[1]) + gf_mulfix(mx->m[13], rev.m[5]) + gf_mulfix(mx->m[14], rev.m[9]) );
	rev.m[14] = -( gf_mulfix(mx->m[12], rev.m[2]) + gf_mulfix(mx->m[13], rev.m[6]) + gf_mulfix(mx->m[14], rev.m[10]) );
	gf_mx_copy(*mx, rev);
}


void gf_mx_apply_vec(GF_Matrix *mx, GF_Vec *pt)
{
	GF_Vec res;
	res.x = gf_mulfix(pt->x, mx->m[0]) + gf_mulfix(pt->y, mx->m[4]) + gf_mulfix(pt->z, mx->m[8]) + mx->m[12];
	res.y = gf_mulfix(pt->x, mx->m[1]) + gf_mulfix(pt->y, mx->m[5]) + gf_mulfix(pt->z, mx->m[9]) + mx->m[13];
	res.z = gf_mulfix(pt->x, mx->m[2]) + gf_mulfix(pt->y, mx->m[6]) + gf_mulfix(pt->z, mx->m[10]) + mx->m[14];
	*pt = res;
}

void gf_mx_ortho(GF_Matrix *mx, Fixed left, Fixed right, Fixed bottom, Fixed top, Fixed z_near, Fixed z_far)
{
	gf_mx_init(*mx);
	mx->m[0] = gf_divfix(2*FIX_ONE, right-left);
	mx->m[5] = gf_divfix(2*FIX_ONE, top-bottom);
	mx->m[10] = gf_divfix(-2*FIX_ONE, z_far-z_near);
	mx->m[12] = gf_divfix(right+left, right-left);
	mx->m[13] = gf_divfix(top+bottom, top-bottom);
	mx->m[14] = gf_divfix(z_far+z_near, z_far-z_near);
	mx->m[15] = FIX_ONE;
}

void gf_mx_perspective(GF_Matrix *mx, Fixed fieldOfView, Fixed aspectRatio, Fixed z_near, Fixed z_far)
{
	Fixed f = gf_divfix(gf_cos(fieldOfView/2), gf_sin(fieldOfView/2));
	gf_mx_init(*mx);
	mx->m[0] = gf_divfix(f, aspectRatio);
	mx->m[5] = f;
	mx->m[10] = gf_divfix(z_far+z_near, z_near-z_far);
	mx->m[11] = -FIX_ONE;
	mx->m[14] = 2*gf_muldiv(z_near, z_far, z_near-z_far);
	mx->m[15] = 0;
}

void gf_mx_lookat(GF_Matrix *mx, GF_Vec eye, GF_Vec center, GF_Vec upVector)
{
	GF_Vec f, s, u;
	
	gf_vec_diff(f, center, eye);
	gf_vec_norm(&f);
	gf_vec_norm(&upVector);

	s = gf_vec_cross(f, upVector);
	u = gf_vec_cross(s, f);
	gf_mx_init(*mx);
	
	mx->m[0] = s.x;
	mx->m[1] = u.x;
	mx->m[2] = -f.x;
	mx->m[4] = s.y;
	mx->m[5] = u.y;
	mx->m[6] = -f.y;
	mx->m[8] = s.z;
	mx->m[9] = u.z;
	mx->m[10] = -f.z;

	gf_mx_add_translation(mx, -eye.x, -eye.y, -eye.z);
}

GF_Vec4 gf_quat_from_matrix(GF_Matrix *mx);

void gf_mx_decompose(GF_Matrix *mx, GF_Vec *translate, GF_Vec *scale, GF_Vec4 *rotate, GF_Vec *shear)
{
	u32 i, j;
	GF_Vec4 quat;
	Fixed locmat[16];
	GF_Matrix tmp;
	GF_Vec row0, row1, row2;
	Fixed shear_xy, shear_xz, shear_yz; 
	assert(mx->m[15]);

	memcpy(locmat, mx->m, sizeof(Fixed)*16);
	/*no perspective*/
    locmat[3] = locmat[7] = locmat[11] = 0;
	/*normalize*/
    for (i=0; i<4; i++) {
        for (j=0; j<4; j++) {
            locmat[4*i+j] = gf_divfix(locmat[4*i+j], locmat[15]);
        }
    }
    translate->x = locmat[12];
    translate->y = locmat[13];
    translate->z = locmat[14];
    locmat[12] = locmat[13] = locmat[14] = 0;
    row0.x = locmat[0]; row0.y = locmat[1]; row0.z = locmat[2];
    row1.x = locmat[4]; row1.y = locmat[5]; row1.z = locmat[6];
    row2.x = locmat[8]; row2.y = locmat[9]; row2.z = locmat[10];

    scale->x = gf_vec_len(row0);
    gf_vec_norm(&row0);
    shear_xy = gf_vec_dot(row0, row1);
    row1.x -= gf_mulfix(row0.x, shear_xy);
    row1.y -= gf_mulfix(row0.y, shear_xy);
    row1.z -= gf_mulfix(row0.z, shear_xy);

    scale->y = gf_vec_len(row1);
    gf_vec_norm(&row1);
    shear->x = gf_divfix(shear_xy, scale->y);

    shear_xz = gf_vec_dot(row0, row2);
    row2.x -= gf_mulfix(row0.x, shear_xz);
    row2.y -= gf_mulfix(row0.y, shear_xz);
    row2.z -= gf_mulfix(row0.z, shear_xz);
    shear_yz = gf_vec_dot(row1, row2);
    row2.x -= gf_mulfix(row1.x, shear_yz);
    row2.y -= gf_mulfix(row1.y, shear_yz);
    row2.z -= gf_mulfix(row1.z, shear_yz);

    scale->z = gf_vec_len(row2);
    gf_vec_norm(&row2);
    shear->y = gf_divfix(shear_xz, scale->z);
    shear->z = gf_divfix(shear_yz, scale->z);

	locmat[0] = row0.x; locmat[4] = row1.x; locmat[8] = row2.x;
	locmat[1] = row0.y; locmat[5] = row1.y; locmat[9] = row2.y;
	locmat[2] = row0.z; locmat[6] = row1.z; locmat[10] = row2.z;

	memcpy(tmp.m, locmat, sizeof(Fixed)*16);
	quat = gf_quat_from_matrix(&tmp);
	*rotate = gf_quat_to_rotation(&quat);
}

void gf_mx_apply_bbox(GF_Matrix *mx, GF_BBox *b)
{
	Fixed var;
	gf_mx_apply_vec(mx, &b->min_edge);
	gf_mx_apply_vec(mx, &b->max_edge);

	if (b->min_edge.x > b->max_edge.x) 
	{
		var = b->min_edge.x; b->min_edge.x = b->max_edge.x; b->max_edge.x = var;
	}
	if (b->min_edge.y > b->max_edge.y) 
	{
		var = b->min_edge.y; b->min_edge.y = b->max_edge.y; b->max_edge.y = var;
	}
	if (b->min_edge.z > b->max_edge.z) 
	{
		var = b->min_edge.z; b->min_edge.z = b->max_edge.z; b->max_edge.z = var;
	}
	gf_bbox_refresh(b);
}


// Apply the rotation portion of a matrix to a vector.
void gf_mx_rotate_vector(GF_Matrix *mx, GF_Vec *pt)
{
	GF_Vec res;
	Fixed den;
	res.x = gf_mulfix(pt->x, mx->m[0]) + gf_mulfix(pt->y, mx->m[4]) + gf_mulfix(pt->z, mx->m[8]);
	res.y = gf_mulfix(pt->x, mx->m[1]) + gf_mulfix(pt->y, mx->m[5]) + gf_mulfix(pt->z, mx->m[9]);
	res.z = gf_mulfix(pt->x, mx->m[2]) + gf_mulfix(pt->y, mx->m[6]) + gf_mulfix(pt->z, mx->m[10]);
	den = gf_mulfix(pt->x, mx->m[3]) + gf_mulfix(pt->y, mx->m[7]) + gf_mulfix(pt->z, mx->m[11]) + mx->m[15];
	if (den == 0) return;
	res.x = gf_divfix(res.x, den);
	res.y = gf_divfix(res.y, den);
	res.z = gf_divfix(res.z, den);
	*pt = res;
}

void gf_mx_rotation_matrix_from_vectors(GF_Matrix *mx, GF_Vec x, GF_Vec y, GF_Vec z)
{
    mx->m[0] = x.x; mx->m[1] = y.x; mx->m[2] = z.x; mx->m[3] = 0;
    mx->m[4] = x.y; mx->m[5] = y.y; mx->m[6] = z.y; mx->m[7] = 0;
    mx->m[8] = x.z; mx->m[9] = y.z; mx->m[10] = z.z; mx->m[11] = 0;
    mx->m[12] = 0; mx->m[13] = 0; mx->m[14] = 0; mx->m[15] = FIX_ONE;
}


/*we should only need a full matrix product for frustrum setup*/
void gf_mx_add_matrix_4x4(GF_Matrix *mat, GF_Matrix *mul)
{
    GF_Matrix tmp;
	gf_mx_init(tmp);
    tmp.m[0] = gf_mulfix(mat->m[0],mul->m[0]) + gf_mulfix(mat->m[4],mul->m[1]) + gf_mulfix(mat->m[8],mul->m[2]) + gf_mulfix(mat->m[12],mul->m[3]);
    tmp.m[1] = gf_mulfix(mat->m[1],mul->m[0]) + gf_mulfix(mat->m[5],mul->m[1]) + gf_mulfix(mat->m[9],mul->m[2]) + gf_mulfix(mat->m[13],mul->m[3]);
    tmp.m[2] = gf_mulfix(mat->m[2],mul->m[0]) + gf_mulfix(mat->m[6],mul->m[1]) + gf_mulfix(mat->m[10],mul->m[2]) + gf_mulfix(mat->m[14],mul->m[3]);
    tmp.m[3] = gf_mulfix(mat->m[3],mul->m[0]) + gf_mulfix(mat->m[7],mul->m[1]) + gf_mulfix(mat->m[11],mul->m[2]) + gf_mulfix(mat->m[15],mul->m[3]);
    tmp.m[4] = gf_mulfix(mat->m[0],mul->m[4]) + gf_mulfix(mat->m[4],mul->m[5]) + gf_mulfix(mat->m[8],mul->m[6]) + gf_mulfix(mat->m[12],mul->m[7]);
    tmp.m[5] = gf_mulfix(mat->m[1],mul->m[4]) + gf_mulfix(mat->m[5],mul->m[5]) + gf_mulfix(mat->m[9],mul->m[6]) + gf_mulfix(mat->m[13],mul->m[7]);
    tmp.m[6] = gf_mulfix(mat->m[2],mul->m[4]) + gf_mulfix(mat->m[6],mul->m[5]) + gf_mulfix(mat->m[10],mul->m[6]) + gf_mulfix(mat->m[14],mul->m[7]);
    tmp.m[7] = gf_mulfix(mat->m[3],mul->m[4]) + gf_mulfix(mat->m[7],mul->m[5]) + gf_mulfix(mat->m[11],mul->m[6]) + gf_mulfix(mat->m[15],mul->m[7]);
    tmp.m[8] = gf_mulfix(mat->m[0],mul->m[8]) + gf_mulfix(mat->m[4],mul->m[9]) + gf_mulfix(mat->m[8],mul->m[10]) + gf_mulfix(mat->m[12],mul->m[11]);
    tmp.m[9] = gf_mulfix(mat->m[1],mul->m[8]) + gf_mulfix(mat->m[5],mul->m[9]) + gf_mulfix(mat->m[9],mul->m[10]) + gf_mulfix(mat->m[13],mul->m[11]);
    tmp.m[10] = gf_mulfix(mat->m[2],mul->m[8]) + gf_mulfix(mat->m[6],mul->m[9]) + gf_mulfix(mat->m[10],mul->m[10]) + gf_mulfix(mat->m[14],mul->m[11]);
    tmp.m[11] = gf_mulfix(mat->m[3],mul->m[8]) + gf_mulfix(mat->m[7],mul->m[9]) + gf_mulfix(mat->m[11],mul->m[10]) + gf_mulfix(mat->m[15],mul->m[11]);
    tmp.m[12] = gf_mulfix(mat->m[0],mul->m[12]) + gf_mulfix(mat->m[4],mul->m[13]) + gf_mulfix(mat->m[8],mul->m[14]) + gf_mulfix(mat->m[12],mul->m[15]);
    tmp.m[13] = gf_mulfix(mat->m[1],mul->m[12]) + gf_mulfix(mat->m[5],mul->m[13]) + gf_mulfix(mat->m[9],mul->m[14]) + gf_mulfix(mat->m[13],mul->m[15]);
    tmp.m[14] = gf_mulfix(mat->m[2],mul->m[12]) + gf_mulfix(mat->m[6],mul->m[13]) + gf_mulfix(mat->m[10],mul->m[14]) + gf_mulfix(mat->m[14],mul->m[15]);
    tmp.m[15] = gf_mulfix(mat->m[3],mul->m[12]) + gf_mulfix(mat->m[7],mul->m[13]) + gf_mulfix(mat->m[11],mul->m[14]) + gf_mulfix(mat->m[15],mul->m[15]);
	memcpy(mat->m, tmp.m, sizeof(Fixed)*16);
}


void gf_mx_apply_vec_4x4(GF_Matrix *mx, GF_Vec4 *vec)
{
	GF_Vec4 res;
	res.x = gf_mulfix(mx->m[0], vec->x) + gf_mulfix(mx->m[4], vec->y) + gf_mulfix(mx->m[8], vec->z) + gf_mulfix(mx->m[12], vec->q);
	res.y = gf_mulfix(mx->m[1], vec->x) + gf_mulfix(mx->m[5], vec->y) + gf_mulfix(mx->m[9], vec->z) + gf_mulfix(mx->m[13], vec->q);
	res.z = gf_mulfix(mx->m[2], vec->x) + gf_mulfix(mx->m[6], vec->y) + gf_mulfix(mx->m[10], vec->z) + gf_mulfix(mx->m[14], vec->q);
	res.q = gf_mulfix(mx->m[3], vec->x) + gf_mulfix(mx->m[7], vec->y) + gf_mulfix(mx->m[11], vec->z) + gf_mulfix(mx->m[15], vec->q);
	*vec = res;
}

/*
 *	Taken from MESA/GLU (LGPL)
 *
 * Compute inverse of 4x4 transformation matrix.
 * Code contributed by Jacques Leroy jle@star.be
 * Return 1 for success, 0 for failure (singular matrix)
 */

Bool gf_mx_inverse_4x4(GF_Matrix *mx)
{

#define SWAP_ROWS(a, b) { Fixed *_tmp = a; (a)=(b); (b)=_tmp; }
	Fixed wtmp[4][8];
	Fixed m0, m1, m2, m3, s;
	Fixed *r0, *r1, *r2, *r3;
	GF_Matrix res;
	r0 = wtmp[0], r1 = wtmp[1], r2 = wtmp[2], r3 = wtmp[3];
	r0[0] = mx->m[0]; r0[1] = mx->m[4]; r0[2] = mx->m[8]; r0[3] = mx->m[12]; r0[4] = FIX_ONE; r0[5] = r0[6] = r0[7] = 0;
	r1[0] = mx->m[1]; r1[1] = mx->m[5]; r1[2] = mx->m[9]; r1[3] = mx->m[13]; r1[5] = FIX_ONE; r1[4] = r1[6] = r1[7] = 0;
	r2[0] = mx->m[2]; r2[1] = mx->m[6]; r2[2] = mx->m[10]; r2[3] = mx->m[14]; r2[6] = FIX_ONE; r2[4] = r2[5] = r2[7] = 0;
	r3[0] = mx->m[3]; r3[1] = mx->m[7]; r3[2] = mx->m[11]; r3[3] = mx->m[15]; r3[7] = FIX_ONE; r3[4] = r3[5] = r3[6] = 0;

	/* choose pivot - or die */
	if (ABS(r3[0]) > ABS(r2[0])) SWAP_ROWS(r3, r2);
	if (ABS(r2[0]) > ABS(r1[0])) SWAP_ROWS(r2, r1);
	if (ABS(r1[0]) > ABS(r0[0])) SWAP_ROWS(r1, r0);
	if (r0[0]==0) return 0;
	
	/*eliminate first variable*/
	m1 = gf_divfix(r1[0], r0[0]);
	m2 = gf_divfix(r2[0], r0[0]);
	m3 = gf_divfix(r3[0], r0[0]);
	s = r0[1];
	r1[1] -= gf_mulfix(m1, s);
	r2[1] -= gf_mulfix(m2, s);
	r3[1] -= gf_mulfix(m3, s);
	s = r0[2];
	r1[2] -= gf_mulfix(m1, s);
	r2[2] -= gf_mulfix(m2, s);
	r3[2] -= gf_mulfix(m3, s);
	s = r0[3];
	r1[3] -= gf_mulfix(m1, s);
	r2[3] -= gf_mulfix(m2, s);
	r3[3] -= gf_mulfix(m3, s);
	s = r0[4];
	if (s != 0) {
		r1[4] -= gf_mulfix(m1, s);
		r2[4] -= gf_mulfix(m2, s);
		r3[4] -= gf_mulfix(m3, s);
	}
	s = r0[5];
	if (s != 0) {
		r1[5] -= gf_mulfix(m1, s);
		r2[5] -= gf_mulfix(m2, s);
		r3[5] -= gf_mulfix(m3, s);
	}
	s = r0[6];
	if (s != 0) {
		r1[6] -= gf_mulfix(m1, s);
		r2[6] -= gf_mulfix(m2, s);
		r3[6] -= gf_mulfix(m3, s);
	}
	s = r0[7];
	if (s != 0) {
		r1[7] -= gf_mulfix(m1, s);
		r2[7] -= gf_mulfix(m2, s);
		r3[7] -= gf_mulfix(m3, s);
	}

	/* choose pivot - or die */
	if (fabs(r3[1]) > fabs(r2[1])) SWAP_ROWS(r3, r2);
	if (fabs(r2[1]) > fabs(r1[1])) SWAP_ROWS(r2, r1);
	if (r1[1]==0) return 0;
	
	/* eliminate second variable */
	m2 = gf_divfix(r2[1], r1[1]);
	m3 = gf_divfix(r3[1], r1[1]);
	r2[2] -= gf_mulfix(m2, r1[2]);
	r3[2] -= gf_mulfix(m3, r1[2]);
	r2[3] -= gf_mulfix(m2, r1[3]);
	r3[3] -= gf_mulfix(m3, r1[3]);
	s = r1[4];
	if (s!=0) {
		r2[4] -= gf_mulfix(m2, s);
		r3[4] -= gf_mulfix(m3, s);
	}
	s = r1[5];
	if (s!=0) {
		r2[5] -= gf_mulfix(m2, s);
		r3[5] -= gf_mulfix(m3, s);
	}
	s = r1[6];
	if (s!=0) {
		r2[6] -= gf_mulfix(m2, s);
		r3[6] -= gf_mulfix(m3, s);
	}
	s = r1[7];
	if (s!=0) {
		r2[7] -= gf_mulfix(m2, s);
		r3[7] -= gf_mulfix(m3, s);
	}

	/* choose pivot - or die */
	if (fabs(r3[2]) > fabs(r2[2])) SWAP_ROWS(r3, r2);
	if (r2[2]==0) return 0;

	/* eliminate third variable */
	m3 = gf_divfix(r3[2], r2[2]);
	r3[3] -= gf_mulfix(m3, r2[3]); r3[4] -= gf_mulfix(m3, r2[4]); r3[5] -= gf_mulfix(m3, r2[5]);
	r3[6] -= gf_mulfix(m3, r2[6]); r3[7] -= gf_mulfix(m3, r2[7]);
	/* last check */
	if (r3[3]==0) return 0;

	s = gf_divfix(FIX_ONE, r3[3]);		/* now back substitute row 3 */
	r3[4] = gf_mulfix(r3[4], s);
	r3[5] = gf_mulfix(r3[5], s);
	r3[6] = gf_mulfix(r3[6], s);
	r3[7] = gf_mulfix(r3[7], s);
	
	m2 = r2[3];			/* now back substitute row 2 */
	s = gf_divfix(FIX_ONE, r2[2]);
	r2[4] = gf_mulfix(s, r2[4] - gf_mulfix(r3[4], m2));
	r2[5] = gf_mulfix(s, r2[5] - gf_mulfix(r3[5], m2));
	r2[6] = gf_mulfix(s, r2[6] - gf_mulfix(r3[6], m2));
	r2[7] = gf_mulfix(s, r2[7] - gf_mulfix(r3[7], m2));
	m1 = r1[3]; 
	r1[4] -= gf_mulfix(r3[4], m1); r1[5] -= gf_mulfix(r3[5], m1); r1[6] -= gf_mulfix(r3[6], m1); r1[7] -= gf_mulfix(r3[7], m1);
	m0 = r0[3];
	r0[4] -= gf_mulfix(r3[4], m0); r0[5] -= gf_mulfix(r3[5], m0); r0[6] -= gf_mulfix(r3[6], m0); r0[7] -= gf_mulfix(r3[7], m0);

	m1 = r1[2];			/* now back substitute row 1 */
	s = gf_divfix(FIX_ONE, r1[1]);
	r1[4] = gf_mulfix(s, r1[4] - gf_mulfix(r2[4], m1));
	r1[5] = gf_mulfix(s, r1[5] - gf_mulfix(r2[5], m1));
	r1[6] = gf_mulfix(s, r1[6] - gf_mulfix(r2[6], m1));
	r1[7] = gf_mulfix(s, r1[7] - gf_mulfix(r2[7], m1));
	m0 = r0[2];
	r0[4] -= gf_mulfix(r2[4], m0); r0[5] -= gf_mulfix(r2[5], m0); r0[6] -= gf_mulfix(r2[6], m0); r0[7] -= gf_mulfix(r2[7], m0);

	m0 = r0[1];			/* now back substitute row 0 */
	s = gf_divfix(FIX_ONE, r0[0]);
	r0[4] = gf_mulfix(s, r0[4] - gf_mulfix(r1[4], m0));
	r0[5] = gf_mulfix(s, r0[5] - gf_mulfix(r1[5], m0)); 
	r0[6] = gf_mulfix(s, r0[6] - gf_mulfix(r1[6], m0));
	r0[7] = gf_mulfix(s, r0[7] - gf_mulfix(r1[7], m0));

	gf_mx_init(res)
	res.m[0] = r0[4]; res.m[4] = r0[5]; res.m[8] = r0[6]; res.m[12] = r0[7];
	res.m[1] = r1[4]; res.m[5] = r1[5], res.m[9] = r1[6]; res.m[13] = r1[7];
	res.m[2] = r2[4]; res.m[6] = r2[5]; res.m[10] = r2[6]; res.m[14] = r2[7];
	res.m[3] = r3[4]; res.m[7] = r3[5]; res.m[11] = r3[6]; res.m[15] = r3[7];
	gf_mx_copy(*mx, res);
	return 1;
#undef SWAP_ROWS

}


Bool gf_plane_exists_intersection(GF_Plane *plane, GF_Plane *with)
{
	GF_Vec cross;
	cross = gf_vec_cross(with->normal, plane->normal);
	return gf_vec_lensq(cross) > FIX_EPSILON;
}

Bool gf_plane_intersect_line(GF_Plane *plane, GF_Vec *linepoint, GF_Vec *linevec, GF_Vec *outPoint)
{
	Fixed t, t2;
	t2 = gf_vec_dot(plane->normal, *linevec);
	if (t2 == 0) return 0;
	t = - gf_divfix((gf_vec_dot(plane->normal, *linepoint) + plane->d) , t2);
	if (t<0) return 0;
	*outPoint = gf_vec_scale(*linevec, t);
	gf_vec_add(*outPoint, *linepoint, *outPoint);
	return 1;
}

Bool gf_plane_intersect_plane(GF_Plane *plane, GF_Plane *with, GF_Vec *linepoint, GF_Vec *linevec)
{
	Fixed fn00 = gf_vec_len(plane->normal);
	Fixed fn01 = gf_vec_dot(plane->normal, with->normal);
	Fixed fn11 = gf_vec_len(with->normal);
	Fixed det = gf_mulfix(fn00,fn11) - gf_mulfix(fn01,fn01);
	if (fabs(det) > FIX_EPSILON) {
		Fixed fc0, fc1;
		GF_Vec v1, v2;
		fc0 = gf_divfix( gf_mulfix(fn11, -plane->d) + gf_mulfix(fn01, with->d) , det);
		fc1 = gf_divfix( gf_mulfix(fn00, -with->d) + gf_mulfix(fn01, plane->d) , det);
		*linevec = gf_vec_cross(plane->normal, with->normal);
		v1 = gf_vec_scale(plane->normal, fc0);
		v2 = gf_vec_scale(with->normal, fc1);
		gf_vec_add(*linepoint, v1, v2);
		return 1;
	}
	return 0;
}

Bool gf_plane_intersect_planes(GF_Plane *plane, GF_Plane *p1, GF_Plane *p2, GF_Vec *outPoint)
{
	GF_Vec lp, lv;
	if (gf_plane_intersect_plane(plane, p1, &lp, &lv))
		return gf_plane_intersect_line(p2, &lp, &lv, outPoint);
	return 0;
}



GF_Ray gf_ray(GF_Vec start, GF_Vec end)
{
	GF_Ray r;
	r.orig = start;
	gf_vec_diff(r.dir, end, start);
	gf_vec_norm(&r.dir);
	return r;
}

void gf_mx_apply_ray(GF_Matrix *mx, GF_Ray *r)
{
	gf_vec_add(r->dir, r->orig, r->dir);
	gf_mx_apply_vec(mx, &r->orig);
	gf_mx_apply_vec(mx, &r->dir);
	gf_vec_diff(r->dir, r->dir, r->orig);
	gf_vec_norm(&r->dir);
}

#define XPLANE 0
#define YPLANE 1
#define ZPLANE 2


Bool gf_ray_hit_box(GF_Ray *ray, GF_Vec box_min, GF_Vec box_max, GF_Vec *outPoint)
{
	Fixed t1, t2, tNEAR=FIX_MIN, tFAR=FIX_MAX;
	Fixed temp;
	s8 xyorz, sign;
	
	if (ray->dir.x == 0) {
		if ((ray->orig.x < box_min.x) || (ray->orig.x > box_max.x))
			return 0;
	} else {
		t1 = gf_divfix(box_min.x - ray->orig.x, ray->dir.x);
		t2 = gf_divfix(box_max.x - ray->orig.x, ray->dir.x);
		if (t1 > t2) {
			temp = t1;
			t1 = t2;
			t2 = temp;
		}
		if (t1 > tNEAR) {
			tNEAR = t1;
			xyorz = XPLANE;
			sign = (ray->dir.x < 0) ? 1 : -1;
		}
		if (t2 < tFAR) tFAR = t2;
		if (tNEAR > tFAR) return 0; // box missed
		if (tFAR < 0) return 0; // box behind the ray
	}

	if (ray->dir.y == 0) {
		if ((ray->orig.y < box_min.y) || (ray->orig.y > box_max.y)) 
			return 0;
	} else {
		t1 = gf_divfix(box_min.y - ray->orig.y, ray->dir.y);
		t2 = gf_divfix(box_max.y - ray->orig.y, ray->dir.y);
		if (t1 > t2) {
			temp = t1;
			t1 = t2;
			t2 = temp;
		}
		if (t1 > tNEAR) {
			tNEAR = t1;
			xyorz = YPLANE;
			sign = (ray->dir.y < 0) ? 1 : -1;
		}
		if (t2 < tFAR) tFAR = t2;
		if (tNEAR > tFAR) return 0; // box missed
		if (tFAR < 0) return 0; // box behind the ray
	}

	// Check the Z plane
	if (ray->dir.z == 0) {
		if ((ray->orig.z < box_min.z) || (ray->orig.z > box_max.z))
			return 0;
	} else {
		t1 = gf_divfix(box_min.z - ray->orig.z, ray->dir.z);
		t2 = gf_divfix(box_max.z - ray->orig.z, ray->dir.z);
		if (t1 > t2) {
			temp = t1;
			t1 = t2;
			t2 = temp;
		}
		if (t1 > tNEAR) {
			tNEAR = t1;
			xyorz = ZPLANE;
			sign = (ray->dir.z < 0) ? 1 : -1;
		}
		if (t2 < tFAR) tFAR = t2;
		if (tNEAR>tFAR) return 0; // box missed
		if (tFAR < 0) return 0;  // box behind the ray
	}
	if (outPoint) {
		*outPoint = gf_vec_scale(ray->dir, tNEAR);
		gf_vec_add(*outPoint, *outPoint, ray->orig);
	}
	return 1;
}


Bool gf_ray_hit_sphere(GF_Ray *ray, GF_Vec *center, Fixed radius, GF_Vec *outPoint)
{
	GF_Vec radv;
	Fixed dist, center_proj, center_proj_sq, hcord;
	if (center) {
		gf_vec_diff(radv, *center, ray->orig);
	} else {
		radv = ray->orig;
		gf_vec_rev(radv);
	}
	dist = gf_vec_len(radv);
	center_proj = gf_vec_dot(radv, ray->dir);
	if (radius + ABS(center_proj) < dist ) return 0;
	
	center_proj_sq = gf_mulfix(center_proj, center_proj);
	hcord = center_proj_sq - gf_mulfix(dist, dist) + gf_mulfix(radius , radius);
    if (hcord < 0) return 0;
	if (center_proj_sq < hcord) return 0;
	if (outPoint) {
		center_proj -= gf_sqrt(hcord);
		radv = gf_vec_scale(ray->dir, center_proj);
		gf_vec_add(*outPoint, ray->orig, radv);
	}
	return 1;
}

/*
 *		Tomas Möller and Ben Trumbore.
 *	 Fast, minimum storage ray-triangle intersection. 
 *		Journal of graphics tools, 2(1):21-28, 1997
 *
 */
Bool gf_ray_hit_triangle(GF_Ray *ray, GF_Vec *v0, GF_Vec *v1, GF_Vec *v2, Fixed *dist)
{
	Fixed u, v, det;
	GF_Vec edge1, edge2, tvec, pvec, qvec;
	/* find vectors for two edges sharing vert0 */
	gf_vec_diff(edge1, *v1, *v0);
	gf_vec_diff(edge2, *v2, *v0);
	/* begin calculating determinant - also used to calculate U parameter */
	pvec = gf_vec_cross(ray->dir, edge2);
	/* if determinant is near zero, ray lies in plane of triangle */
	det = gf_vec_dot(edge1, pvec);
	if (ABS(det) < FIX_EPSILON) return 0;
	/* calculate distance from vert0 to ray origin */
	gf_vec_diff(tvec, ray->orig, *v0);
	/* calculate U parameter and test bounds */
	u = gf_divfix(gf_vec_dot(tvec, pvec), det);
	if ((u < 0) || (u > FIX_ONE)) return 0;
	/* prepare to test V parameter */
	qvec = gf_vec_cross(tvec, edge1);
	/* calculate V parameter and test bounds */
	v = gf_divfix(gf_vec_dot(ray->dir, qvec), det);
	if ((v < 0) || (u + v > FIX_ONE)) return 0;
	/* calculate t, ray intersects triangle */
	*dist = gf_divfix(gf_vec_dot(edge2, qvec), det);
	return 1;
}

Bool gf_ray_hit_triangle_backcull(GF_Ray *ray, GF_Vec *v0, GF_Vec *v1, GF_Vec *v2, Fixed *dist)
{
	Fixed u, v, det;
	GF_Vec edge1, edge2, tvec, pvec, qvec;
	/* find vectors for two edges sharing vert0 */
	gf_vec_diff(edge1, *v1, *v0);
	gf_vec_diff(edge2, *v2, *v0);
	/* begin calculating determinant - also used to calculate U parameter */
	pvec = gf_vec_cross(ray->dir, edge2);
	/* if determinant is near zero, ray lies in plane of triangle */
	det = gf_vec_dot(edge1, pvec);
	if (det < FIX_EPSILON) return 0;
	/* calculate distance from vert0 to ray origin */
	gf_vec_diff(tvec, ray->orig, *v0);
	/* calculate U parameter and test bounds */
	u = gf_vec_dot(tvec, pvec);
	if ((u < 0) || (u > det)) return 0;
	/* prepare to test V parameter */
	qvec = gf_vec_cross(tvec, edge1);
	/* calculate V parameter and test bounds */
	v = gf_vec_dot(ray->dir, qvec);
	if ((v < 0) || (u + v > det)) return 0;
	/* calculate t, scale parameters, ray intersects triangle */
	*dist = gf_divfix(gf_vec_dot(edge2, qvec), det);
	return 1;
}

GF_Vec gf_closest_point_to_line(GF_Vec line_pt, GF_Vec line_vec, GF_Vec pt)
{
	GF_Vec c;
	Fixed t;
	gf_vec_diff(c, pt, line_pt);
	t = gf_vec_dot(line_vec, c);
	c = gf_vec_scale(line_vec, t);
	gf_vec_add(c, c, line_pt);
	return c;
}


GF_Vec4 gf_quat_from_matrix(GF_Matrix *mx)
{
	GF_Vec4 res;
	Fixed diagonal, s;
    diagonal = mx->m[0] + mx->m[5] + mx->m[10];

    if (diagonal > 0) {
        s = gf_sqrt(diagonal + FIX_ONE);
        res.q = s / 2;
        s = gf_divfix(FIX_ONE/2, s);
        res.x = gf_mulfix(mx->m[6] - mx->m[9], s);
        res.y = gf_mulfix(mx->m[8] - mx->m[2], s);
        res.z = gf_mulfix(mx->m[1] - mx->m[4], s);
    } else {
		Fixed q[4];
        u32 i, j, k;
        static const u32 next[3] = { 1, 2, 0 };
        i = 0;
        if (mx->m[5] > mx->m[0]) { i = 1; }
        if (mx->m[10] > mx->m[4*i+i]) { i = 2; }
        j = next[i];
        k = next[j];
        s = gf_sqrt(FIX_ONE + mx->m[4*i + i] - (mx->m[4*j+j] + mx->m[4*k+k]) );
        q[i] = s / 2;
        if (s != 0) { s = gf_divfix(FIX_ONE/2, s); }
        q[3] = gf_mulfix(mx->m[4*j+k] - mx->m[4*k+j], s);
        q[j] = gf_mulfix(mx->m[4*i+j] + mx->m[4*j+i], s);
        q[k] = gf_mulfix(mx->m[4*i+k] + mx->m[4*k+i], s);
		res.x = q[0]; res.y = q[1]; res.z = q[2]; res.q = q[3];
    }
	return res;
}

GF_Vec4 gf_quat_to_rotation(GF_Vec4 *quat)
{
	GF_Vec4 r;
    Fixed val = gf_acos(quat->q);
    if (val == 0) {
        r.x = r.y = 0;
        r.z = FIX_ONE;
		r.q = 0;
    } else {
		GF_Vec axis;
        Fixed sin_val = gf_sin(val);
        axis.x = gf_divfix(quat->x, sin_val);
        axis.y = gf_divfix(quat->y, sin_val);
        axis.z = gf_divfix(quat->z, sin_val);
		gf_vec_norm(&axis);
		r.x = axis.x;
		r.y = axis.y;
		r.z = axis.z;
        r.q= 2 * val;
    }
	return r;
}

GF_Vec4 gf_quat_from_rotation(GF_Vec4 rot)
{
	GF_Vec4 res;
	Fixed s;
	Fixed scale = gf_sqrt( gf_mulfix(rot.x, rot.x) + gf_mulfix(rot.y, rot.y) + gf_mulfix(rot.z, rot.z));

	/* no rotation - use (multiplication ???) identity quaternion */
	if (scale == 0) {
		res.q = FIX_ONE;
		res.x = 0;
		res.y = 0;
		res.z = 0;
	} else {
		s = gf_sin(rot.q/2);
		res.q = gf_cos(rot.q / 2);
		res.x = gf_muldiv(s, rot.x, scale);
		res.y = gf_muldiv(s, rot.y, scale);
		res.z = gf_muldiv(s, rot.z, scale);
		gf_quat_norm(res);
	}
	return res;
}

GF_Vec4 gf_quat_from_axis_cos(GF_Vec axis, Fixed cos_a)
{
	GF_Vec4 r;
	if (cos_a < -FIX_ONE) cos_a = -FIX_ONE;
	else if (cos_a > FIX_ONE) cos_a = FIX_ONE;
	r.x = axis.x; r.y = axis.y; r.z = axis.z;
	r.q = gf_acos(cos_a);
	return gf_quat_from_rotation(r);
}

void gf_quat_conjugate(GF_Vec4 *quat)
{
	quat->x *= -1;
	quat->y *= -1;
	quat->z *= -1;
}

GF_Vec4 gf_quat_get_inv(GF_Vec4 *quat)
{
	GF_Vec4 ret = *quat;
	gf_quat_conjugate(&ret);
	gf_quat_norm(ret);
	return ret;
}


GF_Vec4 gf_quat_multiply(GF_Vec4 *q1, GF_Vec4 *q2)
{
	GF_Vec4 ret;
	ret.q = gf_mulfix(q1->q, q2->q) - gf_mulfix(q1->x, q2->x) - gf_mulfix(q1->y, q2->y) - gf_mulfix(q1->z, q2->z);
	ret.x = gf_mulfix(q1->q, q2->x) + gf_mulfix(q1->x, q2->q) + gf_mulfix(q1->y, q2->z) - gf_mulfix(q1->z, q2->y);
	ret.y = gf_mulfix(q1->q, q2->y) + gf_mulfix(q1->y, q2->q) - gf_mulfix(q1->x, q2->z) + gf_mulfix(q1->z, q2->x);
	ret.z = gf_mulfix(q1->q, q2->z) + gf_mulfix(q1->z, q2->q) + gf_mulfix(q1->x, q2->y) - gf_mulfix(q1->y, q2->x);
	return ret;
}

GF_Vec gf_quat_rotate(GF_Vec4 *quat, GF_Vec *vec)
{
	GF_Vec ret;
	GF_Vec4 q_v, q_i, q_r1, q_r2;
	q_v.q = 0;
	q_v.x = vec->x;
	q_v.y = vec->y;
	q_v.z = vec->z;
	q_i = gf_quat_get_inv(quat);
	q_r1 = gf_quat_multiply(&q_v, &q_i);
	q_r2 = gf_quat_multiply(quat, &q_r1);
	ret.x = q_r2.x;
	ret.y = q_r2.y;
	ret.z = q_r2.z;
	return ret;
}

/*
 * Code from www.gamasutra.com/features/19980703/quaternions_01.htm,
 * Listing 5.
 *
 * SLERP(p, q, t) = [p sin((1 - t)a) + q sin(ta)] / sin(a)
 *
 * where a is the arc angle, quaternions pq = cos(q) and 0 <= t <= 1
 */
GF_Vec4 gf_quat_slerp(GF_Vec4 q1, GF_Vec4 q2, Fixed frac)
{
	GF_Vec4 res;
	Fixed omega, cosom, sinom, scale0, scale1, q2_array[4];

	cosom = gf_mulfix(q1.x, q2.x) + gf_mulfix(q1.y, q2.y) + gf_mulfix(q1.z, q2.z) + gf_mulfix(q1.q, q2.q);
	if (cosom < 0) {
		cosom = -cosom;
		q2_array[0] = -q2.x;
		q2_array[1] = -q2.y;
		q2_array[2] = -q2.z;
		q2_array[3] = -q2.q;
	} else {
		q2_array[0] = q2.x;
		q2_array[1] = q2.y;
		q2_array[2] = q2.z;
		q2_array[3] = q2.q;
	}

	/* calculate coefficients */
	if ((FIX_ONE - cosom) > FIX_EPSILON) {
		omega = gf_acos(cosom);
		sinom = gf_sin(omega);
		scale0 = gf_divfix(gf_sin( gf_mulfix(FIX_ONE - frac, omega)), sinom);
		scale1 = gf_divfix(gf_sin( gf_mulfix(frac, omega)), sinom);
	} else {
		/* q1 & q2 are very close, so do linear interpolation */
		scale0 = FIX_ONE - frac;
		scale1 = frac;
	}
	res.x = gf_mulfix(scale0, q1.x) + gf_mulfix(scale1, q2_array[0]);
	res.y = gf_mulfix(scale0, q1.y) + gf_mulfix(scale1, q2_array[1]);
	res.z = gf_mulfix(scale0, q1.z) + gf_mulfix(scale1, q2_array[2]);
	res.q = gf_mulfix(scale0, q1.q) + gf_mulfix(scale1, q2_array[3]);
	return res;
}


/*update center & radius & is_set flag*/
void gf_bbox_refresh(GF_BBox *b)
{
	GF_Vec v;
	gf_vec_add(v, b->min_edge, b->max_edge);
	b->center = gf_vec_scale(v, FIX_ONE / 2);
	gf_vec_diff(v, b->max_edge, b->min_edge);
	b->radius = gf_vec_len(v) / 2;
	b->is_set = 1;
}

void gf_bbox_from_rect(GF_BBox *box, GF_Rect *rc)
{
	box->min_edge.x = rc->x;
	box->min_edge.y = rc->y - rc->height;
	box->min_edge.z = 0;
	box->max_edge.x = rc->x + rc->width;
	box->max_edge.y = rc->y;
	box->max_edge.z = 0;
	gf_bbox_refresh(box);
}

void gf_rect_from_bbox(GF_Rect *rc, GF_BBox *box)
{
	rc->x = box->min_edge.x;
	rc->y = box->max_edge.y;
	rc->width = box->max_edge.x - box->min_edge.x;
	rc->height = box->max_edge.y - box->min_edge.y;
}

void gf_bbox_grow_point(GF_BBox *box, GF_Vec pt)
{
	if (pt.x > box->max_edge.x) box->max_edge.x = pt.x;
	if (pt.y > box->max_edge.y) box->max_edge.y = pt.y;
	if (pt.z > box->max_edge.z) box->max_edge.z = pt.z;
	if (pt.x < box->min_edge.x) box->min_edge.x = pt.x;
	if (pt.y < box->min_edge.y) box->min_edge.y = pt.y;
	if (pt.z < box->min_edge.z) box->min_edge.z = pt.z;
}

void gf_bbox_union(GF_BBox *b1, GF_BBox *b2)
{
	if (b2->is_set) {
		if (!b1->is_set) {
			*b1 = *b2;
		} else {
			gf_bbox_grow_point(b1, b2->min_edge);
			gf_bbox_grow_point(b1, b2->max_edge);
			gf_bbox_refresh(b1);
		}
	}
}

Bool gf_bbox_equal(GF_BBox *b1, GF_BBox *b2)
{
	return (gf_vec_equal(b1->min_edge, b2->min_edge) && gf_vec_equal(b1->max_edge, b2->max_edge));
}

Bool gf_bbox_point_inside(GF_BBox *box, GF_Vec *p)
{
	return (p->x >= box->min_edge.x && p->x <= box->max_edge.x &&
			p->y >= box->min_edge.y && p->y <= box->max_edge.y &&
			p->z >= box->min_edge.z && p->z <= box->max_edge.z);
}

/*vertices are ordered to respect p vertex indexes (vertex from bbox closer to plane)
and so that n-vertex (vertex from bbox farther from plane) is 7-p_vx_idx*/
void gf_bbox_get_vertices(GF_Vec bmin, GF_Vec bmax, GF_Vec *vecs)
{
	vecs[0].x = vecs[1].x = vecs[2].x = vecs[3].x = bmax.x;
	vecs[4].x = vecs[5].x = vecs[6].x = vecs[7].x = bmin.x;
	vecs[0].y = vecs[1].y = vecs[4].y = vecs[5].y = bmax.y;
	vecs[2].y = vecs[3].y = vecs[6].y = vecs[7].y = bmin.y;
	vecs[0].z = vecs[2].z = vecs[4].z = vecs[6].z = bmax.z;
	vecs[1].z = vecs[3].z = vecs[5].z = vecs[7].z = bmin.z;
}


void gf_mx_apply_plane(GF_Matrix *mx, GF_Plane *plane)
{
	GF_Vec pt, end;
	/*get pt*/
	pt = gf_vec_scale(plane->normal, -plane->d);
	gf_vec_add(end, pt, plane->normal);
	gf_mx_apply_vec(mx, &pt);
	gf_mx_apply_vec(mx, &end);
	gf_vec_diff(plane->normal, end, pt);
	gf_vec_norm(&plane->normal);
	plane->d = - gf_vec_dot(pt, plane->normal);
}

Fixed gf_plane_get_distance(GF_Plane *plane, GF_Vec *p)
{
	return gf_vec_dot(*p, plane->normal) + plane->d;
}


/*return p-vertex index (vertex from bbox closer to plane) - index range from 0 to 8*/
u32 gf_plane_get_p_vertex_idx(GF_Plane *p)
{
	if (p->normal.x>=0) {
		if (p->normal.y>=0) return (p->normal.z>=0) ? 0 : 1;
		return (p->normal.z>=0) ? 2 : 3;
	} else {
		if (p->normal.y>=0) return (p->normal.z>=0) ? 4 : 5;
		return (p->normal.z>=0) ? 6 : 7;
	}
}


u32 gf_bbox_plane_relation(GF_BBox *box, GF_Plane *p)
{
	GF_Vec nearv, farv;
	nearv = box->max_edge;
	farv = box->min_edge;
	if (p->normal.x > 0) {
		nearv.x = box->min_edge.x;
		farv.x = box->max_edge.x;
	}
	if (p->normal.y > 0) {
		nearv.y = box->min_edge.y;
		farv.y = box->max_edge.y;
	}
	if (p->normal.z > 0) {
		nearv.z = box->min_edge.z;
		farv.z = box->max_edge.z;
	}
	if (gf_vec_dot(p->normal, nearv) + p->d > 0) return GF_BBOX_FRONT;
	if (gf_vec_dot(p->normal, farv) + p->d > 0) return GF_BBOX_INTER;
	return GF_BBOX_BACK;
}

