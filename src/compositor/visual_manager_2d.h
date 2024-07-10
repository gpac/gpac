/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
 *					All rights reserved
 *
 *  This file is part of GPAC / Scene Compositor sub-project
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

#ifndef _VISUAL_MANAGER_2D_
#define _VISUAL_MANAGER_2D_

Bool gf_irect_overlaps(GF_IRect *rc1, GF_IRect *rc2);
/*intersects @rc1 with @rc2 - the new @rc1 is the intersection*/
void gf_irect_intersect(GF_IRect *rc1, GF_IRect *rc2);
GF_Rect gf_rect_ft(GF_IRect *rc);
/*@rc2 fully contained in @rc1*/
Bool gf_irect_inside(GF_IRect *rc1, GF_IRect *rc2);

/*@rc1 equales @rc2*/
#define irect_rect_equal(rc1, rc2) ((rc1.width == rc2.width) && (rc1.height == rc2.height) && (rc1.x == rc2.x)  && (rc1.y == rc2.y))

//#define TRACK_OPAQUE_REGIONS

/*ra_: rectangle array macros to speed dirty rects*/
#define RA_DEFAULT_STEP	10

typedef struct
{
	GF_IRect rect;
#ifdef TRACK_OPAQUE_REGIONS
	/*list of nodes covering (no transparency) each rect, or 0 otherwise.*/
	u32 opaque_node_index;
#endif
} GF_RectArrayEntry;

typedef struct
{
	GF_RectArrayEntry *list;
	u32 count, alloc;
} GF_RectArray;

#define ra_init(ra) { (ra)->count = 0; (ra)->alloc = RA_DEFAULT_STEP; (ra)->list = (GF_RectArrayEntry*)gf_malloc(sizeof(GF_RectArrayEntry)*(ra)->alloc); }
/*deletes structure - called as a destructor*/
#define ra_del(ra) { if ((ra)->list) { gf_free((ra)->list); (ra)->list = NULL; } }


/*adds rect to list - expand if needed*/
#define ra_add(ra, rc) {	\
	gf_assert((rc)->width); 	\
	if ((ra)->count==(ra)->alloc) { \
		(ra)->alloc += RA_DEFAULT_STEP; \
		(ra)->list = (GF_RectArrayEntry*)gf_realloc((ra)->list, sizeof(GF_RectArrayEntry) * (ra)->alloc); \
	}	\
	(ra)->list[(ra)->count].rect = *rc; (ra)->count++;	}


/*adds rectangle to the list performing union test*/
void ra_union_rect(GF_RectArray *ra, GF_IRect *rc);
/*refreshes the content of the array to have only non-overlapping rects*/
void ra_refresh(GF_RectArray *ra);

struct _drawable_store
{
	struct _drawable *drawable;
	struct _drawable_store *next;
};

/*
 *	Visual 2D functions
 */
Bool visual_2d_draw_frame(GF_VisualManager *visual, GF_Node *root, GF_TraverseState *tr_state, Bool is_root_visual);

void visual_2d_pick_node(GF_VisualManager *visual, GF_TraverseState *tr_state, GF_Event *ev, GF_ChildNodeItem *children);

void visual_2d_clear_surface(GF_VisualManager *visual, GF_IRect *rc, u32 BackColor, u32 is_offscreen);

/*gets a drawable context on this visual*/
DrawableContext *visual_2d_get_drawable_context(GF_VisualManager *visual);
/*remove last drawable context*/
void visual_2d_remove_last_context(GF_VisualManager *visual);
/*signal the given drawable is being deleted*/
void visual_2d_drawable_delete(GF_VisualManager *visual, Drawable *node);

/*inits raster surface handler for subsequent draw*/
GF_Err visual_2d_init_raster(GF_VisualManager *visual);
/*releases raster surface handler */
void visual_2d_release_raster(GF_VisualManager *visual);

/*texture the path with the given context info*/
void visual_2d_texture_path(GF_VisualManager *visual, GF_Path *path, DrawableContext *ctx, GF_TraverseState *tr_state);
/*draw the path (fill and strike) - if brushes are NULL they are created if needed based on the context aspect
DrawPath shall always be called after TexturePath*/
void visual_2d_draw_path(GF_VisualManager *visual, GF_Path *path, DrawableContext *ctx, GF_EVGStencil * brush, GF_EVGStencil * pen, GF_TraverseState *tr_state);
/*special texturing extension for text, using a given path (text rectangle) and texture*/
void visual_2d_texture_path_text(GF_VisualManager *visual, DrawableContext *txt_ctx, GF_Path *path, GF_Rect *object_bounds, GF_TextureHandler *txh, GF_TraverseState *tr_state);
/*fill given rect with given color with given ctx transform and clipper (used for text highlighting only)
if rc is NULL, fills object bounds*/
void visual_2d_fill_rect(GF_VisualManager *visual, DrawableContext *ctx, GF_Rect *rc, u32 color, u32 strike_color, GF_TraverseState *tr_state);


/*extended version of above function to override texture transforms - needed for proper texturing of glyphs*/
void visual_2d_texture_path_extended(GF_VisualManager *visual, GF_Path *path, GF_TextureHandler *txh, struct _drawable_context *ctx, GF_Rect *orig_bounds, GF_Matrix2D *ext_mx, GF_TraverseState *tr_state);
void visual_2d_draw_path_extended(GF_VisualManager *visual, GF_Path *path, DrawableContext *ctx, GF_EVGStencil * brush, GF_EVGStencil * pen, GF_TraverseState *tr_state, GF_Rect *orig_bounds, GF_Matrix2D *ext_mx, Bool is_erase);


/*video overlay context*/
typedef struct _video_overlay
{
	struct _video_overlay *next;
	GF_Window src, dst;
	DrawableContext *ctx;
	GF_RectArray ra;
} GF_OverlayStack;

/*check if the object is over an overlay. If so, adds its cliper to the list of rectangles to redraw for the overlay
and returns 1 (in which case it shouldn't be drawn)*/
Bool visual_2d_overlaps_overlay(GF_VisualManager *visual, DrawableContext *ctx, GF_TraverseState *tr_state);
/*draw all partial overlays in software and all overlaping objects*/
void visual_2d_flush_overlay_areas(GF_VisualManager *visual, GF_TraverseState *tr_state);
/*finally blit the overlays - MUST be called once the main visual has been flushed*/
void visual_2d_draw_overlays(GF_VisualManager *visual);


#ifndef GPAC_DISABLE_3D
void visual_2d_flush_hybgl_canvas(GF_VisualManager *visual, GF_TextureHandler *txh, struct _drawable_context *ctx, GF_TraverseState *tr_state);
#endif

#endif	/*_VISUAL_MANAGER_2D_*/

