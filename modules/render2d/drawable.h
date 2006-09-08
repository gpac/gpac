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

/*

		ALL THE DRAWING ARCHITECTURE HERE IS FOR 2D SURFACES ONLY

	a drawable node is a node that is drawn:) eg all geometry nodes and background2D
a node may be drawn in different places (DEF/USE) so we also define a drawble context (eg context of one
instance of the drawn node)

*/



/*this is used to store context info between 2 frames, since the DrawableContext is not kept between frames */
typedef struct _boundinfo
{
	/*integer bounds in pixels (full and clip) */
	GF_IRect clip, unclip;
	/* a node may have several appearances (due to DEF/USE), 
	   an the appearance is needed to compute bounds for the drawable in this context*/
	M_Appearance *appear;
	/*surface the node is drawn on (to handle compositeTexture2D)*/
	struct _visual_surface_2D *surface;
} BoundsInfo;


typedef struct _drawable
{
	GF_Node *owner;
	GF_Renderer *compositor;

	/*
			part to overload per drawable node if needed (IFS2D, ILS2D, Text, ...)
		simple objects using the default graphics path and a single texture/appearance use
		the default methods
	*/

	/*actual drawing of the node */
	void (*Draw)(struct _drawable_context *ctx);
	/*returns TRUE if point is over node - by default use drawable path
	check_type: 
		0: check point is over path, regardless of visual settings
		1: check point is over path and outline, regardless of visual settings
		2: check point is over path and/or outline, with respect to visual settings
	*/
	Bool (*IsPointOver)(struct _drawable_context *ctx, Fixed x, Fixed y, u32 check_type);

	/*
			common data for all	drawable nodes
	*/

	/*default graphic path - this is also the path used in PathLayout*/
	GF_Path *path;

	/*
			Private data for compositor use
	*/

	/*current bounds and previous frame bounds list*/
	struct _boundinfo **current_bounds, **previous_bounds;
	u32 current_count, previous_count, bounds_size;
	/*frame number (from compositor.current_frame) used to skip bounds flushing on nodes present on main surface and
	composite textures, otherwise the scene would be constantly redrawn*/
	u32 last_bound_flush_frame;

	/*unused node detetction*/
	Bool first_ctx_update;
	Bool node_was_drawn;
	Bool node_changed;
	/*list of surfaces the node is still attached to for dirty rects*/
	GF_List *on_surfaces;

	/*cached outlines*/
	GF_List *strike_list;
} Drawable;

/*construction destruction*/
Drawable *drawable_new();
void drawable_del(Drawable *);
/*store ctx bounds in current bounds*/
void drawable_store_bounds(struct _drawable_context *ctx);

/*move current bounds to previous bounds - THIS MUST BE CALLED IN BEFORE UPDATING ANY CONTEXT INFO
OF THE NODE, otherwise some bounds info may be lost when using composite textures*/
void drawable_flush_bounds(Drawable *node, u32 frame_num);

/*register/unregister node on surface for dirty rect*/
void drawable_register_on_surface(Drawable *node, struct _visual_surface_2D *surf);
void drawable_unregister_from_surface(Drawable *node, struct _visual_surface_2D *surf);
/*
	return 1 if same bound is found in previous list (and remove it from the list)
	return 0 otherwise
*/
Bool drawable_has_same_bounds(struct _drawable_context *ctx);
/*
	return any previous bounds related to the same surface in @rc if any
	if nothing found return 0
*/
Bool drawable_get_previous_bound(Drawable *node, GF_IRect *rc, struct _visual_surface_2D *surf);
/*reset content of previous bounds list*/
void drawable_reset_previous_bounds(Drawable *node);

/*
decide whether drawing is needed or not based on rendering settings and parent node - must be called
at the end of each render of drawable nodes
*/
void drawable_finalize_render(struct _drawable_context *ctx, RenderEffect2D *effects);
/*performs final task common to drawable_finalize_render and layout node rendering (cf grouping.c)*/
void drawable_finalize_end(struct _drawable_context *ctx, RenderEffect2D *eff);

