/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / 2D rendering module
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

#ifndef VISUALSURFACE2D_H_
#define VISUALSURFACE2D_H_

#include "render2d.h"
#include "drawable.h"

/*sensors info*/
typedef struct 
{
	struct _drawable_context *ctx;
	GF_List *nodes_on_top;
} SensorInfo;


static GFINLINE Bool gf_irect_overlaps(GF_IRect rc1, GF_IRect rc2)
{
	if (! rc2.height || !rc2.width || !rc1.height || !rc1.width) return 0;
	if (rc2.x+rc2.width<=rc1.x) return 0;
	if (rc2.x>=rc1.x+rc1.width) return 0;
	if (rc2.y-rc2.height>=rc1.y) return 0;
	if (rc2.y<=rc1.y-rc1.height) return 0;
	return 1;
}

/*adds @rc2 to @rc1 - the new @rc1 contains the old @rc1 and @rc2*/
static GFINLINE void gf_irect_union(GF_IRect *rc1, GF_IRect *rc2) 
{
	if (!rc1->width || !rc1->height) {*rc1=*rc2; return;}

	if (rc2->x < rc1->x) {
		rc1->width += rc1->x - rc2->x;
		rc1->x = rc2->x;
	}
	if (rc2->x + rc2->width > rc1->x+rc1->width) rc1->width = rc2->x + rc2->width - rc1->x;
	if (rc2->y > rc1->y) {
		rc1->height += rc2->y - rc1->y;
		rc1->y = rc2->y;
	}
	if (rc2->y - rc2->height < rc1->y - rc1->height) rc1->height = rc1->y - rc2->y + rc2->height;
}

/*@rc1 equales @rc2*/
static GFINLINE Bool gf_irect_equal(GF_IRect rc1, GF_IRect rc2) 
{ 
	if ( (rc1.x == rc2.x)  && (rc1.y == rc2.y) && (rc1.width == rc2.width) && (rc1.height == rc2.height) )
		return 1;
	return 0;
}

/*makes @rc empty*/
#define gf_rect_reset(rc)  { (rc)->x = (rc)->y = (rc)->width = (rc)->height = 0; }

/*is @rc empty*/
#define gf_rect_is_empty(rc) ( ((rc).width && (rc).height) ? 0 : 1 )

/*intersects @rc1 with @rc2 - the new @rc1 is the intersection*/
static GFINLINE void gf_irect_intersect(GF_IRect *rc1, GF_IRect *rc2)
{
	if (! gf_irect_overlaps(*rc1, *rc2)) {
		gf_rect_reset(rc1); 
		return;
	}
	if (rc2->x > rc1->x) {
		rc1->width -= rc2->x - rc1->x;
		rc1->x = rc2->x;
	} 
	if (rc2->x + rc2->width < rc1->x + rc1->width) {
		rc1->width = rc2->width + rc2->x - rc1->x;
	} 
	if (rc2->y < rc1->y) {
		rc1->height -= rc1->y - rc2->y; 
		rc1->y = rc2->y;
	} 
	if (rc2->y - rc2->height > rc1->y - rc1->height) {
		rc1->height = rc1->y - rc2->y + rc2->height;
	} 
}

/*@rc2 fully contained in @rc1*/
static GFINLINE Bool gf_irect_inside(GF_IRect rc1, GF_IRect rc2) 
{
	if (!rc1.width || !rc1.height) return 0;
	if ( (rc1.x <= rc2.x)  && (rc1.y >= rc2.y)  && (rc1.x + rc1.width >= rc2.x + rc2.width) && (rc1.y - rc1.height <= rc2.y - rc2.height) )
		return 1;
	return 0;
}

GF_IRect gf_rect_pixelize(GF_Rect *r);

static GFINLINE GF_Rect gf_rect_ft(GF_IRect *rc)
{
	GF_Rect rcft;
	rcft.x = INT2FIX(rc->x); rcft.y = INT2FIX(rc->y); rcft.width = INT2FIX(rc->width); rcft.height = INT2FIX(rc->height); 
	return rcft;
}

