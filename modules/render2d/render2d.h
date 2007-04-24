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

#ifndef RENDER2D_H
#define RENDER2D_H

#include <gpac/internal/renderer_dev.h>

#ifndef GPAC_DISABLE_SVG
#include <gpac/scenegraph_svg.h>
#endif

typedef struct _render_2d
{
	/*remember main renderer*/
	GF_Renderer *compositor;
	/*all outlines cached*/
	GF_List *strike_bank;
	/*all 2D surfaces created*/
	GF_List *surfaces_2D;
	/*all 2D sensors registered*/
	GF_List *sensors;

	/*simple counter for composite texture check*/
	u32 frame_num;

	/*tracking status*/
	Bool is_tracking;
	struct _drawable_context *grab_ctx;
	struct _drawable *grab_node;
	u32 last_sensor;
	GF_Node *focus_node;

	/*top level effect for zoom/pan*/
	struct _render2d_effect *top_effect;
	/*main 2D surface we're writing on*/
	struct _visual_surface_2D *surface;
	Bool main_surface_setup;

	/*screen access*/
	void *hardware_context;
	GF_VideoSurface hw_surface;
	Bool locked;
	Bool scalable_zoom, enable_yuv_hw;

	/*current output info: top-left point of scene viewport (according to user AR settings), and current scaled scene size*/
	u32 offset_x, offset_y, cur_width, cur_height;
	/*scale factor (scalable zoom only)*/
	Fixed scale_x, scale_y;
	/*rotation angle*/
	Fixed rotation;

	Bool navigation_disabled;

	Bool grabbed;
	s32 grab_x, grab_y;
	Fixed zoom, trans_x, trans_y;
	u32 navigate_mode;
	Bool use_dom_events;

#ifndef GPAC_DISABLE_SVG
	s32 last_click_x, last_click_y;
	u32 num_clicks;
#endif
} Render2D;


/*sensor node handler - this is not defined as a stack (cf below) because Anchor is both a grouping
node and a sensor node, and we DO need the groupingnode stack...*/
typedef struct _sensorhandler
{
	/*sensor enabled or not ?*/
	Bool (*IsEnabled)(struct _sensorhandler *sh);
	/*user input on sensor. If return value is non-0, any sensors present above(before in VRML)
	the called sensor won't be called
		@evt: the event called
		@ctx: the drawable context of the node attached to this handler, NULL when deactivating
		@sensor_matrix: the world->local matrix at the sensor location
	*/
	Bool (*OnUserEvent)(struct _sensorhandler *sh, GF_Event *evt, struct _drawable_context *ctx, GF_Matrix2D *sensor_matrix);
	/*set the node pointer here*/
	GF_Node *owner;
	/*private to compositor for deactivating sensors*/
	Bool skip_second_pass;
} SensorHandler;

/*returns TRUE if the node is a pointing device sensor node that can be stacked during traversing (all sensor except anchor)*/
Bool is_sensor_node(GF_Node *node);
/*returns associated sensor handler from traversable stack (the node handler is always responsible for creation/deletion)
returns NULL if not a sensor or sensor is not activated*/
SensorHandler *get_sensor_handler(GF_Node *node);

void R2D_RegisterSensor(GF_Renderer *sr, SensorHandler *sh);
void R2D_UnregisterSensor(GF_Renderer *sr, SensorHandler *sh);

/*sensor context is needed for DEF/USE of a sensor over several shapes*/
typedef struct __sensor_ctx
{
	struct __sensor_ctx *next;
	SensorHandler *h_node;
	GF_Matrix2D matrix;
} SensorContext;

/*extra texture flags*/
enum
{
	/*set to signal texture is a composite one*/
	GF_SR_TEXTURE_COMPOSITE = (1<<3),
};

enum
{
	/*when set objects are drawn as soon as traversed, at each frame*/
	TF_RENDER_DIRECT		= (1<<2),
};

/*picking modes*/
enum
{
	/*check point is over path, regardless of visual settings*/
	PICK_PATH = 0,
	/*check point is over path and outline, regardless of visual settings*/
	PICK_PATH_AND_OUTLINE, 
	/*check point is over path and/or outline, with respect to visual settings*/
	PICK_FULL
};

