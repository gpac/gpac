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

/*
	JLF: modified to support only standalone mode and NO STATIC RASTER
	renamed base types to avoid any conflict with freeType support
*/
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


#define ErrRaster_MemoryOverflow   -4


#include "rast_soft.h"

#include <setjmp.h>
#include <limits.h>
#define FT_UINT_MAX  UINT_MAX

#define ft_memset   memset
#define ft_setjmp   setjmp
#define ft_longjmp  longjmp
#define ft_jmp_buf  jmp_buf


#define ErrRaster_Invalid_Mode     -2
#define ErrRaster_Invalid_Outline  -1


  /* This macro is used to indicate that a function parameter is unused. */
  /* Its purpose is simply to reduce compiler warnings.  Note also that  */
  /* simply defining it as `(void)x' doesn't avoid warnings with certain */
  /* ANSI compilers (e.g. LCC).                                          */
#define FT_UNUSED( x )  (x) = (x)

  /* Disable the tracing mechanism for simplicity -- developers can      */
  /* activate it easily by redefining these two macros.                  */
#ifndef FT_ERROR
#define FT_ERROR( x )  do ; while ( 0 )     /* nothing */
#endif

#ifndef FT_TRACE
#define FT_TRACE( x )  do ; while ( 0 )     /* nothing */
#endif


#ifndef FT_MEM_SET
#define FT_MEM_SET( d, s, c )  ft_memset( d, s, c )
#endif

#ifndef FT_MEM_ZERO
#define FT_MEM_ZERO( dest, count )  FT_MEM_SET( dest, 0, count )
#endif

  /* define this to dump debugging information */
#define xxxDEBUG_GRAYS


#define GPAC_FIX_BITS		16

  /* must be at least 6 bits! */
#define PIXEL_BITS  8

#define ONE_PIXEL       ( 1L << PIXEL_BITS )
#define PIXEL_MASK      ( -1L << PIXEL_BITS )
#define TRUNC( x )      ( (TCoord)((x) >> PIXEL_BITS) )
#define SUBPIXELS( x )  ( (TPos)(x) << PIXEL_BITS )
#define FLOOR( x )      ( (x) & -ONE_PIXEL )
#define CEILING( x )    ( ( (x) + ONE_PIXEL - 1 ) & -ONE_PIXEL )
#define ROUND( x )      ( ( (x) + ONE_PIXEL / 2 ) & -ONE_PIXEL )

#if PIXEL_BITS >= GPAC_FIX_BITS
#define UPSCALE( x )    ( (x) << ( PIXEL_BITS - GPAC_FIX_BITS ) )
#define DOWNSCALE( x )  ( (x) >> ( PIXEL_BITS - GPAC_FIX_BITS ) )
#else
#define UPSCALE( x )    ( (x) >> ( GPAC_FIX_BITS - PIXEL_BITS ) )
#define DOWNSCALE( x )  ( (x) << ( GPAC_FIX_BITS - PIXEL_BITS ) )
#endif

  /* Define this if you want to use a more compact storage scheme.  This   */
  /* increases the number of cells available in the render pool but slows  */
  /* down the rendering a bit.  It is useful if you have a really tiny     */
  /* render pool.                                                          */
#undef GRAYS_COMPACT

  /*************************************************************************/
  /*                                                                       */
  /*   TYPE DEFINITIONS                                                    */
  /*                                                                       */

  /* don't change the following types to FT_Int or FT_Pos, since we might */
  /* need to define them to "float" or "double" when experimenting with   */
  /* new algorithms                                                       */

  typedef int   TCoord;   /* integer scanline/pixel coordinate */
  typedef long  TPos;     /* sub-pixel coordinate              */

  /* determine the type used to store cell areas.  This normally takes at */
  /* least PIXEL_BYTES*2 + 1.  On 16-bit systems, we need to use `long'   */
  /* instead of `int', otherwise bad things happen                        */

#if PIXEL_BITS <= 7

  typedef int   TArea;

#else /* PIXEL_BITS >= 8 */

  /* approximately determine the size of integers using an ANSI-C header */
#if FT_UINT_MAX == 0xFFFFU
  typedef long  TArea;
#else
  typedef int  TArea;
#endif

#endif /* PIXEL_BITS >= 8 */


  /* maximal number of gray spans in a call to the span callback */
#define FT_MAX_GRAY_SPANS  64


#ifdef GRAYS_COMPACT

  typedef struct  TCell_
  {
    short  x     : 14;
    short  y     : 14;
    int    cover : PIXEL_BITS + 2;
    int    area  : PIXEL_BITS * 2 + 2;

  } TCell, *PCell;

#else /* GRAYS_COMPACT */

  typedef struct  TCell_
  {
    TCoord  x;
    TCoord  y;
    int     cover;
    TArea   area;

  } TCell, *PCell;

#endif /* GRAYS_COMPACT */



	
typedef int (*EVG_Outline_MoveToFunc)(EVG_Vector*  to, void* user );
#define EVG_Outline_MoveTo_Func  EVG_Outline_MoveToFunc

typedef int (*EVG_Outline_LineToFunc)(EVG_Vector*  to, void*       user );
#define  EVG_Outline_LineTo_Func  EVG_Outline_LineToFunc

typedef int (*EVG_Outline_ConicToFunc)(EVG_Vector*  control, EVG_Vector*  to, void*       user );
#define  EVG_Outline_ConicTo_Func  EVG_Outline_ConicToFunc

typedef int (*EVG_Outline_CubicToFunc)(EVG_Vector*  control1, EVG_Vector*  control2, EVG_Vector*  to, void*       user );
#define  EVG_Outline_CubicTo_Func  EVG_Outline_CubicToFunc