/*@x, y in @rc*/
static GFINLINE Bool gf_point_in_rect(GF_IRect _rc, Fixed x, Fixed y) 
{
	GF_Rect rc = gf_rect_ft(&_rc);
	if ( (x >= rc.x) && (y <= rc.y) && (x <= rc.x + rc.width) && (y >= rc.y - rc.height) )
		return 1;
	return 0;
}

/*ra_: rectangle array macros to speed dirty rects*/
#define RA_DEFAULT_STEP	50

typedef struct
{	
	GF_IRect *list;
	u32 count, alloc;
	/*list of nodes covering (no transparency) each rect, or 0 otherwise.*/
	u32 *opaque_node_index;
} GF_RectArray;

/*inits structure - called as a constructor*/
#define ra_init(ra) { (ra)->count = 0; (ra)->alloc = 1; (ra)->list = (GF_IRect*)malloc(sizeof(GF_IRect)); (ra)->opaque_node_index = NULL;}
/*deletes structure - called as a destructor*/
#define ra_del(ra) { free((ra)->list); if ((ra)->opaque_node_index) free((ra)->opaque_node_index); }
/*adds rect to list - expand if needed*/
#define ra_add(ra, rc) {	\
	if ((ra)->count==(ra)->alloc) { (ra)->alloc += RA_DEFAULT_STEP; (ra)->list = (GF_IRect*)realloc((ra)->list, sizeof(GF_IRect) * (ra)->alloc); }	\
	(ra)->list[(ra)->count] = rc; (ra)->count++;	}
/*clears list*/
#define ra_clear(ra) { (ra)->count = 0; }
/*is list empty*/
#define ra_is_empty(ra) (!((ra)->count))


/*adds rectangle to the list performing union test*/
static GFINLINE void ra_union_rect(GF_RectArray *ra, GF_IRect rc) 
{
	u32 i;

	for (i=0; i<ra->count; i++) { 
		if (gf_irect_overlaps(ra->list[i], rc)) { 
			gf_irect_union(&ra->list[i], &rc); 
			return; 
		} 
	}
	ra_add(ra, rc); 
}

/*merges all rects in ra2 into ra1*/
static GFINLINE void ra_merge(GF_RectArray *ra1, GF_RectArray *ra2)  
{
	u32 i;
	for (i=0; i<ra2->count; i++) {
		ra_union_rect(ra1, ra2->list[i]);
	}
}

/*refreshes the content of the array to have only non-overlapping rects*/
static GFINLINE void ra_refresh(GF_RectArray *ra)
{
	u32 i, j, k;
	for (i=0; i<ra->count; i++) {
		for (j=i+1; j<ra->count; j++) {
			if (gf_irect_overlaps(ra->list[i], ra->list[j])) {
				gf_irect_union(&ra->list[i], &ra->list[j]);

				/*remove rect*/
				k = ra->count - j - 1;
				if (k) memmove(&ra->list[j], & ra->list[j+1], sizeof(GF_IRect)*k);
				ra->count--; 

				ra_refresh(ra);
				return;
			}
		}
	}
}