/*rendering modes*/
enum
{
	/*regular traversin mode*/
	TRAVERSE_RENDER = 0,
	/*draw routine*/
	TRAVERSE_DRAW,
	/*pick routine*/
	TRAVERSE_PICK,
	/*get bounds routine: returns bounds in local coord system (including node transform if any)*/
	TRAVERSE_GET_BOUNDS,
};

/*the traversing context: set_up at top-level and passed through SFNode_Render*/
typedef struct _render2d_effect
{
	BASE_EFFECT_CLASS
	
	u32 traversing_mode;

	/*current graph traversed is in pixel metrics*/
	Bool is_pixel_metrics;
	Fixed min_hsize;

	/*the one and only 2D surface currently traverse*/
	struct _visual_surface_2D *surface;
	
	/*current background and viewport stacks*/
	GF_List *back_stack, *view_stack;

	/*current transformation from top-level. 
	If TF_RENDER_GET_BOUNDS is set, this shall be updated to the matrix at current level*/
	GF_Matrix2D transform;
	/*current color transformation from top-level*/
	GF_ColorMatrix color_mat;
	/* Contains the viewbox transform, used for svg ref() transform */
	GF_Matrix2D vb_transform;

	/*if set all nodes shall be redrawn - set only at specific places in the tree*/
	Bool invalidate_all;
	/*indicates background nodes shall be drawn*/
	Bool draw_background;
	/*text splitting: 0: no splitting, 1: word by word, 2:letter by letter*/
	u32 text_split_mode;

	/*all sensors for the current level*/
	GF_List *sensors;

	/*current appearance when traversing geometry nodes*/
	GF_Node *appear;
	/*parent group for composition: can be Form, Layout or Layer2D*/
	struct _parent_group *parent;

	/*for object picking*/
	GF_Rect bounds;
	GF_Node *for_node;

	/* Styling Property and others for SVG context */
#ifndef GPAC_DISABLE_SVG
	GF_Node *parent_use;
	/*number of listeners in the current tree branch*/
	u32 nb_listeners;
#endif
	/*set when drawing/picking*/
	struct _drawable_context *ctx;
	/*picking info*/
	Fixed x, y;
	u32 pick_type;
	/*pick result*/
	Bool is_over;

	Bool inside_cliper;
} RenderEffect2D;

void effect_reset(RenderEffect2D *eff);
void effect_delete(RenderEffect2D *eff);

void effect_add_sensor(RenderEffect2D *eff, SensorHandler *ptr, GF_Matrix2D *mat);
/*destroy last entry in sensitive list*/
void effect_pop_sensor(RenderEffect2D *eff);
void effect_reset_sensors(RenderEffect2D *eff);

void R2D_RegisterSurface(Render2D *sr, struct _visual_surface_2D *);
void R2D_UnregisterSurface(Render2D *sr, struct _visual_surface_2D *);
Bool R2D_IsSurfaceRegistered(Render2D *sr, struct _visual_surface_2D *);

void R2D_LinePropsRemoved(Render2D *sr, GF_Node *n);

Bool R2D_IsPixelMetrics(GF_Node *n);

Bool R2D_NodeChanged(GF_VisualRenderer *vr, GF_Node *byObj);
void R2D_NodeInit(GF_VisualRenderer *vr, GF_Node *node);

GF_TextureHandler *R2D_GetTextureHandler(GF_Node *n);

/*converts clipper to pixel metrics (used by layer, layout, etc)*/
GF_Rect R2D_ClipperToPixelMetrics(RenderEffect2D *eff, SFVec2f size);


#ifndef GPAC_DISABLE_SVG

#ifdef GPAC_ENABLE_SVG_SA
void R2D_render_svg_sa_animation(GF_Node *anim, GF_Node *sub_root, void *rs);
void R2D_render_svg_sa_use(GF_Node *anim, GF_Node *sub_root, void *rs);
#endif


#ifdef GPAC_ENABLE_SVG_SANI
void r2d_render_svg_sani_use(GF_Node *anim, GF_Node *sub_root, void *rs);
#endif
void R2D_RenderUse3(GF_Node *anim, GF_Node *sub_root, void *rs);
#endif

#endif