typedef struct
{
	EVG_Outline_MoveToFunc   move_to;
	EVG_Outline_LineToFunc   line_to;
	EVG_Outline_ConicToFunc  conic_to;
	EVG_Outline_CubicToFunc  cubic_to;
	s32 shift;
	s32 delta;
} EVG_Outline_Funcs;

  typedef struct  TRaster_
  {
    PCell   cells;
    int     max_cells;
    int     num_cells;

    TPos    min_ex, max_ex;
    TPos    min_ey, max_ey;

    TArea   area;
    int     cover;
    int     invalid;

    TCoord  ex, ey;
    TCoord  cx, cy;
    TPos    x,  y;

    TPos    last_ey;

    EVG_Vector   bez_stack[32 * 3 + 1];
    int         lev_stack[32];

    EVG_Outline  outline;
    EVG_BBox     clip_box;

    EVG_Span     gray_spans[FT_MAX_GRAY_SPANS];
    int         num_gray_spans;

    EVG_Raster_Span_Func  render_span;
    void*                render_span_data;
    int                  span_y;

    int  band_size;
    int  band_shoot;
    int  conic_level;
    int  cubic_level;

    void*       memory;
    ft_jmp_buf  jump_buffer;
  } TRaster;


  /*************************************************************************/
  /*                                                                       */
  /* Initialize the cells table.                                           */
  /*                                                                       */
  static void
  gray_init_cells( TRaster *raster, void*  buffer,
                   long            byte_size )
  {
    raster->cells     = (PCell)buffer;
    raster->max_cells = (int)( byte_size / sizeof ( TCell ) );
    raster->num_cells = 0;
    raster->area      = 0;
    raster->cover     = 0;
    raster->invalid   = 1;
  }


  /*************************************************************************/
  /*                                                                       */
  /* Compute the outline bounding box.                                     */
  /*                                                                       */
  static void
  gray_compute_cbox( TRaster *raster )
  {
    EVG_Outline*  outline = &raster->outline;
    EVG_Vector*   vec     = outline->points;
    EVG_Vector*   limit   = vec + outline->n_points;


    if ( outline->n_points <= 0 )
    {
      raster->min_ex = raster->max_ex = 0;
      raster->min_ey = raster->max_ey = 0;
      return;
    }

    raster->min_ex = raster->max_ex = vec->x;
    raster->min_ey = raster->max_ey = vec->y;

    vec++;

    for ( ; vec < limit; vec++ )
    {
      TPos  x = vec->x;
      TPos  y = vec->y;


      if ( x < raster->min_ex ) raster->min_ex = x;
      if ( x > raster->max_ex ) raster->max_ex = x;
      if ( y < raster->min_ey ) raster->min_ey = y;
      if ( y > raster->max_ey ) raster->max_ey = y;
    }

    /* truncate the bounding box to integer pixels */
    raster->min_ex = raster->min_ex >> GPAC_FIX_BITS;
    raster->min_ey = raster->min_ey >> GPAC_FIX_BITS;
    raster->max_ex = ( raster->max_ex + ((1<<GPAC_FIX_BITS) -1) ) >> GPAC_FIX_BITS;
    raster->max_ey = ( raster->max_ey + ((1<<GPAC_FIX_BITS) -1) ) >> GPAC_FIX_BITS;
  }


  /*************************************************************************/
  /*                                                                       */
  /* Record the current cell in the table.                                 */
  /*                                                                       */
  static void
  gray_record_cell( TRaster *raster )
  {
    PCell  cell;


    if ( !raster->invalid && ( raster->area | raster->cover ) )
    {
      if ( raster->num_cells >= raster->max_cells )
        ft_longjmp( raster->jump_buffer, 1 );

      cell        = raster->cells + raster->num_cells++;
      cell->x     = (TCoord)(raster->ex - raster->min_ex);
      cell->y     = (TCoord)(raster->ey - raster->min_ey);
      cell->area  = raster->area;
      cell->cover = raster->cover;
    }
  }


  /*************************************************************************/
  /*                                                                       */
  /* Set the current cell to a new position.                               */
  /*                                                                       */
  static void
  gray_set_cell( TRaster *raster, TCoord  ex,
                          TCoord  ey )
  {
    int  invalid, record, clean;


    /* Move the cell pointer to a new position.  We set the `invalid'      */
    /* flag to indicate that the cell isn't part of those we're interested */
    /* in during the render phase.  This means that:                       */
    /*                                                                     */
    /* . the new vertical position must be within min_ey..max_ey-1.        */
    /* . the new horizontal position must be strictly less than max_ex     */
    /*                                                                     */
    /* Note that if a cell is to the left of the clipping region, it is    */
    /* actually set to the (min_ex-1) horizontal position.                 */

    record  = 0;
    clean   = 1;

    invalid = ( ey < raster->min_ey || ey >= raster->max_ey || ex >= raster->max_ex );
    if ( !invalid )
    {
      /* All cells that are on the left of the clipping region go to the */
      /* min_ex - 1 horizontal position.                                 */
      if ( ex < raster->min_ex )
        ex = (TCoord)(raster->min_ex - 1);

      /* if our position is new, then record the previous cell */
      if ( ex != raster->ex || ey != raster->ey )
        record = 1;
      else
        clean = raster->invalid;  /* do not clean if we didn't move from */
                              /* a valid cell                        */
    }

    /* record the previous cell if needed (i.e., if we changed the cell */
    /* position, of changed the `invalid' flag)                         */
    if ( raster->invalid != invalid || record )
      gray_record_cell( raster );

    if ( clean )
    {
      raster->area  = 0;
      raster->cover = 0;
    }

    raster->invalid = invalid;
    raster->ex      = ex;
    raster->ey      = ey;
  }


  /*************************************************************************/
  /*                                                                       */
  /* Start a new contour at a given cell.                                  */
  /*                                                                       */
  static void
  gray_start_cell( TRaster *raster,  TCoord  ex,
                             TCoord  ey )
  {
    if ( ex < raster->min_ex )
      ex = (TCoord)(raster->min_ex - 1);

    raster->area    = 0;
    raster->cover   = 0;
    raster->ex      = ex;
    raster->ey      = ey;
    raster->last_ey = SUBPIXELS( ey );
    raster->invalid = 0;

    gray_set_cell( raster, ex, ey );
  }


  /*************************************************************************/
  /*                                                                       */
  /* Render a scanline as one or more cells.                               */
  /*                                                                       */
  static void
  gray_render_scanline( TRaster *raster,  TCoord  ey,
                                  TPos    x1,
                                  TCoord  y1,
                                  TPos    x2,
                                  TCoord  y2 )
  {
    TCoord  ex1, ex2, fx1, fx2, delta;
    long    p, first, dx;
    int     incr, lift, mod, rem;


    dx = x2 - x1;

    ex1 = TRUNC( x1 ); /* if (ex1 >= raster->max_ex) ex1 = raster->max_ex-1; */
    ex2 = TRUNC( x2 ); /* if (ex2 >= raster->max_ex) ex2 = raster->max_ex-1; */
    fx1 = (TCoord)( x1 - SUBPIXELS( ex1 ) );
    fx2 = (TCoord)( x2 - SUBPIXELS( ex2 ) );

    /* trivial case.  Happens often */
    if ( y1 == y2 )
    {
      gray_set_cell( raster, ex2, ey );
      return;
    }

    /* everything is located in a single cell.  That is easy! */
    /*                                                        */
    if ( ex1 == ex2 )
    {
      delta      = y2 - y1;
      raster->area  += (TArea)( fx1 + fx2 ) * delta;
      raster->cover += delta;
      return;
    }

    /* ok, we'll have to render a run of adjacent cells on the same */
    /* scanline...                                                  */
    /*                                                              */
    p     = ( ONE_PIXEL - fx1 ) * ( y2 - y1 );
    first = ONE_PIXEL;
    incr  = 1;

    if ( dx < 0 )
    {
      p     = fx1 * ( y2 - y1 );
      first = 0;
      incr  = -1;
      dx    = -dx;
    }

    delta = (TCoord)( p / dx );
    mod   = (TCoord)( p % dx );
    if ( mod < 0 )
    {
      delta--;
      mod += (TCoord)dx;
    }

    raster->area  += (TArea)( fx1 + first ) * delta;
    raster->cover += delta;

    ex1 += incr;
    gray_set_cell( raster, ex1, ey );
    y1  += delta;

    if ( ex1 != ex2 )
    {
      p     = ONE_PIXEL * ( y2 - y1 + delta );
      lift  = (TCoord)( p / dx );
      rem   = (TCoord)( p % dx );
      if ( rem < 0 )
      {
        lift--;
        rem += (TCoord)dx;
      }

      mod -= (int) dx;

      while ( ex1 != ex2 )
      {
        delta = lift;
        mod  += rem;
        if ( mod >= 0 )
        {
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
  /*                                                                       */
  static void
  gray_render_line( TRaster *raster, TPos  to_x,
                             TPos  to_y )
  {
    TCoord  ey1, ey2, fy1, fy2;
    TPos    dx, dy, x, x2;
    long    p, first;
    int     delta, rem, mod, lift, incr;


    ey1 = TRUNC( raster->last_ey );
    ey2 = TRUNC( to_y ); /* if (ey2 >= raster->max_ey) ey2 = raster->max_ey-1; */
    fy1 = (TCoord)( raster->y - raster->last_ey );
    fy2 = (TCoord)( to_y - SUBPIXELS( ey2 ) );

    dx = to_x - raster->x;
    dy = to_y - raster->y;

    /* XXX: we should do something about the trivial case where dx == 0, */
    /*      as it happens very often!                                    */

    /* perform vertical clipping */
    {
      TCoord  min, max;


      min = ey1;
      max = ey2;
      if ( ey1 > ey2 )
      {
        min = ey2;
        max = ey1;
      }
      if ( min >= raster->max_ey || max < raster->min_ey )
        goto End;
    }

    /* everything is on a single scanline */
    if ( ey1 == ey2 )
    {
      gray_render_scanline( raster, ey1, raster->x, fy1, to_x, fy2 );
      goto End;
    }

    /* vertical line - avoid calling gray_render_scanline */
    incr = 1;

    if ( dx == 0 )
    {
      TCoord  ex     = TRUNC( raster->x );
      TCoord  two_fx = (TCoord)( ( raster->x - SUBPIXELS( ex ) ) << 1 );
      TPos    area;


      first = ONE_PIXEL;
      if ( dy < 0 )
      {
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
      while ( ey1 != ey2 )
      {
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

    if ( dy < 0 )
    {
      p     = fy1 * dx;
      first = 0;
      incr  = -1;
      dy    = -dy;
    }

    delta = (int)( p / dy );
    mod   = (int)( p % dy );
    if ( mod < 0 )
    {
      delta--;
      mod += (TCoord)dy;
    }

    x = raster->x + delta;
    gray_render_scanline( raster, ey1, raster->x, fy1, x, (TCoord)first );

    ey1 += incr;
    gray_set_cell( raster, TRUNC( x ), ey1 );

    if ( ey1 != ey2 )
    {
      p     = ONE_PIXEL * dx;
      lift  = (int)( p / dy );
      rem   = (int)( p % dy );
      if ( rem < 0 )
      {
        lift--;
        rem += (int)dy;
      }
      mod -= (int)dy;

      while ( ey1 != ey2 )
      {
        delta = lift;
        mod  += rem;
        if ( mod >= 0 )
        {
          mod -= (int)dy;
          delta++;
        }

        x2 = x + delta;
        gray_render_scanline( raster, ey1, x,
                                       (TCoord)( ONE_PIXEL - first ), x2,
                                       (TCoord)first );
        x = x2;

        ey1 += incr;
        gray_set_cell( raster, TRUNC( x ), ey1 );
      }
    }

    gray_render_scanline( raster, ey1, x,
                                   (TCoord)( ONE_PIXEL - first ), to_x,
                                   fy2 );

  End:
    raster->x       = to_x;
    raster->y       = to_y;
    raster->last_ey = SUBPIXELS( ey2 );
  }


  static void
  gray_split_conic( EVG_Vector*  base )
  {
    TPos  a, b;


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


  static void
  gray_render_conic( TRaster *raster, EVG_Vector*  control,
                              EVG_Vector*  to )
  {
    TPos        dx, dy;
    int         top, level;
    int*        levels;
    EVG_Vector*  arc;


    dx = DOWNSCALE( raster->x ) + to->x - ( control->x << 1 );
    if ( dx < 0 )
      dx = -dx;
    dy = DOWNSCALE( raster->y ) + to->y - ( control->y << 1 );
    if ( dy < 0 )
      dy = -dy;
    if ( dx < dy )
      dx = dy;

    level = 1;
    dx = dx / raster->conic_level;
    while ( dx > 0 )
    {
      dx >>= 2;
      level++;
    }

    /* a shortcut to speed things up */
    if ( level <= 1 )
    {
      /* we compute the mid-point directly in order to avoid */
      /* calling gray_split_conic()                          */
      TPos   to_x, to_y, mid_x, mid_y;


      to_x  = UPSCALE( to->x );
      to_y  = UPSCALE( to->y );
      mid_x = ( raster->x + to_x + 2 * UPSCALE( control->x ) ) / 4;
      mid_y = ( raster->y + to_y + 2 * UPSCALE( control->y ) ) / 4;

      gray_render_line( raster, mid_x, mid_y );
      gray_render_line( raster, to_x, to_y );
      return;
    }

    arc       = raster->bez_stack;
    levels    = raster->lev_stack;
    top       = 0;
    levels[0] = level;

    arc[0].x = UPSCALE( to->x );
    arc[0].y = UPSCALE( to->y );
    arc[1].x = UPSCALE( control->x );
    arc[1].y = UPSCALE( control->y );
    arc[2].x = raster->x;
    arc[2].y = raster->y;

    while ( top >= 0 )
    {
      level = levels[top];
      if ( level > 1 )
      {
        /* check that the arc crosses the current band */
        TPos  min, max, y;


        min = max = arc[0].y;

        y = arc[1].y;
        if ( y < min ) min = y;
        if ( y > max ) max = y;

        y = arc[2].y;
        if ( y < min ) min = y;
        if ( y > max ) max = y;

        if ( TRUNC( min ) >= raster->max_ey || TRUNC( max ) < raster->min_ey )
          goto Draw;

        gray_split_conic( arc );
        arc += 2;
        top++;
        levels[top] = levels[top - 1] = level - 1;
        continue;
      }

    Draw:
      {
        TPos  to_x, to_y, mid_x, mid_y;


        to_x  = arc[0].x;
        to_y  = arc[0].y;
        mid_x = ( raster->x + to_x + 2 * arc[1].x ) / 4;
        mid_y = ( raster->y + to_y + 2 * arc[1].y ) / 4;

        gray_render_line( raster, mid_x, mid_y );
        gray_render_line( raster, to_x, to_y );

        top--;
        arc -= 2;
      }
    }
    return;
  }


  static void
  gray_split_cubic( EVG_Vector*  base )
  {
    TPos  a, b, c, d;


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


  static void
  gray_render_cubic( TRaster *raster, EVG_Vector*  control1,
                              EVG_Vector*  control2,
                              EVG_Vector*  to )
  {
    TPos        dx, dy, da, db;
    int         top, level;
    int*        levels;
    EVG_Vector*  arc;


    dx = DOWNSCALE( raster->x ) + to->x - ( control1->x << 1 );
    if ( dx < 0 )
      dx = -dx;
    dy = DOWNSCALE( raster->y ) + to->y - ( control1->y << 1 );
    if ( dy < 0 )
      dy = -dy;
    if ( dx < dy )
      dx = dy;
    da = dx;

    dx = DOWNSCALE( raster->x ) + to->x - 3 * ( control1->x + control2->x );
    if ( dx < 0 )
      dx = -dx;
    dy = DOWNSCALE( raster->y ) + to->y - 3 * ( control1->x + control2->y );
    if ( dy < 0 )
      dy = -dy;
    if ( dx < dy )
      dx = dy;
    db = dx;

    level = 1;
    da    = da / raster->cubic_level;
    db    = db / raster->conic_level;
    while ( da > 0 || db > 0 )
    {
      da >>= 2;
      db >>= 3;
      level++;
    }

    if ( level <= 1 )
    {
      TPos   to_x, to_y, mid_x, mid_y;


      to_x  = UPSCALE( to->x );
      to_y  = UPSCALE( to->y );
      mid_x = ( raster->x + to_x +
                3 * UPSCALE( control1->x + control2->x ) ) / 8;
      mid_y = ( raster->y + to_y +
                3 * UPSCALE( control1->y + control2->y ) ) / 8;

      gray_render_line( raster, mid_x, mid_y );
      gray_render_line( raster, to_x, to_y );
      return;
    }

    arc      = raster->bez_stack;
    arc[0].x = UPSCALE( to->x );
    arc[0].y = UPSCALE( to->y );
    arc[1].x = UPSCALE( control2->x );
    arc[1].y = UPSCALE( control2->y );
    arc[2].x = UPSCALE( control1->x );
    arc[2].y = UPSCALE( control1->y );
    arc[3].x = raster->x;
    arc[3].y = raster->y;

    levels    = raster->lev_stack;
    top       = 0;
    levels[0] = level;

    while ( top >= 0 )
    {
      level = levels[top];
      if ( level > 1 )
      {
        /* check that the arc crosses the current band */
        TPos  min, max, y;


        min = max = arc[0].y;
        y = arc[1].y;
        if ( y < min ) min = y;
        if ( y > max ) max = y;
        y = arc[2].y;
        if ( y < min ) min = y;
        if ( y > max ) max = y;
        y = arc[3].y;
        if ( y < min ) min = y;
        if ( y > max ) max = y;
        if ( TRUNC( min ) >= raster->max_ey || TRUNC( max ) < 0 )
          goto Draw;
        gray_split_cubic( arc );
        arc += 3;
        top ++;
        levels[top] = levels[top - 1] = level - 1;
        continue;
      }

    Draw:
      {
        TPos  to_x, to_y, mid_x, mid_y;


        to_x  = arc[0].x;
        to_y  = arc[0].y;
        mid_x = ( raster->x + to_x + 3 * ( arc[1].x + arc[2].x ) ) / 8;
        mid_y = ( raster->y + to_y + 3 * ( arc[1].y + arc[2].y ) ) / 8;

        gray_render_line( raster, mid_x, mid_y );
        gray_render_line( raster, to_x, to_y );
        top --;
        arc -= 3;
      }
    }
    return;
  }


  /* a macro comparing two cell pointers.  Returns true if a <= b. */
#if 1

#define PACK( a )          ( ( (long)(a)->y << 16 ) + (a)->x )
#define LESS_THAN( a, b )  ( PACK( a ) < PACK( b ) )

#else /* 1 */

#define LESS_THAN( a, b )  ( (a)->y < (b)->y || \
                             ( (a)->y == (b)->y && (a)->x < (b)->x ) )

#endif /* 1 */

#define SWAP_CELLS( a, b, temp )  do             \
                                  {              \
                                    temp = *(a); \
                                    *(a) = *(b); \
                                    *(b) = temp; \
                                  } while ( 0 )
#define DEBUG_SORT
#define QUICK_SORT

#ifdef SHELL_SORT

  /* a simple shell sort algorithm that works directly on our */
  /* cells table                                              */
  static void
  gray_shell_sort ( PCell  cells,
                    int    count )
  {
    PCell  i, j, limit = cells + count;
    TCell  temp;
    int    gap;


    /* compute initial gap */
    for ( gap = 0; ++gap < count; gap *= 3 )
      ;

    while ( gap /= 3 )
    {
      for ( i = cells + gap; i < limit; i++ )
      {
        for ( j = i - gap; ; j -= gap )
        {
          PCell  k = j + gap;


          if ( LESS_THAN( j, k ) )
            break;

          SWAP_CELLS( j, k, temp );

          if ( j < cells + gap )
            break;
        }
      }
    }
  }

#endif /* SHELL_SORT */


#ifdef QUICK_SORT

  /* This is a non-recursive quicksort that directly process our cells     */
  /* array.  It should be faster than calling the stdlib qsort(), and we   */
  /* can even tailor our insertion threshold...                            */

#define QSORT_THRESHOLD  9  /* below this size, a sub-array will be sorted */
                            /* through a normal insertion sort             */

  static void
  gray_quick_sort( PCell  cells,
                   int    count )
  {
    PCell   stack[40];  /* should be enough ;-) */
    PCell*  top;        /* top of stack */
    PCell   base, limit;
    TCell   temp;


    limit = cells + count;
    base  = cells;
    top   = stack;

    for (;;)
    {
      int    len = (int)( limit - base );
      PCell  i, j, pivot;


      if ( len > QSORT_THRESHOLD )
      {
        /* we use base + len/2 as the pivot */
        pivot = base + len / 2;
        SWAP_CELLS( base, pivot, temp );

        i = base + 1;
        j = limit - 1;

        /* now ensure that *i <= *base <= *j */
        if ( LESS_THAN( j, i ) )
          SWAP_CELLS( i, j, temp );

        if ( LESS_THAN( base, i ) )
          SWAP_CELLS( base, i, temp );

        if ( LESS_THAN( j, base ) )
          SWAP_CELLS( base, j, temp );

        for (;;)
        {
          do i++; while ( LESS_THAN( i, base ) );
          do j--; while ( LESS_THAN( base, j ) );

          if ( i > j )
            break;

          SWAP_CELLS( i, j, temp );
        }

        SWAP_CELLS( base, j, temp );

        /* now, push the largest sub-array */
        if ( j - base > limit - i )
        {
          top[0] = base;
          top[1] = j;
          base   = i;
        }
        else
        {
          top[0] = i;
          top[1] = limit;
          limit  = j;
        }
        top += 2;
      }
      else
      {
        /* the sub-array is small, perform insertion sort */
        j = base;
        i = j + 1;

        for ( ; i < limit; j = i, i++ )
        {
          for ( ; LESS_THAN( j + 1, j ); j-- )
          {
            SWAP_CELLS( j + 1, j, temp );
            if ( j == base )
              break;
          }
        }
        if ( top > stack )
        {
          top  -= 2;
          base  = top[0];
          limit = top[1];
        }
        else
          break;
      }
    }
  }

#endif /* QUICK_SORT */


#ifdef DEBUG_GRAYS
#ifdef DEBUG_SORT

  static int
  gray_check_sort( PCell  cells,
                   int    count )
  {
    PCell  p, q;


    for ( p = cells + count - 2; p >= cells; p-- )
    {
      q = p + 1;
      if ( !LESS_THAN( p, q ) )
        return 0;
    }
    return 1;
  }

#endif /* DEBUG_SORT */
#endif /* DEBUG_GRAYS */


  static int
  gray_move_to( EVG_Vector*  to,
                EVG_Raster   raster )
  {
    TPos  x, y;


    /* record current cell, if any */
    gray_record_cell(raster);

    /* start to a new position */
    x = UPSCALE( to->x );
    y = UPSCALE( to->y );

    gray_start_cell(raster, TRUNC( x ), TRUNC( y ) );

    raster->x = x;
    raster->y = y;
    return 0;
  }


  static int
  gray_line_to( EVG_Vector*  to,
                EVG_Raster   raster )
  {
    gray_render_line(raster, UPSCALE( to->x ), UPSCALE( to->y ) );
    return 0;
  }


  static int
  gray_conic_to( EVG_Vector*  control,
                 EVG_Vector*  to,
                 EVG_Raster   raster )
  {
    gray_render_conic(raster, control, to);
    return 0;
  }


  static int
  gray_cubic_to( EVG_Vector*  control1,
                 EVG_Vector*  control2,
                 EVG_Vector*  to,
                 EVG_Raster   raster )
  {
    gray_render_cubic(raster, control1, control2, to );
    return 0;
  }


#ifdef DEBUG_GRAYS

#include <stdio.h>

  static void
  gray_dump_cells( TRaster *raster )
  {
    PCell  cell, limit;
    int    y = -1;


    cell  = raster->cells;
    limit = cell + raster->num_cells;

    for ( ; cell < limit; cell++ )
    {
      if ( cell->y != y )
      {
        fprintf( stderr, "\n%2d: ", cell->y );
        y = cell->y;
      }
      fprintf( stderr, "[%d %d %d]",
               cell->x, cell->area, cell->cover );
    }
    fprintf(stderr, "\n" );
  }

#endif /* DEBUG_GRAYS */


  static void
  gray_hline( TRaster *raster, TCoord  x,
                       TCoord  y,
                       TPos    area,
                       int     acount )
  {
    EVG_Span*   span;
    int        count;
    int        coverage;


    /* compute the coverage line's coverage, depending on the    */
    /* outline fill rule                                         */
    /*                                                           */
    /* the coverage percentage is area/(PIXEL_BITS*PIXEL_BITS*2) */
    /*                                                           */
    coverage = (int)( area >> ( PIXEL_BITS * 2 + 1 - 8 ) );
                                                    /* use range 0..256 */
    if ( coverage < 0 )
      coverage = -coverage;

    if ( raster->outline.flags & GF_PATH_FILL_ZERO_NONZERO)
    {
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

    y += (TCoord)raster->min_ey;
    x += (TCoord)raster->min_ex;

    if ( coverage )
    {
      /* see if we can add this span to the current list */
      count = raster->num_gray_spans;
      span  = raster->gray_spans + count - 1;
      if ( count > 0                          &&
           raster->span_y == y                    &&
           (int)span->x + span->len == (int)x &&
           span->coverage == coverage )
      {
        span->len = (unsigned short)( span->len + acount );
        return;
      }

      if ( raster->span_y != y || count >= FT_MAX_GRAY_SPANS )
      {
        if ( raster->render_span && count > 0 )
          raster->render_span( raster->span_y, count, raster->gray_spans,
                           raster->render_span_data );
        /* raster->render_span( span->y, raster->gray_spans, count ); */

#ifdef DEBUG_GRAYS

        if ( raster->span_y >= 0 )
        {
          int  n;


          fprintf( stderr, "y=%3d ", raster->span_y );
          span = raster->gray_spans;
          for ( n = 0; n < count; n++, span++ )
            fprintf( stderr, "[%d..%d]:%02x ",
                     span->x, span->x + span->len - 1, span->coverage );
          fprintf( stderr, "\n" );
        }

#endif /* DEBUG_GRAYS */

        raster->num_gray_spans = 0;
        raster->span_y         = y;

        count = 0;
        span  = raster->gray_spans;
      }
      else
        span++;

      /* add a gray span to the current list */
      span->x        = (short)x;
      span->len      = (unsigned short)acount;
      span->coverage = (unsigned char)coverage;
      raster->num_gray_spans++;
    }
  }


  static void
  gray_sweep( TRaster *raster)
  {
    TCoord  x, y, cover;
    TArea   area;
    PCell   start, cur, limit;

    if ( raster->num_cells == 0 )
      return;

    cur   = raster->cells;
    limit = cur + raster->num_cells;

    cover              = 0;
    raster->span_y         = -1;
    raster->num_gray_spans = 0;

    for (;;)
    {
      start  = cur;
      y      = start->y;
      x      = start->x;

      area   = start->area;
      cover += start->cover;

      /* accumulate all start cells */
      for (;;)
      {
        ++cur;
        if ( cur >= limit || cur->y != start->y || cur->x != start->x )
          break;

        area  += cur->area;
        cover += cur->cover;
      }

      /* if the start cell has a non-null area, we must draw an */
      /* individual gray pixel there                            */
      if ( area && x >= 0 )
      {
        gray_hline( raster, x, y, cover * ( ONE_PIXEL * 2 ) - area, 1 );
        x++;
      }

      if ( x < 0 )
        x = 0;

      if ( cur < limit && start->y == cur->y )
      {
        /* draw a gray span between the start cell and the current one */
        if ( cur->x > x )
          gray_hline( raster, x, y,
                      cover * ( ONE_PIXEL * 2 ), cur->x - x );
      }
      else
      {
        /* draw a gray span until the end of the clipping region */
        if ( cover && x < raster->max_ex - raster->min_ex )
          gray_hline( raster, x, y,
                      cover * ( ONE_PIXEL * 2 ),
                      (int)( raster->max_ex - x - raster->min_ex ) );
        cover = 0;
      }

      if ( cur >= limit )
        break;
    }

    if ( raster->render_span && raster->num_gray_spans > 0 )
      raster->render_span( raster->span_y, raster->num_gray_spans,
                       raster->gray_spans, raster->render_span_data );

#ifdef DEBUG_GRAYS

    {
      int       n;
      EVG_Span*  span;


      fprintf( stderr, "y=%3d ", raster->span_y );
      span = raster->gray_spans;
      for ( n = 0; n < raster->num_gray_spans; n++, span++ )
        fprintf( stderr, "[%d..%d]:%02x ",
                 span->x, span->x + span->len - 1, span->coverage );
      fprintf( stderr, "\n" );
    }

#endif /* DEBUG_GRAYS */

  }


  /*************************************************************************/
  /*                                                                       */
  /*  The following function should only compile in stand_alone mode,      */
  /*  i.e., when building this component without the rest of FreeType.     */
  /*                                                                       */
  /*************************************************************************/

  /*************************************************************************/
  /*                                                                       */
  /* <Function>                                                            */
  /*    FT_Outline_Decompose                                               */
  /*                                                                       */
  /* <Description>                                                         */
  /*    Walks over an outline's structure to decompose it into individual  */
  /*    segments and Bezier arcs.  This function is also able to emit      */
  /*    `move to' and `close to' operations to indicate the start and end  */
  /*    of new contours in the outline.                                    */
  /*                                                                       */
  /* <Input>                                                               */
  /*    outline        :: A pointer to the source target.                  */
  /*                                                                       */
  /*    func_interface :: A table of `emitters', i.e,. function pointers   */
  /*                      called during decomposition to indicate path     */
  /*                      operations.                                      */
  /*                                                                       */
  /*    user           :: A typeless pointer which is passed to each       */
  /*                      emitter during the decomposition.  It can be     */
  /*                      used to store the state during the               */
  /*                      decomposition.                                   */
  /*                                                                       */
  /* <Return>                                                              */
  /*    Error code.  0 means sucess.                                       */
  /*                                                                       */
  static
  int  EVG_Outline_Decompose( EVG_Outline*              outline,
                             const EVG_Outline_Funcs*  func_interface,
                             void*                    user )
  {
#undef SCALED
#if 0
#define SCALED( x )  ( ( (x) << shift ) - delta )
#else
#define SCALED( x )  (x)
#endif

    EVG_Vector   v_last;
    EVG_Vector   v_control;
    EVG_Vector   v_start;

    EVG_Vector*  point;
    EVG_Vector*  limit;
    char*       tags;

    int   n;         /* index of contour in outline     */
    int   first;     /* index of first point in contour */
    int   error;
    char  tag;       /* current point's state           */

#if 0
    int   shift = func_interface->shift;
    TPos  delta = func_interface->delta;
#endif


    first = 0;

    for ( n = 0; n < outline->n_contours; n++ )
    {
      int  last;  /* index of last point in contour */


      last  = outline->contours[n];
      limit = outline->points + last;

      v_start = outline->points[first];
      v_last  = outline->points[last];

      v_start.x = SCALED( v_start.x ); v_start.y = SCALED( v_start.y );
      v_last.x  = SCALED( v_last.x );  v_last.y  = SCALED( v_last.y );

      v_control = v_start;

      point = outline->points + first;
      tags  = outline->tags  + first;
      tag   = tags[0];

      /* A contour cannot start with a cubic control point! */
      if ( tag == GF_PATH_CURVE_CUBIC )
        goto Invalid_Outline;

      /* check first point to determine origin */
      if ( tag == GF_PATH_CURVE_CONIC )
      {
        /* first point is conic control.  Yes, this happens. */
        if ( outline->tags[last] & GF_PATH_CURVE_ON )
        {
          /* start at last point if it is on the curve */
          v_start = v_last;
          limit--;
        }
        else
        {
          /* if both first and last points are conic,         */
          /* start at their middle and record its position    */
          /* for closure                                      */
          v_start.x = ( v_start.x + v_last.x ) / 2;
          v_start.y = ( v_start.y + v_last.y ) / 2;

          v_last = v_start;
        }
        point--;
        tags--;
      }

      error = func_interface->move_to( &v_start, user );
      if ( error )
        goto Exit;

      while ( point < limit )
      {
        point++;
        tags++;

        tag = tags[0];
        switch ( tag )
        {
        case GF_PATH_CURVE_ON:  /* emit a single line_to */
        case GF_PATH_CLOSE:  /* emit a single line_to */
          {
            EVG_Vector  vec;


            vec.x = SCALED( point->x );
            vec.y = SCALED( point->y );

            error = func_interface->line_to( &vec, user );
            if ( error )
              goto Exit;
            continue;
          }

        case GF_PATH_CURVE_CONIC:  /* consume conic arcs */
          {
            v_control.x = SCALED( point->x );
            v_control.y = SCALED( point->y );

          Do_Conic:
            if ( point < limit )
            {
              EVG_Vector  vec;
              EVG_Vector  v_middle;


              point++;
              tags++;
              tag = tags[0];

              vec.x = SCALED( point->x );
              vec.y = SCALED( point->y );

              if ( tag & GF_PATH_CURVE_ON )
              {
                error = func_interface->conic_to( &v_control, &vec, user );
                if ( error )
                  goto Exit;
                continue;
              }

              if ( tag != GF_PATH_CURVE_CONIC )
                goto Invalid_Outline;

              v_middle.x = ( v_control.x + vec.x ) / 2;
              v_middle.y = ( v_control.y + vec.y ) / 2;

              error = func_interface->conic_to( &v_control, &v_middle, user );
              if ( error )
                goto Exit;

              v_control = vec;
              goto Do_Conic;
            }

            error = func_interface->conic_to( &v_control, &v_start, user );
            goto Close;
          }

        default:  /* GF_PATH_CURVE_CUBIC */
          {
            EVG_Vector  vec1, vec2;


            if ( point + 1 > limit                             ||
                 tags[1] != GF_PATH_CURVE_CUBIC )
              goto Invalid_Outline;

            point += 2;
            tags  += 2;

            vec1.x = SCALED( point[-2].x ); vec1.y = SCALED( point[-2].y );
            vec2.x = SCALED( point[-1].x ); vec2.y = SCALED( point[-1].y );

            if ( point <= limit )
            {
              EVG_Vector  vec;


              vec.x = SCALED( point->x );
              vec.y = SCALED( point->y );

              error = func_interface->cubic_to( &vec1, &vec2, &vec, user );
              if ( error )
                goto Exit;
              continue;
            }

            error = func_interface->cubic_to( &vec1, &vec2, &v_start, user );
            goto Close;
          }
        }
      }

      /* close the contour with a line segment */
      error = func_interface->line_to( &v_start, user );

   Close:
      if ( error )
        goto Exit;

      first = last + 1;
    }

    return 0;

  Exit:
    return error;

  Invalid_Outline:
    return ErrRaster_Invalid_Outline;
  }

  typedef struct  TBand_
  {
    TPos  min, max;

  } TBand;


  static int
  gray_convert_glyph_inner( TRaster *raster )
  {
    static
    const EVG_Outline_Funcs  func_interface =
    {
      (EVG_Outline_MoveTo_Func) gray_move_to,
      (EVG_Outline_LineTo_Func) gray_line_to,
      (EVG_Outline_ConicTo_Func)gray_conic_to,
      (EVG_Outline_CubicTo_Func)gray_cubic_to,
      0,
      0
    };

    volatile int  error = 0;

    if ( ft_setjmp( raster->jump_buffer ) == 0 )
    {
      error = EVG_Outline_Decompose( &raster->outline, &func_interface, &(*raster) );
      gray_record_cell( raster );
    }
    else
    {
      error = ErrRaster_MemoryOverflow;
    }

    return error;
  }


static int gray_convert_glyph( TRaster *raster ) 
{
	int level;
	TBand            bands[40];
	volatile TBand*  band;
	volatile int     n, num_bands;
	volatile TPos    min, max, max_y;
	EVG_BBox*         clip;

	/* Set up state in the raster object */
	gray_compute_cbox( raster );

	/* clip to target bitmap, exit if nothing to do */
	clip = &raster->clip_box;

	if ( raster->max_ex <= clip->xMin || raster->min_ex >= clip->xMax || raster->max_ey <= clip->yMin || raster->min_ey >= clip->yMax )
		return 0;

	if ( raster->min_ex < clip->xMin ) raster->min_ex = clip->xMin;
	if ( raster->min_ey < clip->yMin ) raster->min_ey = clip->yMin;

	if ( raster->max_ex > clip->xMax ) raster->max_ex = clip->xMax;
	if ( raster->max_ey > clip->yMax ) raster->max_ey = clip->yMax;

	/* simple heuristic used to speed-up the bezier decomposition -- see */
	/* the code in gray_render_conic() and gray_render_cubic() for more  */
	/* details                                                           */
	raster->conic_level = 32;
	raster->cubic_level = 16;

	level = 0;
	if ( raster->max_ex > 24 || raster->max_ey > 24 )
		level++;
	if ( raster->max_ex > 120 || raster->max_ey > 120 )
		level++;

	raster->conic_level <<= level;
	raster->cubic_level <<= level;
	
	/* setup vertical bands */
	num_bands = (int)( ( raster->max_ey - raster->min_ey ) / raster->band_size );
	if ( num_bands == 0 )  num_bands = 1;
	if ( num_bands >= 39 ) num_bands = 39;
	raster->band_shoot = 0;

	min   = raster->min_ey;
	max_y = raster->max_ey;

	for ( n = 0; n < num_bands; n++, min = max ) {
		max = min + raster->band_size;
		if ( n == num_bands - 1 || max > max_y )
			max = max_y;
		
		bands[0].min = min;
		bands[0].max = max;
		band         = bands;

		while ( band >= bands ) {
			TPos  bottom, top, middle;
			int   error;

			raster->num_cells = 0;
			raster->invalid   = 1;
			raster->min_ey    = band->min;
			raster->max_ey    = band->max;

			error = gray_convert_glyph_inner( raster );
			
			if ( !error ) {
#ifdef SHELL_SORT
				gray_shell_sort( raster->cells, raster->num_cells );
#else
				gray_quick_sort( raster->cells, raster->num_cells );
#endif

#ifdef DEBUG_GRAYS
				gray_check_sort( raster->cells, raster->num_cells );
				gray_dump_cells( raster );
#endif

				gray_sweep( raster);
				band--;
				continue;
			}
			else if ( error != ErrRaster_MemoryOverflow )
				return 1;
			
			/* render pool overflow, we will reduce the render band by half */
			bottom = band->min;
			top    = band->max;
			middle = bottom + ( ( top - bottom ) >> 1 );

			/* waoow! This is too complex for a single scanline, something */
			/* must be really rotten here!                                 */
			if ( middle == bottom ) {
#ifdef DEBUG_GRAYS
				fprintf( stderr, "Rotten glyph!\n" );
#endif
				return 1;
			}

			if ( bottom-top >= raster->band_size )
				raster->band_shoot++;
			
			band[1].min = bottom;
			band[1].max = middle;
			band[0].min = middle;
			band[0].max = top;
			band++;
		}
	}

	if ( raster->band_shoot > 8 && raster->band_size > 16 )
		raster->band_size = raster->band_size / 2;

	return 0;
}


int evg_raster_render(EVG_Raster raster, EVG_Raster_Params*  params)
{
	EVG_Outline*  outline = (EVG_Outline*)params->source;
	if ( !raster || !raster->cells || !raster->max_cells )
		return -1;

	/* return immediately if the outline is empty */
	if ( outline->n_points == 0 || outline->n_contours <= 0 )
		return 0;
	if ( !outline || !outline->contours || !outline->points )
		return ErrRaster_Invalid_Outline;
	if ( outline->n_points != outline->contours[outline->n_contours - 1] + 1 )
		return ErrRaster_Invalid_Outline;

	raster->clip_box = params->clip_box;
	raster->outline   = *outline;
	raster->num_cells = 0;
	raster->invalid   = 1;
	raster->render_span      = (EVG_Raster_Span_Func)params->gray_spans;
	raster->render_span_data = params->user;
	return gray_convert_glyph(raster);
}

int evg_raster_new( void*       memory, EVG_Raster*  araster )
{
	TRaster *raster;
	*araster = 0;
	raster = malloc(sizeof(TRaster));
	if (!raster) return 1;
	memset(raster, 0, sizeof(TRaster));
	raster->memory = memory;
	*araster = (EVG_Raster) raster;
    return 0;
}

void evg_raster_done(EVG_Raster raster)
{
    free(raster);
}

void evg_raster_reset(EVG_Raster raster, const char*  pool_base, long pool_size)
{
    if (raster && pool_base && pool_size >= 4096 )
		gray_init_cells(raster, (char*)pool_base, pool_size );
	raster->band_size  = (int)( ( pool_size / sizeof ( TCell ) ) / 8 );
}


/* END */
