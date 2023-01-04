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


void gray_record_cell(GF_EVGSurface *surf)
{
	if (( surf->area | surf->cover) && (surf->ey<surf->max_ey)) {
		long y = surf->ey - surf->min_ey;
		if (y>=0) {
			AACell *cell;
			AAScanline *sl = &surf->scanlines[y];

			if (sl->num >= sl->alloc) {
				sl->cells = (AACell*)gf_realloc(sl->cells, sizeof(AACell)* (sl->alloc + AA_CELL_STEP_ALLOC));
				sl->alloc += AA_CELL_STEP_ALLOC;
			}
			cell = &sl->cells[sl->num];
			sl->num++;
			/*clip cell */
			if (surf->ex<surf->min_ex) cell->x = (TCoord) -1;
			else if (surf->ex>surf->max_ex) cell->x = (TCoord) (surf->max_ex - surf->min_ex);
			else cell->x = (TCoord)(surf->ex - surf->min_ex);
			cell->area = surf->area;
			cell->cover = surf->cover;
			cell->idx1 = surf->idx1;
			cell->idx2 = surf->idx2;

			if (surf->first_scanline > (u32) y)
				surf->first_scanline = y;
		}
	}
}

void gray_set_cell(GF_EVGSurface *surf, TCoord  ex, TCoord  ey )
{
	if ((surf->ex != ex) || (surf->ey != ey)) {
		gray_record_cell(surf);
		surf->ex = ex;
		surf->ey = ey;
		surf->area = 0;
		surf->cover = 0;
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

static GFINLINE int gray_move_to(EVG_Vector *to, GF_EVGSurface *surf)
{
	TPos  x, y;
	TCoord  ex, ey;

	/* record current cell, if any */
	gray_record_cell(surf);

	evg_translate_point(surf->mx, to, &x, &y);

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

/*************************************************************************/
/*                                                                       */
/* Render a scanline as one or more cells.                               */
/*                                                                       */
static void gray_render_scanline(GF_EVGSurface *surf, TCoord ey, TPos x1, TCoord y1, TPos x2, TCoord y2)
{
	TCoord  ex1, ex2, fx1, fx2, delta;
	long    p, first;
	long dx;
	int     incr, mod;

	dx = x2 - x1;
	ex1 = TRUNC( x1 ); /* if (ex1 >= surf->max_ex) ex1 = surf->max_ex-1; */
	ex2 = TRUNC( x2 ); /* if (ex2 >= surf->max_ex) ex2 = surf->max_ex-1; */
	if (ex1<0) {
		ex1=0;
		fx1 = (TCoord) 0;
	} else {
		fx1 = (TCoord)( x1 - SUBPIXELS( ex1 ) );
	}
	if (ex2<0) {
		ex2=0;
		fx2 = (TCoord) 0;
	} else {
		fx2 = (TCoord)( x2 - SUBPIXELS( ex2 ) );
	}
	/* trivial case.  Happens often */
	if ( y1 == y2 ) {
		gray_set_cell(surf, ex2, ey );
		return;
	}

	/* everything is located in a single cell.  That is easy! */
	if ( ex1 == ex2 ) {
		delta      = y2 - y1;
		surf->area  += (TArea)( fx1 + fx2 ) * delta;
		surf->cover += delta;
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
	surf->area  += (TArea)( fx1 + first ) * delta;
	surf->cover += delta;

	ex1 += incr;
	gray_set_cell(surf, ex1, ey );
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

			surf->area  += (TArea)ONE_PIXEL * delta;
			surf->cover += delta;
			y1        += delta;
			ex1       += incr;
			gray_set_cell(surf, ex1, ey );
		}
	}
	delta      = y2 - y1;
	surf->area  += (TArea)( fx2 + ONE_PIXEL - first ) * delta;
	surf->cover += delta;
}


/*************************************************************************/
/*                                                                       */
/* Render a given line as a series of scanlines.                         */
void gray_render_line(GF_EVGSurface *surf, TPos to_x, TPos to_y)
{
	TCoord  min, max;
	TCoord  ey1, ey2, fy1, fy2;
	TPos    x, x2;
	long dx, dy;
	long    p, first;
	int     delta, rem, mod, lift, incr;


	ey1 = TRUNC( surf->last_ey );
	ey2 = TRUNC( to_y ); /* if (ey2 >= surf->max_ey) ey2 = surf->max_ey-1; */
	if (ey2<0) ey2=0;
	fy1 = (TCoord)( surf->y - surf->last_ey );
	fy2 = (TCoord)( to_y - SUBPIXELS( ey2 ) );

	dx = to_x - surf->x;
	dy = to_y - surf->y;

	/* perform vertical clipping */
	min = ey1;
	max = ey2;
	if ( ey1 > ey2 ) {
		min = ey2;
		max = ey1;
	}
	if ( min >= surf->max_ey || max < surf->min_ey ) goto End;

	/* everything is on a single scanline */
	if ( ey1 == ey2 ) {
		gray_render_scanline(surf, ey1, surf->x, fy1, to_x, fy2 );
		goto End;
	}
	/* vertical line - avoid calling gray_render_scanline */
	incr = 1;
	if (dx == 0 ) {
		TCoord  ex     = TRUNC( surf->x );
		TCoord tdiff;
		if (ex<0) {
			ex = 0;
			tdiff=0;
		} else {
			tdiff = surf->x - SUBPIXELS( ex );
		}
		TCoord  two_fx = (TCoord)( ( tdiff ) << 1 );
		TPos    area;

		first = ONE_PIXEL;
		if ( dy < 0 ) {
			first = 0;
			incr  = -1;
		}

		delta      = (int)( first - fy1 );
		surf->area  += (TArea)two_fx * delta;
		surf->cover += delta;
		ey1       += incr;

		gray_set_cell(surf, ex, ey1 );

		delta = (int)( first + first - ONE_PIXEL );
		area  = (TArea)two_fx * delta;
		while ( ey1 != ey2 ) {
			surf->area  += area;
			surf->cover += delta;
			ey1       += incr;
			gray_set_cell(surf, ex, ey1 );
		}
		delta      = (int)( fy2 - ONE_PIXEL + first );
		surf->area  += (TArea)two_fx * delta;
		surf->cover += delta;
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
	x = surf->x + delta;
	gray_render_scanline(surf, ey1, surf->x, fy1, x, (TCoord)first );

	ey1 += incr;
	gray_set_cell(surf, TRUNC( x ), ey1 );

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
			gray_render_scanline(surf, ey1, x, (TCoord)( ONE_PIXEL - first ), x2, (TCoord)first );
			x = x2;

			ey1 += incr;
			gray_set_cell(surf, TRUNC( x ), ey1 );
		}
	}

	gray_render_scanline(surf, ey1, x, (TCoord)( ONE_PIXEL - first ), to_x, fy2 );

End:
	surf->x       = to_x;
	surf->y       = to_y;
	surf->last_ey = SUBPIXELS( ey2 );
}



static int EVG_Outline_Decompose(EVG_Outline *outline, GF_EVGSurface *surf)
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
		gray_move_to(&v_start, surf);
		while ( point < limit ) {
			point++;
			evg_translate_point(surf->mx, point, &_x, &_y);
			gray_render_line(surf, _x, _y);
		}
		/* close the contour with a line segment */
		evg_translate_point(surf->mx, &v_start, &_x, &_y);
		gray_render_line(surf, _x, _y);
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

static void gray_hline(EVGRasterCtx *raster, TCoord  x, TCoord  y, TPos area, int acount, u32 fill_rule, u32 idx1, u32 idx2)
{
	int        coverage;
	EVG_Span*   span;
	int        count;
	int        odd_flag = 1;

	x += (TCoord)raster->surf->min_ex;
	if (x>=raster->surf->max_ex) return;
	y += (TCoord)raster->surf->min_ey;

	/* compute the coverage line's coverage, depending on the    */
	/* outline fill rule                                         */
	/*                                                           */
	/* the coverage percentage is area/(PIXEL_BITS*PIXEL_BITS*2) */
	/*                                                           */
	coverage = (int)( area >> ( PIXEL_BITS * 2 + 1 - 8 ) );
	/* use range 0..256 */
	if ( coverage < 0 )
		coverage = -coverage;

	if (fill_rule) {
		/* odd / even winding rule */
		if ( coverage >= 256 ) {
			coverage &= 511;

			if ( coverage > 256 )
				coverage = 512 - coverage;
			else if ( coverage == 256 )
				coverage = 255;

			if (coverage!=255)
				odd_flag = 0;

			/* even only fill */
			if (fill_rule==2) {
				coverage = odd_flag ? 0 : (255-coverage);
			} else {
				coverage = 255;
			}
		}
		else if (fill_rule==2) {
			coverage = 255-coverage;
		}
	} else {
		// normal non-zero winding rule
		if ( coverage >= 256 )
			coverage = 255;
	}

	if ((coverage < 0xFF) && (coverage > raster->surf->aa_level))
		return;
		
	if (!coverage)
		return;

	/* see if we can add this span to the current list */
	count = raster->num_gray_spans;
	span  = raster->gray_spans + count - 1;
	if ( (count > 0)
		&& ((int)span->x + span->len == (int)x)
		&& (span->coverage == coverage )
		&& (span->odd_flag == odd_flag )
	) {
		span->len = (unsigned short)( span->len + acount );
		return;
	}

	if ((u32) count >= raster->max_gray_spans) {
		raster->surf->render_span(y, count, raster->gray_spans, raster->surf, raster);
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
	span->odd_flag = (unsigned char)odd_flag;
	span->idx1 = idx1;
	span->idx2 = idx2;
	raster->num_gray_spans++;
}

void gray_sweep_line(EVGRasterCtx *raster, AAScanline *sl, int y, u32 fill_rule)
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
		assert(sl->num);
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
			gray_hline( raster, x, y, cover * ( ONE_PIXEL * 2 ) - area, 1, fill_rule, start->idx1, start->idx2);
			x++;
		}
		if ( x < 0 ) x = 0;

		/* draw a gray span between the start cell and the current one */
		if ( cur->x > x )
			gray_hline( raster, x, y, cover * ( ONE_PIXEL * 2 ), cur->x - x, fill_rule, cur->idx1, cur->idx2);
	}
	raster->surf->render_span((int) (y + raster->surf->min_ey), raster->num_gray_spans, raster->gray_spans, raster->surf, raster );
}

