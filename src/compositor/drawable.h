/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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


#ifndef DRAWABLE_H
#define DRAWABLE_H

#include <gpac/internal/compositor_dev.h>


typedef struct _drawable Drawable;
typedef struct _drawable_context DrawableContext;

typedef struct _bound_info
{
	/*cliped bounds in pixels - needed to track static objects with animated cliping*/
	GF_IRect clip;
	/*uncliped bounds - needed to track moving objects fully contained in visual and for image bliting*/
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
	/*the visual manager for which we're storing the bounds of this node*/
	GF_VisualManager *visual;
	/*the current location of the node on the visual manager collected during traverse step*/
	struct _bound_info *current_bounds;
	/*the location of the node on the visual manager at the previous frame*/
	struct _bound_info *previous_bounds;

	/**/
	struct _dirty_rect_info *next;
} DRInfo;

enum {
	/*user defined flags*/

	/*if flag set, the node callback function will be used to draw the node. Otherwise,
	high-level draw operations on the drawable & drawable context will be used*/
	DRAWABLE_USE_TRAVERSE_DRAW = 1,

	/*bounds tracker flags, INTERNAL TO RENDERER */

	/*flag set by drawable_mark_modified during a TRAVERSE_SORT pass when geometry's node has been modified. This forces clearing of the node
	and skips bounds checking.
	Flag is cleared by the compositor*/
	DRAWABLE_HAS_CHANGED = 1<<1,
	/*same flag as above except set when picking/getting bounds out of the main scene traversal routine (user event, script)*/
	DRAWABLE_HAS_CHANGED_IN_LAST_TRAVERSE = 1<<2,

	/*flag set if node has been drawn for the current visual manager*/
	DRAWABLE_DRAWN_ON_VISUAL = 1<<3,
	/*set if node is registered in previous node drawn list of the current visual manager
	the flag is only set during a visual_draw_frame pass*/
	DRAWABLE_REGISTERED_WITH_VISUAL = 1<<4,

	/*drawable is an overlay surface*/
	DRAWABLE_IS_OVERLAY = 1<<5,

	/*drawable has a cache texture*/
	DRAWABLE_IS_CACHED = 1<<6,

	/*drawable has been initialized for hybrid GL mode - used to perform the initial clear of the overlay graphics*/
	DRAWABLE_HYBGL_INIT = 1<<7,
};

struct _drawable
{
#ifndef GPAC_DISABLE_3D
	/*3D object for drawable if needed - NOT ALLOCATED BY DEFAULT
	DO NOT CHANGE THE LOCATION IN THE STRUCTURE, IT SHALL BE FIRST TO BE TYPECASTED TO
	A Drawable3D when drawing OpenGL*/
	GF_Mesh *mesh;
#endif
	/*set of drawable flags*/
	u32 flags;

	/*node owning the drawable*/
	GF_Node *node;

	/*bounds collector*/
	struct _dirty_rect_info *dri;

	/*graphic path - !! CREATED BY DEFAULT !! */
	GF_Path *path;
	/*cached outlines*/
	struct _strikeinfo2d *outline;
};

/*construction destruction*/
Drawable *drawable_new();
void drawable_del(Drawable *dr);
void drawable_del_ex(Drawable *dr, GF_Compositor *compositor, Bool no_free);

/*cleans up the drawable attached to the node*/
void drawable_node_del(GF_Node *node);

/*init drawable structure when stack extends drawable (text, bitmap)*/
void drawable_init_ex(Drawable *tmp);


/*
decide whether drawing is needed or not based on visual settings and parent node - must be called
at the end of each TRAVERSE_SORT of drawable nodes
if orig_bounds is NULL, function uses the bounds of the drawable's path
*/
void drawable_finalize_sort(DrawableContext *ctx, GF_TraverseState *tr_state, GF_Rect *orig_bounds);
/*same as drawable_finalize_sort but skips focus check*/
void drawable_finalize_sort_ex(DrawableContext *ctx, GF_TraverseState *tr_state, GF_Rect *orig_bounds, Bool skip_focus);

/*base constructor for geometry objects that work without overloading the drawable stuff*/
Drawable *drawable_stack_new(GF_Compositor *compositor, GF_Node *node);
/*reset all paths (main path and any outline) of the stack*/
void drawable_reset_path(Drawable *st);
/*reset all paths outlines (only) of the stack*/
void drawable_reset_path_outline(Drawable *st);

/*mark the drawable as modified - this shall be caleed whenever the node geometry is rebuilt
in order to signal this change to the bounds tracker algorithm*/
void drawable_mark_modified(Drawable *st, GF_TraverseState *tr_state);

/*checks if the current object is the focused one, and insert the focus drawable at the current pos*/
void drawable_check_focus_highlight(GF_Node *node, GF_TraverseState *tr_state, GF_Rect *orig_bounds);

/*reset the highlight state (bounds) if associated with the current node. This is automatically called
whenever reseting a drawable but must be called when a grouping node is modified*/
void drawable_reset_group_highlight(GF_TraverseState *tr_state, GF_Node *n);

/*move current bounds to previous bounds for given target visual manager - called BEFORE updating the visual manager
returns 1 if nod was draw last frame on this visual manager, 0 otherwise*/
Bool drawable_flush_bounds(Drawable *node, GF_VisualManager *on_visual, u32 mode2d);

