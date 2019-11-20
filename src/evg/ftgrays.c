/***************************************************************************/
/*                                                                         */
/*  ftgrays.c                                                              */
/*                                                                         */
/*    A new `perfect' anti-aliasing renderer (body).                       */
/*                                                                         */
/*  Copyright 2000-2001, 2002 by                                           */
/*  David Turner, Robert Wilhelm, and Werner Lemberg.                      */
/*                                                                         */
/*  This file is part of the FreeType project, and may only be used,       */
/*  modified, and distributed under the terms of the FreeType project      */
/*  license, LICENSE.TXT.  By continuing to use, modify, or distribute     */
/*  this file you indicate that you have read the license and              */
/*  understand and accept it fully.                                        */
/*                                                                         */
/***************************************************************************/

/*************************************************************************/
/*                                                                       */
/* This is a new anti-aliasing scan-converter for FreeType 2.  The       */
/* algorithm used here is _very_ different from the one in the standard  */
/* `ftraster' module.  Actually, `ftgrays' computes the _exact_          */
/* coverage of the outline on each pixel cell.                           */
/*                                                                       */
/* It is based on ideas that I initially found in Raph Levien's          */
/* excellent LibArt graphics library (see http://www.levien.com/libart   */
/* for more information, though the web pages do not tell anything       */
/* about the renderer; you'll have to dive into the source code to       */
/* understand how it works).                                             */
/*                                                                       */
/* Note, however, that this is a _very_ different implementation         */
/* compared to Raph's.  Coverage information is stored in a very         */
/* different way, and I don't use sorted vector paths.  Also, it doesn't */
/* use floating point values.                                            */
/*                                                                       */
/* This renderer has the following advantages:                           */
/*                                                                       */
/* - It doesn't need an intermediate bitmap.  Instead, one can supply a  */
/*   callback function that will be called by the renderer to draw gray  */
/*   spans on any target surface.  You can thus do direct composition on */
/*   any kind of bitmap, provided that you give the renderer the right   */
/*   callback.                                                           */
/*                                                                       */
/* - A perfect anti-aliaser, i.e., it computes the _exact_ coverage on   */
/*   each pixel cell.                                                    */
/*                                                                       */
/* - It performs a single pass on the outline (the `standard' FT2        */
/*   renderer makes two passes).                                         */
/*                                                                       */
/* - It can easily be modified to render to _any_ number of gray levels  */
/*   cheaply.                                                            */
/*                                                                       */
/* - For small (< 20) pixel sizes, it is faster than the standard        */
/*   renderer.                                                           */
/*                                                                       */
/*************************************************************************/


/*
	GPAC version modifications:
		* removed all cubic/quadratic support (present in GPAC path objects)
		* moved span data memory to dynamic allocation
		* bypassed Y-sorting of cells by using an array of scanlines: a bit more consuming
		in memory, but faster cell sorting (X-sorting only)
		* deferred coverage push for 3D
*/

#include "rast_soft.h"


void gray_record_cell( TRaster *raster )
{
	if (( raster->area | raster->cover) && (raster->ey<raster->max_ey)) {
		long y = raster->ey - raster->min_ey;
		if (y>=0) {
			AACell *cell;
			AAScanline *sl = &raster->scanlines[y];

			if (sl->num >= sl->alloc) {
				sl->cells = (AACell*)gf_realloc(sl->cells, sizeof(AACell)* (sl->alloc + AA_CELL_STEP_ALLOC));
				sl->alloc += AA_CELL_STEP_ALLOC;
			}
			cell = &sl->cells[sl->num];
			sl->num++;
			/*clip cell */
			if (raster->ex<raster->min_ex) cell->x = (TCoord) -1;
			else if (raster->ex>raster->max_ex) cell->x = (TCoord) (raster->max_ex - raster->min_ex);
			else cell->x = (TCoord)(raster->ex - raster->min_ex);
			cell->area = raster->area;
			cell->cover = raster->cover;
			cell->idx1 = raster->idx1;
			cell->idx2 = raster->idx2;

			if (raster->first_scanline > (u32) y)
				raster->first_scanline = y;
		}
	}
}

void gray_set_cell( TRaster *raster, TCoord  ex, TCoord  ey )
{
	if ((raster->ex != ex) || (raster->ey != ey)) {
		gray_record_cell(raster);
		raster->ex = ex;
		raster->ey = ey;
		raster->area = 0;
		raster->cover = 0;
	}
}