#define LINES_PER_THREAD	6
static Bool th_fetch_lines(EVGRasterCtx *rctx)
{
	gf_mx_p(rctx->surf->raster_mutex);
	if (!rctx->surf->last_dispatch_line) {
		rctx->first_line = rctx->last_line = 0;
		gf_mx_v(rctx->surf->raster_mutex);
		return GF_FALSE;
	}
	rctx->first_line = rctx->surf->last_dispatch_line;
	rctx->surf->last_dispatch_line += LINES_PER_THREAD;
	if (rctx->surf->last_dispatch_line >= rctx->surf->max_line_y) {
		rctx->surf->last_dispatch_line = 0;
		rctx->last_line = rctx->surf->max_line_y;
	} else {
		rctx->last_line = rctx->surf->last_dispatch_line;
	}
	gf_mx_v(rctx->surf->raster_mutex);
	return GF_TRUE;
}

u32 th_sweep_lines(void *par)
{
	EVGRasterCtx *rctx = par;

	while (rctx->th_state == 1) {
		u32 i;
		u32 first_patch, last_patch;

		//only for threads, wait for start raster event
		if (rctx->th) {
			//we are active, don't grab sema (final flush of sweep)
			if (rctx->active) {
				gf_sleep(0);
				continue;
			}
			gf_sema_wait(rctx->surf->raster_sem);
			if (!rctx->th_state) break;
			rctx->active = GF_TRUE;
		}

		first_patch = 0xFFFFFFFF;
		last_patch = 0;

		while (1) {
			/* sort each scanline and render it*/
			for (i=rctx->first_line; i<rctx->last_line; i++) {
				AAScanline *sl = &rctx->surf->scanlines[i];
				if (sl->num) {
					if (sl->num>1) gray_quick_sort(sl->cells, sl->num);
					gray_sweep_line(rctx, sl, i, rctx->fill_rule);
					sl->num = 0;
				} else if (rctx->is_tri_raster) {
					//if nothing on this line, we are done for this quad/triangle
					break;
				}
				//only true if rctx->is_tri_raster is true
				if (sl->pnum) {
					if (first_patch > i) first_patch = i;
					if (last_patch < i) last_patch = i;
				}
			}
			if (!th_fetch_lines(rctx))
				break;
		}
		gf_mx_p(rctx->surf->raster_mutex);
		if (rctx->is_tri_raster) {
			if (first_patch<rctx->surf->first_patch)
				rctx->surf->first_patch = first_patch;
			if (last_patch>rctx->surf->last_patch)
				rctx->surf->last_patch = last_patch;
		}
		rctx->surf->pending_threads--;
		gf_mx_v(rctx->surf->raster_mutex);

		if (!rctx->th) break;
	}
	//we are done
	rctx->th_state = 2;
	return 0;
}