typedef struct _visual_surface_2D 
{
	Render2D *render;
	
	/*the one and only dirty rect collector*/
	GF_RectArray to_redraw;
	u32 draw_node_index;

	/*static list of context*/
	struct _drawable_context **contexts;
	u32 num_contexts, alloc_contexts;

	/*background and viewport stacks*/
	GF_List *back_stack;
	GF_List *view_stack;

	/*top-level transform (active viewport, user zoom and pan) */
	GF_Matrix2D top_transform;
	/*pixel area of surface in BIFS coords - eg surface to fill with background*/
	GF_IRect surf_rect;
	/*top clipper (may be different than surf_rect when a viewport is active)*/
	GF_IRect top_clipper;

	/*keeps track of nodes drawn last frame*/
	GF_List *prev_nodes_drawn;
	/*currently active sensors*/
	GF_List *sensors;	
	Bool last_had_back;

	/*signals that the surface is attached to buffer/device/stencil*/
	Bool is_attached;

	/*size in pixels*/
	u32 width, height;
	Bool center_coords;

	/*this is set by the video renderer or the composite texture object */

	/*gets access to graphics handle*/
	GF_Err (*GetSurfaceAccess)(struct _visual_surface_2D *);
	/*release graphics handle*/
	void (*ReleaseSurfaceAccess)(struct _visual_surface_2D *);

	/*draws specified texture as flat bitmap*/
	void (*DrawBitmap)(struct _visual_surface_2D *, struct _gf_sr_texture_handler *, GF_IRect *clip, GF_Rect *unclip, u8 alpha, u32 *col_key, GF_ColorMatrix *cmat);
	Bool (*SupportsFormat)(struct _visual_surface_2D *surf, u32 pixel_format);

	/*composite texture renderer if any*/
	struct _composite_2D *composite;
	
	GF_SURFACE the_surface;
	GF_STENCIL the_brush;
	GF_STENCIL the_pen;
} VisualSurface2D;
/*constructor/destructor*/
VisualSurface2D *NewVisualSurface2D();
void DeleteVisualSurface2D(VisualSurface2D *);
void VS2D_ResetSensors(VisualSurface2D *surf);
/*gets a drawable context on this surface*/
struct _drawable_context *VS2D_GetDrawableContext(VisualSurface2D *surf);
/*remove last drawable context*/
void VS2D_RemoveLastContext(VisualSurface2D *surf);
/*signal the given drawable is being deleted*/
void VS2D_DrawableDeleted(VisualSurface2D *surf, Drawable *node);

/*inits rendering cycle - called at each cycle start regardless of rendering mode*/
void VS2D_InitDraw(VisualSurface2D *surf, RenderEffect2D *eff);
/*terminates rendering cycle - called at each cycle end regardless of rendering mode
if rendering is indirect, actual drawing is performed here. Returns 1 if the surface has been modified*/
Bool VS2D_TerminateDraw(VisualSurface2D *surf, RenderEffect2D *eff);
/*register context sensors for later picking*/
void VS2D_RegisterSensor(VisualSurface2D *surf, DrawableContext *ctx);


/*locates drawable context under the given point for VRML-based scene (that is for nodes with sensors attached)
Note: this also locate context in composite textures
*/
DrawableContext *VS2D_PickSensitiveNode(VisualSurface2D *surf, Fixed X, Fixed Y);
/*same as above but doesn't check for any sensor & co*/
DrawableContext *VS2D_PickContext(VisualSurface2D *surf, Fixed x, Fixed y);

/*clear given rect or all surface if no rect specified - clear color depends on surface type - 0 for composite
surfaces, renderer clear color otherwise
BackColor is non 0 for background node only*/
void VS2D_Clear(VisualSurface2D *, GF_IRect *clear, u32 BackColor);
/*texture the path with the given context info*/
void VS2D_TexturePath(VisualSurface2D *, GF_Path *path, DrawableContext *ctx);
/*draw the path (fill and strike) - if brushes are NULL they are created if needed based on the context aspect
DrawPath shall always be called after TexturePath*/
void VS2D_DrawPath(VisualSurface2D *, GF_Path *path, DrawableContext *ctx, GF_STENCIL brush, GF_STENCIL pen);
/*special texturing extension for text, using a given path (text rectangle) and texture (rendered text)*/
void VS2D_TexturePathText(VisualSurface2D *surf, DrawableContext *txt_ctx, GF_Path *path, GF_Rect *object_bounds, GF_HWTEXTURE hwtx, GF_Rect *gf_sr_texture_bounds);

/*inits graphics surface handler for subsequent draw*/
GF_Err VS2D_InitSurface(VisualSurface2D *surf);
/*releases graphics surface handler */
void VS2D_TerminateSurface(VisualSurface2D *surf);
/*destroys graphics driver objects used by the surface*/
void VS2D_ResetGraphics(VisualSurface2D *surf);

/* this is to use carefully: picks a node based on the PREVIOUS frame state (no traversing)*/
GF_Node *VS2D_PickNode(VisualSurface2D *surf, Fixed x, Fixed y);

/*fill given rect with given color with given ctx transform and clipper (used for text hilighting only)*/
void VS2D_FillRect(VisualSurface2D *surf, DrawableContext *ctx, GF_Rect rc, u32 color);

#endif