static GFINLINE void evg_translate_point(GF_Matrix2D *mx, EVG_Vector *pt, TPos *x, TPos *y)
{
	Fixed _x, _y;
	_x = pt->x;
	_y = pt->y;
	gf_mx2d_apply_coords(mx, &_x, &_y);
#ifdef GPAC_FIXED_POINT
	*x = UPSCALE(_x);
	*y = UPSCALE(_y);
#else
	*x = (s32) (_x * ONE_PIXEL);
	*y = (s32) (_y * ONE_PIXEL);
#endif
}

static GFINLINE int gray_move_to( EVG_Vector*  to, EVG_Raster   raster)
{
	TPos  x, y;
	TCoord  ex, ey;

	/* record current cell, if any */
	gray_record_cell(raster);

	evg_translate_point(raster->mx, to, &x, &y);

	ex = TRUNC(x);
	ey = TRUNC(y);
	if ( ex < raster->min_ex ) ex = (TCoord)(raster->min_ex - 1);
	raster->area    = 0;
	raster->cover   = 0;
	gray_set_cell( raster, ex, ey );
	if (ey<0) ey=0;
	raster->last_ey = SUBPIXELS( ey );

	raster->x = x;
	raster->y = y;
	return 0;
}

/*************************************************************************/
/*                                                                       */
/* Render a scanline as one or more cells.                               */
/*                                                                       */
static void gray_render_scanline( TRaster *raster,  TCoord  ey, TPos x1, TCoord y1, TPos x2, TCoord y2)
{
	TCoord  ex1, ex2, fx1, fx2, delta;
	long    p, first;
	long dx;
	int     incr, mod;

	dx = x2 - x1;
	ex1 = TRUNC( x1 ); /* if (ex1 >= raster->max_ex) ex1 = raster->max_ex-1; */
	ex2 = TRUNC( x2 ); /* if (ex2 >= raster->max_ex) ex2 = raster->max_ex-1; */
	if (ex1<0) ex1=0;
	if (ex2<0) ex2=0;
	fx1 = (TCoord)( x1 - SUBPIXELS( ex1 ) );
	fx2 = (TCoord)( x2 - SUBPIXELS( ex2 ) );

	/* trivial case.  Happens often */
	if ( y1 == y2 ) {
		gray_set_cell( raster, ex2, ey );
		return;
	}

	/* everything is located in a single cell.  That is easy! */
	if ( ex1 == ex2 ) {
		delta      = y2 - y1;
		raster->area  += (TArea)( fx1 + fx2 ) * delta;
		raster->cover += delta;
		return;
	}

	/* ok, we'll have to render a run of adjacent cells on the same */
	/* scanline...                                                  */
	p     = ( ONE_PIXEL - fx1 ) * ( y2 - y1 );
	first = ONE_PIXEL;
	incr  = 1;

	if ( dx < 0 ) {
		p     = fx1 * ( y2 - y1 );
		first = 0;
		incr  = -1;
		dx    = -dx;
	}
	delta = (TCoord)( p / dx );
	mod   = (TCoord)( p % dx );
	if ( mod < 0 ) {
		delta--;
		mod += (TCoord)dx;
	}
	raster->area  += (TArea)( fx1 + first ) * delta;
	raster->cover += delta;

	ex1 += incr;
	gray_set_cell( raster, ex1, ey );
	y1  += delta;

	if ( ex1 != ex2 ) {
		int lift, rem;
		p     = ONE_PIXEL * ( y2 - y1 + delta );
		lift  = (TCoord)( p / dx );
		rem   = (TCoord)( p % dx );
		if ( rem < 0 ) {
			lift--;
			rem += (TCoord)dx;
		}
		mod -= (int) dx;

		while ( ex1 != ex2 ) {
			delta = lift;
			mod  += rem;
			if ( mod >= 0 ) {
				mod -= (TCoord)dx;
				delta++;
			}

			raster->area  += (TArea)ONE_PIXEL * delta;
			raster->cover += delta;
			y1        += delta;
			ex1       += incr;
			gray_set_cell( raster, ex1, ey );
		}
	}
	delta      = y2 - y1;
	raster->area  += (TArea)( fx2 + ONE_PIXEL - first ) * delta;
	raster->cover += delta;
}