/* sort each scanline and render it*/
GF_Err evg_sweep_lines(GF_EVGSurface *surf, u32 size_y, u32 fill_rule, Bool is_tri_raster, GF_EVGFragmentParam *fparam)
{
	u32 i;

	if (fparam) {
		surf->raster_ctx.frag_param = *fparam;
		if (surf->frag_shader_init)
			surf->frag_shader_init(surf->frag_shader_udta, &surf->raster_ctx.frag_param, 0, GF_FALSE);

		//shaders are not thread-safe yet
		for (i=0; i<surf->nb_threads; i++) {
			surf->th_raster_ctx[i].frag_param = *fparam;
			if (surf->frag_shader_init)
				surf->frag_shader_init(surf->frag_shader_udta, &surf->th_raster_ctx[i].frag_param, i+1, GF_FALSE);
		}
	}

	if (!surf->nb_threads) {
		for (i=surf->first_scanline; i<size_y; i++) {
			AAScanline *sl = &surf->scanlines[i];
			if (sl->num) {
				if (sl->num>1) gray_quick_sort(sl->cells, sl->num);
				gray_sweep_line(&surf->raster_ctx, sl, i, fill_rule);
				sl->num = 0;
			} else if (is_tri_raster) {
				//if nothing on this line, we are done for this quad/triangle
				break;
			}
			if (sl->pnum) {
				if (surf->first_patch > i) surf->first_patch = i;
				if (surf->last_patch < i) surf->last_patch = i;
			}
		}
		return GF_OK;
	}

	surf->raster_ctx.fill_rule = fill_rule;
	surf->raster_ctx.is_tri_raster = is_tri_raster ? 1 : 0;
	surf->raster_ctx.first_line = surf->first_scanline;
	surf->raster_ctx.last_line = surf->raster_ctx.first_line + LINES_PER_THREAD;
	if (surf->raster_ctx.first_line%2) surf->raster_ctx.last_line++;

	if (surf->raster_ctx.last_line >= size_y) {
		surf->raster_ctx.last_line = size_y;
		surf->last_dispatch_line = 0;
	}

	surf->pending_threads = 1;
	surf->max_line_y = size_y;

	surf->last_dispatch_line = surf->raster_ctx.last_line;
	for (i=0; i<surf->nb_threads; i++) {
		EVGRasterCtx *rctx = &surf->th_raster_ctx[i];
		surf->pending_threads++;

		if (!surf->last_dispatch_line) {
			rctx->first_line = 0;
			rctx->last_line = 0;
			continue;
		}
		rctx->first_line = surf->last_dispatch_line;
		rctx->last_line = surf->last_dispatch_line + LINES_PER_THREAD;
		if (rctx->last_line >= size_y) {
			rctx->last_line = size_y;
			surf->last_dispatch_line = 0;
		} else {
			surf->last_dispatch_line += LINES_PER_THREAD;
		}
		rctx->fill_rule = fill_rule;
		rctx->is_tri_raster = is_tri_raster ? 1 : 0;
	}

	//notify semaphore
	gf_sema_notify(surf->raster_sem, surf->nb_threads);

	//run using caller process
	surf->raster_ctx.th_state = 1;
	th_sweep_lines(&surf->raster_ctx);

	while (surf->pending_threads) {
		gf_sleep(0);
	}

	//move all threads to inactive so that they grab the sema
	for (i=0; i<surf->nb_threads; i++) {
		EVGRasterCtx *rctx = &surf->th_raster_ctx[i];
		rctx->active = GF_FALSE;
	}

	if (fparam && surf->frag_shader_init) {
		surf->frag_shader_init(surf->frag_shader_udta, &surf->raster_ctx.frag_param, 0, GF_TRUE);
		for (i=0; i<surf->nb_threads; i++) {
			surf->frag_shader_init(surf->frag_shader_udta, &surf->th_raster_ctx[i].frag_param, i+1, GF_TRUE);
		}
	}

	return GF_OK;
}