/*
	return 1 if same bound is found in previous list (and remove it from the list)
	return 0 otherwise
*/
Bool drawable_has_same_bounds(DrawableContext *ctx, GF_VisualManager *visual);

/*
	return any previous bounds related to the same visual manager in @rc if any
	if nothing found return 0
*/
Bool drawable_get_previous_bound(Drawable *node, GF_IRect *rc, GF_VisualManager *visual);

/*reset bounds array (current and previous) on the given visual manager*/
void drawable_reset_bounds(Drawable *dr, GF_VisualManager *visual);
/*setup clip/uncli pointers for the drawable context*/
void drawable_check_bounds(DrawableContext *ctx, GF_VisualManager *visual);

/*the focus drawable routine of the focus object*/
void drawable_traverse_focus(GF_Node *node, void *rs, Bool is_destroy);

struct _draw_aspect_2d
{
	/*including alpha*/
	GF_Color fill_color, line_color;
	Fixed line_scale;
	GF_PenSettings pen_props;
	/*texture fill handler*/
	struct _gf_sc_texture_handler *fill_texture;
	/*texture stroke handler*/
	struct _gf_sc_texture_handler *line_texture;
};

/*fills the 2D drawing properties according to the traversing state - MPEG4/X3D only*/
u32 drawable_get_aspect_2d_mpeg4(GF_Node *node, DrawAspect2D *asp, GF_TraverseState *tr_state);
/*fills the 2D drawing properties according to the traversing state - SVG only*/
Bool drawable_get_aspect_2d_svg(GF_Node *node, DrawAspect2D *asp, GF_TraverseState *tr_state);


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
	/*indicates the context is in a flip coord state (used for image and text flip)*/
	CTX_FLIPED_COORDS = 1<<10,
	//flag set in opengl auto mode to indicate that the corresponding area should not be cleared but still redrawn (this means an opaque texture is used)
	CTX_HYBOGL_NO_CLEAR = 1<<11,
	//flag set in opengl auto mode to indicate that the corresponding area should not be cleared but still redrawn (this means an opaque texture is used)
	CTX_BACKROUND_NOT_LAYER = 1<<12,
	CTX_BACKROUND_NO_CLEAR= 1<<13,
};

#define CTX_3DTYPE_MASK 0x7800

#define CTX_REDRAW_MASK	0x00000003

struct _drawable_context
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

	/* extra check data for bounds matching
		for MPEG-4: appearance node
		for SVG: parent <use> node if any
	*/
	GF_Node *appear;

	GF_Node *cliper;

#ifdef GF_SR_USE_DEPTH
	//local gain and offset
	Fixed depth_gain, depth_offset;
#endif

};

DrawableContext *NewDrawableContext();
void DeleteDrawableContext(DrawableContext *);
void drawctx_reset(DrawableContext *ctx);
void drawctx_update_info(DrawableContext *ctx, GF_VisualManager *visual);

#ifndef GPAC_DISABLE_VRML
/*inits context - may return NULL if the node doesn't have to be drawn*/
DrawableContext *drawable_init_context_mpeg4(Drawable *node, GF_TraverseState *tr_state);
#endif

/*inits context for SVG - may return NULL if the node doesn't have to be drawn*/
DrawableContext *drawable_init_context_svg(Drawable *drawable, GF_TraverseState *tr_state, SVG_ClipPath *clip_path);

/*base draw function for 2D objects (texturing, fill and strike)
	in 2D, the current context is passed in the traversing state
*/
void drawable_draw(Drawable *drawable, GF_TraverseState *tr_state);

void call_drawable_draw(DrawableContext *ctx, GF_TraverseState *tr_state, Bool set_cyclic);

/*picking function for VRML-based scene graphs*/
void vrml_drawable_pick(Drawable *drawable, GF_TraverseState *tr_state);

/*SVG picking function (uses SVG pointrer events)*/
void svg_drawable_pick(GF_Node *node, Drawable *drawable, GF_TraverseState *tr_state);

/*stored at compositor level and in each drawable node*/
typedef struct _strikeinfo2d
{
	struct _strikeinfo2d *next;
	/*vectorial outline*/
	GF_Path *outline;
	/*drawable using this outline*/
	Drawable *drawable;
	/*lineprops used to build outline (MPEG-4 only)*/
	GF_Node *lineProps;
	/*user+world->local scaling for non-scalable outlines*/
	Fixed line_scale;
	/*SVG path length*/
	Fixed path_length;
	/*set only for text, indicates sub-path outline*/
	GF_Path *original;


#ifndef GPAC_DISABLE_3D
	/*3D drawing*/
	Bool is_vectorial;
	GF_Mesh *mesh_outline;
#endif
} StrikeInfo2D;

void delete_strikeinfo2d(StrikeInfo2D *info);
/*get strike and manage any scale change&co. This avoids recomputing outline at each frame...*/
StrikeInfo2D *drawable_get_strikeinfo(GF_Compositor *compositor, Drawable *drawable, DrawAspect2D *asp, GF_Node *appear, GF_Path *path, u32 svg_flags, GF_TraverseState *tr_state);


void drawable_compute_line_scale(GF_TraverseState *tr_state, DrawAspect2D *asp);

Bool svg_drawable_is_over(Drawable *drawable, Fixed x, Fixed y, DrawAspect2D *asp, GF_TraverseState *tr_state, GF_Rect *glyph_rc);

void drawable_check_texture_dirty(DrawableContext *ctx, Drawable *drawable, GF_TraverseState *tr_state);
#endif