/*************************************************************************/
/*                                                                       */
/* Render a given line as a series of scanlines.                         */
void gray_render_line(TRaster *raster, TPos  to_x, TPos  to_y)
{
	TCoord  min, max;
	TCoord  ey1, ey2, fy1, fy2;
	TPos    x, x2;
	long dx, dy;
	long    p, first;
	int     delta, rem, mod, lift, incr;


	ey1 = TRUNC( raster->last_ey );
	ey2 = TRUNC( to_y ); /* if (ey2 >= raster->max_ey) ey2 = raster->max_ey-1; */
	if (ey2<0) ey2=0;
	fy1 = (TCoord)( raster->y - raster->last_ey );
	fy2 = (TCoord)( to_y - SUBPIXELS( ey2 ) );

	dx = to_x - raster->x;
	dy = to_y - raster->y;

	/* perform vertical clipping */
	min = ey1;
	max = ey2;
	if ( ey1 > ey2 ) {
		min = ey2;
		max = ey1;
	}
	if ( min >= raster->max_ey || max < raster->min_ey ) goto End;

	/* everything is on a single scanline */
	if ( ey1 == ey2 ) {
		gray_render_scanline( raster, ey1, raster->x, fy1, to_x, fy2 );
		goto End;
	}
	/* vertical line - avoid calling gray_render_scanline */
	incr = 1;
	if (dx == 0 ) {
		TCoord  ex     = TRUNC( raster->x );
		TCoord  two_fx = (TCoord)( ( raster->x - SUBPIXELS( ex ) ) << 1 );
		TPos    area;

		first = ONE_PIXEL;
		if ( dy < 0 ) {
			first = 0;
			incr  = -1;
		}

		delta      = (int)( first - fy1 );
		raster->area  += (TArea)two_fx * delta;
		raster->cover += delta;
		ey1       += incr;

		gray_set_cell( raster, ex, ey1 );

		delta = (int)( first + first - ONE_PIXEL );
		area  = (TArea)two_fx * delta;
		while ( ey1 != ey2 ) {
			raster->area  += area;
			raster->cover += delta;
			ey1       += incr;
			gray_set_cell( raster, ex, ey1 );
		}
		delta      = (int)( fy2 - ONE_PIXEL + first );
		raster->area  += (TArea)two_fx * delta;
		raster->cover += delta;
		goto End;
	}
	/* ok, we have to render several scanlines */
	p     = ( ONE_PIXEL - fy1 ) * dx;
	first = ONE_PIXEL;
	incr  = 1;

	if ( dy < 0 ) {
		p     = fy1 * dx;
		first = 0;
		incr  = -1;
		dy    = -dy;
	}

	delta = (int)( p / dy );
	mod   = (int)( p % dy );
	if ( mod < 0 ) {
		delta--;
		mod += (TCoord)dy;
	}
	x = raster->x + delta;
	gray_render_scanline( raster, ey1, raster->x, fy1, x, (TCoord)first );

	ey1 += incr;
	gray_set_cell( raster, TRUNC( x ), ey1 );

	if ( ey1 != ey2 ) {
		p     = ONE_PIXEL * dx;
		lift  = (int)( p / dy );
		rem   = (int)( p % dy );
		if ( rem < 0 ) {
			lift--;
			rem += (int)dy;
		}
		mod -= (int)dy;

		while ( ey1 != ey2 ) {
			delta = lift;
			mod  += rem;
			if ( mod >= 0 )	{
				mod -= (int)dy;
				delta++;
			}

			x2 = x + delta;
			gray_render_scanline( raster, ey1, x, (TCoord)( ONE_PIXEL - first ), x2, (TCoord)first );
			x = x2;

			ey1 += incr;
			gray_set_cell( raster, TRUNC( x ), ey1 );
		}
	}

	gray_render_scanline( raster, ey1, x, (TCoord)( ONE_PIXEL - first ), to_x, fy2 );

End:
	raster->x       = to_x;
	raster->y       = to_y;
	raster->last_ey = SUBPIXELS( ey2 );
}



static int EVG_Outline_Decompose(EVG_Outline *outline, TRaster *user)
{
	EVG_Vector   v_start;
	int   n;         /* index of contour in outline     */
	int   first;     /* index of first point in contour */
	TPos _x, _y;

	first = 0;
	for ( n = 0; n < outline->n_contours; n++ ) {
		EVG_Vector *point;
		EVG_Vector *limit;
		int  last;  /* index of last point in contour */
		last  = outline->contours[n];
		limit = outline->points + last;
		v_start = outline->points[first];
		point = outline->points + first;
		gray_move_to(&v_start, user);
		while ( point < limit ) {
			point++;
			evg_translate_point(user->mx, point, &_x, &_y);
			gray_render_line(user, _x, _y);
		}
		/* close the contour with a line segment */
		evg_translate_point(user->mx, &v_start, &_x, &_y);
		gray_render_line(user, _x, _y);
		first = last + 1;
	}
	return 0;
}