GF_Err evg_raster_render(GF_EVGSurface *surf)
{
	u32 fill_rule = 0;
	u32 size_y;
	EVG_Outline*  outline = (EVG_Outline*)&surf->ftoutline;

	/* return immediately if the outline is empty */
	if ( outline->n_points == 0 || outline->n_contours <= 0 ) return GF_OK;

	/* Set up state in the raster object */
	surf->min_ex = surf->clip_xMin;
	surf->min_ey = surf->clip_yMin;
	surf->max_ex = surf->clip_xMax;
	surf->max_ey = surf->clip_yMax;
	size_y = (u32) (surf->max_ey - surf->min_ey);
	if (surf->max_lines < size_y) {
		surf->scanlines = (AAScanline*)gf_realloc(surf->scanlines, sizeof(AAScanline)*size_y);
		if (!surf->scanlines) return GF_OUT_OF_MEM;
		memset(&surf->scanlines[surf->max_lines], 0, sizeof(AAScanline)*(size_y-surf->max_lines) );
		surf->max_lines = size_y;
	}

	surf->ex = (int) (surf->max_ex+1);
	surf->ey = (int) (surf->max_ey+1);
	surf->cover = 0;
	surf->area = 0;
	surf->first_scanline = surf->max_ey;

	EVG_Outline_Decompose(outline, surf);
	gray_record_cell(surf);

	/*store odd/even rule*/
	if (outline->flags & GF_PATH_FILL_ZERO_NONZERO) fill_rule = 1;
	else if (outline->flags & GF_PATH_FILL_EVEN) fill_rule = 2;

	if (surf->mask_mode == GF_EVGMASK_RECORD) {
		surf->render_span = evg_fill_span_mask;
	} else {
		surf->render_span = (EVG_SpanFunc) surf->fill_spans;
	}

	if ((void *) surf->sten == (void *) &surf->shader_sten) {
		GF_EVGFragmentParam fparam;
		memset(&fparam, 0, sizeof(GF_EVGFragmentParam));
		fparam.tx_width = surf->shader_sten.width;
		fparam.tx_height = surf->shader_sten.height;
		return evg_sweep_lines(surf, size_y, fill_rule, GF_FALSE, &fparam);
	}

	return evg_sweep_lines(surf, size_y, fill_rule, GF_FALSE, NULL);

}



/* END */
