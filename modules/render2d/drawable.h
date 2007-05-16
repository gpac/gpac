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


#ifndef DRAWABLE_H
#define DRAWABLE_H

#include "render2d.h"
#include <gpac/nodes_mpeg4.h>


typedef struct _bound_info
{
	/*cliped bounds in pixels - needed to track static objects with animated cliping*/
	GF_IRect clip;
	/*uncliped bounds - needed to track moving objects fully contained in surface and for image bliting*/
	GF_Rect unclip;
	/* extra_check: 
		for MPEG-4: pointer to appearance node (due to DEF/USE) in order to detect same bounds and appearance node change
		for SVG: currently not used, should be needed for <use>
	*/
	void *extra_check;

	/**/
	struct _bound_info *next;
} BoundInfo;

typedef struct _dirty_rect_info
{
	/*the surface for which we're storing the bounds of this node*/
	struct _visual_surface_2D *surface;
	/*the current location of the node on the surface collected during traverse step*/
	struct _bound_info *current_bounds;
	/*the location of the node on the surface at the previous frame*/
	struct _bound_info *previous_bounds;

	/**/
	struct _dirty_rect_info *next;
} DRInfo;

enum {
	/*flag set if node has been drawn for the current surface*/
	DRAWABLE_DRAWN_ON_SURFACE = 1,
	/*flag set when geometry's node has been modified (eg, skips bounds checking)*/
	DRAWABLE_HAS_CHANGED = 1<<1,
	/*set if node is registered in previous node drawn list of the current surface
	the flag is only set during a surface render*/
	DRAWABLE_REGISTERED_WITH_SURFACE = 1<<2,
};

typedef struct _drawable
{
	/*set of above flags*/
	u32 flags;
	/*node owning the drawable*/
	GF_Node *node;

	/*graphic path - !! CREATED BY DEFAULT !! */
	GF_Path *path;

	/*bounds collector*/
	struct _dirty_rect_info *dri;

	/*cached outlines*/
	struct _strikeinfo2d *outline;
} Drawable;

/*construction destruction*/
Drawable *drawable_new();
void drawable_del(Drawable *);
void drawable_del_ex(Drawable *dr, Render2D *r2d);

void DestroyDrawableNode(GF_Node *node);

/*move current bounds to previous bounds for given target surface - called before updating a surface
returns 1 if nod was draw last frame on this surface, 0 otherwise*/
Bool drawable_flush_bounds(Drawable *node, struct _visual_surface_2D *on_surface, u32 render_mode);

/*
	return 1 if same bound is found in previous list (and remove it from the list)
	return 0 otherwise
*/
Bool drawable_has_same_bounds(struct _drawable_context *ctx, struct _visual_surface_2D *surf);

/*
	return any previous bounds related to the same surface in @rc if any
	if nothing found return 0
*/
Bool drawable_get_previous_bound(Drawable *node, GF_IRect *rc, struct _visual_surface_2D *surf);


/*
decide whether drawing is needed or not based on rendering settings and parent node - must be called
at the end of each render of drawable nodes
if orig_bounds is NULL, function uses the bounds of the drawable's path
*/
void drawable_finalize_render(struct _drawable_context *ctx, RenderEffect2D *effects, GF_Rect *orig_bounds);

/*performs final task common to drawable_finalize_render and layout node rendering (cf grouping.c)*/
void drawable_finalize_end(struct _drawable_context *ctx, RenderEffect2D *eff);

/*base constructor for geometry objects that work without overloading the drawable stuff*/
Drawable *drawable_stack_new(Render2D *sr, GF_Node *node);
/*reset all paths (main path and any outline) of the stack*/
void drawable_reset_path(Drawable *st);
/*reset all paths outlines (only) of the stack*/
void drawable_reset_path_outline(Drawable *st);

/*reset bounds array (current and previous) on the given surface*/
void drawable_reset_bounds(Drawable *dr, struct _visual_surface_2D *surf);
/*setup clip/uncli pointers for the drawable context*/
void drawable_check_bounds(struct _drawable_context *ctx, struct _visual_surface_2D *surf);