#define SWAP_CELLS( a, b, temp )  {              \
                                    temp = *(a); \
                                    *(a) = *(b); \
                                    *(b) = temp; \
                                  }


/* This is a non-recursive quicksort that directly process our cells     */
/* array.  It should be faster than calling the stdlib qsort(), and we   */
/* can even tailor our insertion threshold...                            */

#define QSORT_THRESHOLD  9  /* below this size, a sub-array will be sorted */
/* through a normal insertion sort             */

void gray_quick_sort( AACell *cells, int    count )
{
	AACell *stack[80];  /* should be enough ;-) */
	AACell **top;        /* top of stack */
	AACell *base, *limit;
	AACell temp;

	limit = cells + count;
	base  = cells;
	top   = stack;

	for (;;) {
		int    len = (int)( limit - base );
		AACell *i, *j;
		if ( len > QSORT_THRESHOLD ) {
			AACell *pivot;
			/* we use base + len/2 as the pivot */
			pivot = base + len / 2;
			SWAP_CELLS( base, pivot, temp );

			i = base + 1;
			j = limit - 1;

			/* now ensure that *i <= *base <= *j */
			if(j->x < i->x)
				SWAP_CELLS( i, j, temp );

			if(base->x < i->x)
				SWAP_CELLS( base, i, temp );

			if(j->x < base->x)
				SWAP_CELLS( base, j, temp );

			for (;;) {
				int x = base->x;
				do i++;
				while( i->x < x );
				do j--;
				while( x < j->x );

				if ( i > j )
					break;

				SWAP_CELLS( i, j, temp );
			}

			SWAP_CELLS( base, j, temp );

			/* now, push the largest sub-array */
			if ( j - base > limit - i ) {
				top[0] = base;
				top[1] = j;
				base   = i;
			} else {
				top[0] = i;
				top[1] = limit;
				limit  = j;
			}
			top += 2;
		} else {
			/* the sub-array is small, perform insertion sort */
			j = base;
			i = j + 1;

			for ( ; i < limit; j = i, i++ ) {
				for ( ; j[1].x < j->x; j-- ) {
					SWAP_CELLS( j + 1, j, temp );
					if ( j == base )
						break;
				}
			}
			if ( top > stack ) {
				top  -= 2;
				base  = top[0];
				limit = top[1];
			} else
				break;
		}
	}
}

static void gray_hline( TRaster *raster, TCoord  x, TCoord  y, TPos area, int acount, Bool zero_non_zero_rule, u32 idx1, u32 idx2)
{
	int        coverage;
	EVG_Span*   span;
	int        count;

	x += (TCoord)raster->min_ex;
	if (x>=raster->max_ex) return;
	y += (TCoord)raster->min_ey;

	/* compute the coverage line's coverage, depending on the    */
	/* outline fill rule                                         */
	/*                                                           */
	/* the coverage percentage is area/(PIXEL_BITS*PIXEL_BITS*2) */
	/*                                                           */
	coverage = (int)( area >> ( PIXEL_BITS * 2 + 1 - 8 ) );
	/* use range 0..256 */
	if ( coverage < 0 )
		coverage = -coverage;

	if (zero_non_zero_rule) {
		/* normal non-zero winding rule */
		if ( coverage >= 256 )
			coverage = 255;
	} else {
		coverage &= 511;

		if ( coverage > 256 )
			coverage = 512 - coverage;
		else if ( coverage == 256 )
			coverage = 255;
	}

	if (!coverage)
		return;

	/* see if we can add this span to the current list */
	count = raster->num_gray_spans;
	span  = raster->gray_spans + count - 1;
	if ( count > 0                          &&
			(int)span->x + span->len == (int)x &&
			span->coverage == coverage )
	{
		span->len = (unsigned short)( span->len + acount );
		return;
	}

	if ((u32) count >= raster->max_gray_spans) {
		raster->render_span(y, count, raster->gray_spans, raster->render_span_data );
		raster->num_gray_spans = 0;

		span = raster->gray_spans;
	} else {
		if (count==raster->alloc_gray_spans) {
			raster->alloc_gray_spans*=2;
			raster->gray_spans = gf_realloc(raster->gray_spans, sizeof(EVG_Span)*raster->alloc_gray_spans);
			span = raster->gray_spans + count - 1;
		}
		span++;
	}

	/* add a gray span to the current list */
	span->x        = (short)x;
	span->len      = (unsigned short)acount;
	span->coverage = (unsigned char)coverage;
	span->idx1 = idx1;
	span->idx2 = idx2;
	raster->num_gray_spans++;
}