/*base constructor for geometry objects that work without overloading the drawable stuff*/
Drawable *drawable_stack_new(Render2D *sr, GF_Node *node);
/*reset all paths (main path and any outline) of the stack*/
void drawable_reset_path(Drawable *st);

/*reset bounds array (current and previous) - used when turning direct rendering on, to 
release some memory*/
void drawable_reset_bounds(Drawable *dr);


typedef struct
{
	/*including alpha*/
	GF_Color fill_color, line_color;
	Bool filled, has_line, is_scalable;
	Fixed line_scale;
	GF_PenSettings pen_props;
	/*texture fill handler*/
	struct _gf_sr_texture_handler *line_texture;
	/*original alpha without color transforms*/
	u8 fill_alpha;
} DrawAspect2D;

enum
{
	/*set whenever geometry node changed*/
	CTX_NODE_DIRTY = 1,
	/*set whenever appearance changed*/
	CTX_APP_DIRTY = 1<<1,
	/*set whenever texture data changed*/
	CTX_TEXTURE_DIRTY = 1<<2
};

typedef struct _drawable_context
{
	/*clipped (drawned) and uncliped (for sensors) rect in pixels*/
	GF_IRect clip, unclip_pix;
	/*exact unclipped rect for sensors*/
	GF_Rect unclip;
	/*original bounds (in local coord system, float)
		x,y is top left
		x is going right
		y is going down
	*/
	GF_Rect original;
	/*draw info*/
	DrawAspect2D aspect;
	/*transform matrix from top*/
	GF_Matrix2D transform;
	/*color matrix*/
	GF_ColorMatrix cmat;
	/*sensors attached to this context*/
	GF_List *sensors;
	/*visual surface this ctx belongs to*/
	struct _visual_surface_2D *surface;
	/*drawable using this context*/
	Drawable *node;
	/*current appearance node*/
	GF_Node *appear;
	/*texture fill handler*/
	struct _gf_sr_texture_handler *h_texture;

	/*any of the above flags*/
	u32 redraw_flags;

	/*set if node completely fits its bounds (flat rect and bitmap) then non transparent*/
	Bool transparent;
	/*set if text ctx*/
	Bool is_text;
	/*set by background*/
	Bool is_background;
	/*used by bitmap - no antialiasing when drawn*/
	Bool no_antialias;
	/*only used by text when splitting strings into chars / substrings*/
	s32 sub_path_index;

	/*private for render, indicates path has been textured, in which case FILL is skiped*/
	Bool path_filled;
	/*private for render, indicates path outline has been textured, in which case STRIKE is skiped*/
	Bool path_stroke;
	Bool invalid;

	/*for SVG & co: number of listeners from doc root to current element*/
	u32 num_listeners;
#ifndef GPAC_DISABLE_SVG
	GF_Node *parent_use;
#endif
} DrawableContext;	

DrawableContext *NewDrawableContext();
void DeleteDrawableContext(DrawableContext *);
void drawctx_reset(DrawableContext *ctx);
void drawctx_update_info(DrawableContext *ctx);
void drawctx_reset_sensors(DrawableContext *ctx);

/*inits context - may return NULL if the node doesn't have to be drawn*/
DrawableContext *drawable_init_context(Drawable *node, RenderEffect2D *effects);

/*store untransformed bounds for this context - provides bounds storing for basic nodes 
otherwise complex nodes shall store their bounds manually, and always before calling drawable_finalize_render*/
void drawctx_store_original_bounds(DrawableContext *ctx);


/*stored at compositor level and in each drawable node*/
typedef struct
{
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
StrikeInfo2D *drawctx_get_strikeinfo(DrawableContext *ctx, GF_Path *txt_path);

#endif