typedef struct
{
	/*including alpha*/
	GF_Color fill_color, line_color;
	Fixed line_scale;
	GF_PenSettings pen_props;
	/*texture fill handler*/
	struct _gf_sr_texture_handler *line_texture;
} DrawAspect2D;

enum
{
	/*set whenever appearance changed*/
	CTX_APP_DIRTY = 1,
	/*set whenever texture data changed*/
	CTX_TEXTURE_DIRTY = 1<<1,

	/*set when context is an MPEG-4/VRML node*/
	CTX_HAS_APPEARANCE = 1<<2,
	/*set if node completely fits its bounds (flat rect and bitmap) then non transparent*/
	CTX_IS_TRANSPARENT = 1<<3,
	/*set if node is text data*/
	CTX_IS_TEXT = 1<<4,
	/*set if node is background*/
	CTX_IS_BACKGROUND = 1<<5,
	/*turn antialiasing off when drawing*/
	CTX_NO_ANTIALIAS = 1<<6,
	/*indicates path has been textured, in which case FILL is skiped*/
	CTX_PATH_FILLED = 1<<7,
	/*indicates path outline has been textured, in which case STRIKE is skiped*/
	CTX_PATH_STROKE = 1<<8,
	/*indicates SVG path outline geometry has been modified*/
	CTX_SVG_OUTLINE_GEOMETRY_DIRTY = 1<<9,

	CTX_SVG_PICK_PATH = 1<<10,
	CTX_SVG_PICK_OUTLINE = 1<<11,
	CTX_SVG_PICK_BOUNDS = 1<<12,
};

#define CTX_REDRAW_MASK	0x00000003

typedef struct _drawable_context
{
	/*next context allocated, or NULL*/
	struct _drawable_context *next;

	/*any of the above flags*/
	u16 flags;
	/*only used by text when splitting strings into chars / substrings*/
	s16 sub_path_index;

	/*drawable using this context*/
	Drawable *drawable;

	/*pointer to clipped and uncliped (for sensors) rect in pixels.*/
	BoundInfo *bi;

	/*draw info*/
	DrawAspect2D aspect;
	/*transform matrix from top*/
	GF_Matrix2D transform;
	/*color matrix, NULL if none/identity*/
	GF_ColorMatrix *col_mat;
	/*sensors attached to this context*/
	struct __sensor_ctx *sensor;

	/* extra check data for bounds matching
		for MPEG-4: appearance node
		for SVG: parent <use> node if any
	*/
	GF_Node *appear;

	/*texture fill handler*/
	struct _gf_sr_texture_handler *h_texture;
} DrawableContext;	

DrawableContext *NewDrawableContext();
void DeleteDrawableContext(DrawableContext *);
void drawctx_reset(DrawableContext *ctx);
void drawctx_update_info(DrawableContext *ctx, struct _visual_surface_2D *surf);
void drawctx_reset_sensors(DrawableContext *ctx);

/*inits context - may return NULL if the node doesn't have to be drawn*/
DrawableContext *drawable_init_context(Drawable *node, RenderEffect2D *effects);

/*base draw function (texturing, fill and strike)*/
void drawable_draw(RenderEffect2D *effects);
/*base draw function (texturing, fill and strike)*/
void drawable_pick(RenderEffect2D *effects);

/*stored at compositor level and in each drawable node*/
typedef struct _strikeinfo2d
{
	struct _strikeinfo2d *next;
	GF_Path *outline;
	GF_Node *lineProps;
	GF_Node *node;
	u32 last_update_time;
	/*we also MUST handle width changes due to scale transforms, most content is designed with width=cst 
	whatever the transformation*/
	Fixed line_scale;
	/*SVG path length*/
	Fixed path_length;
	/*set only for text*/
	GF_Path *original;
} StrikeInfo2D;

void delete_strikeinfo2d(StrikeInfo2D *info);
/*get strike and manage any scale change&co. This avoids recomputing outline at each frame...*/
StrikeInfo2D *drawctx_get_strikeinfo(Render2D *r2d, DrawableContext *ctx, GF_Path *txt_path);

void drawable_render_focus(GF_Node *node, void *rs, Bool is_destroy);
void drawable_check_focus_highlight(GF_Node *node, RenderEffect2D *eff, GF_Rect *orig_bounds);

#endif