void gray_sweep_line( TRaster *raster, AAScanline *sl, int y, Bool zero_non_zero_rule)
{
	TCoord  cover;
	AACell *start, *cur;

	cur = sl->cells;
	cover = 0;
	raster->num_gray_spans = 0;

	while (sl->num) {
		TCoord  x;
		TArea   area;
		start  = cur;
		x      = start->x;
		area   = start->area;
		cover += start->cover;
		/* accumulate all start cells */
		while(--sl->num) {
			++cur ;
			if (cur->x != start->x )
				break;

			area  += cur->area;
			cover += cur->cover;
		}

		/* if the start cell has a non-null area, we must draw an */
		/* individual gray pixel there                            */
		if ( area && x >= 0 ) {
			gray_hline( raster, x, y, cover * ( ONE_PIXEL * 2 ) - area, 1, zero_non_zero_rule, start->idx1, start->idx2);
			x++;
		}
		if ( x < 0 ) x = 0;

		/* draw a gray span between the start cell and the current one */
		if ( cur->x > x )
			gray_hline( raster, x, y, cover * ( ONE_PIXEL * 2 ), cur->x - x, zero_non_zero_rule, cur->idx1, cur->idx2);
	}
	raster->render_span((int) (y + raster->min_ey), raster->num_gray_spans, raster->gray_spans, raster->render_span_data );
}


int evg_raster_render(GF_EVGSurface *surf)
{
	Bool zero_non_zero_rule;
	u32 i, size_y;
	EVG_Raster raster = surf->raster;
	EVG_Outline*  outline = (EVG_Outline*)&surf->ftoutline;

	/* return immediately if the outline is empty */
	if ( outline->n_points == 0 || outline->n_contours <= 0 ) return 0;

	raster->render_span  = (EVG_Raster_Span_Func) surf->gray_spans;
	raster->render_span_data = surf;

	/* Set up state in the raster object */
	raster->min_ex = surf->clip_xMin;
	raster->min_ey = surf->clip_yMin;
	raster->max_ex = surf->clip_xMax;
	raster->max_ey = surf->clip_yMax;

	raster->mx = surf->mx;

	size_y = (u32) (raster->max_ey - raster->min_ey);
	if (raster->max_lines < size_y) {
		raster->scanlines = (AAScanline*)gf_realloc(raster->scanlines, sizeof(AAScanline)*size_y);
		memset(&raster->scanlines[raster->max_lines], 0, sizeof(AAScanline)*(size_y-raster->max_lines) );
		raster->max_lines = size_y;
	}

	raster->ex = (int) (raster->max_ex+1);
	raster->ey = (int) (raster->max_ey+1);
	raster->cover = 0;
	raster->area = 0;
	raster->first_scanline = raster->max_ey;

	EVG_Outline_Decompose(outline, raster);
	gray_record_cell( raster );

	/*store odd/even rule*/
	zero_non_zero_rule = (outline->flags & GF_PATH_FILL_ZERO_NONZERO) ? GF_TRUE : GF_FALSE;

	/* sort each scanline and render it*/
	for (i=raster->first_scanline; i<size_y; i++) {
		AAScanline *sl = &raster->scanlines[i];
		if (sl->num) {
			if (sl->num>1) gray_quick_sort(sl->cells, sl->num);
			gray_sweep_line(raster, sl, i, zero_non_zero_rule);
			sl->num = 0;
		}
	}

	return 0;
}




EVG_Raster evg_raster_new()
{
	TRaster *raster;
	GF_SAFEALLOC(raster , TRaster);
	raster->max_gray_spans = raster->alloc_gray_spans = FT_MAX_GRAY_SPANS;
	raster->gray_spans = gf_malloc(sizeof(EVG_Span)* raster->max_gray_spans);
	return raster;
}

void evg_raster_del(EVG_Raster raster)
{
	u32 i;
	for (i=0; i<raster->max_lines; i++) {
		gf_free(raster->scanlines[i].cells);
		if (raster->scanlines[i].pixels)
			gf_free(raster->scanlines[i].pixels);
	}
	gf_free(raster->gray_spans);

	gf_free(raster->scanlines);
	gf_free(raster);
}

/* END */
