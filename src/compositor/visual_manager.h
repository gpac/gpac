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

#ifndef _VISUAL_MANAGER_H_
#define _VISUAL_MANAGER_H_

#include "drawable.h"

/*all 2D related functions and macro are locate there*/
#include "visual_manager_2d.h"

/*all 3D related functions and macro are locate there*/
#include "visual_manager_3d.h"


enum 
{
	GF_3D_STEREO_NONE = 0,
	GF_3D_STEREO_TOP,
	GF_3D_STEREO_SIDE,
	/*all modes above GF_3D_STEREO_SIDE require shaders*/

	/*custom interleaving using GLSL shaders*/
	GF_3D_STEREO_CUSTOM,
	/*some built-in interleaving modes*/
	/*each pixel correspond to a different view*/
	GF_3D_STEREO_COLUMNS,
	GF_3D_STEREO_ROWS,
	/*special case of sub-pixel interleaving for 2 views*/
	GF_3D_STEREO_ANAGLYPH,
	/*SpatialView 19'' 5views interleaving*/
	GF_3D_STEREO_5VSP19,
};

enum 
{
	GF_3D_CAMERA_STRAIGHT = 0,
	GF_3D_CAMERA_OFFAXIS,
	GF_3D_CAMERA_LINEAR,
	GF_3D_CAMERA_CIRCULAR,
};

struct _visual_manager
{
	GF_Compositor *compositor;
	Bool direct_flush;

#ifndef GPAC_DISABLE_3D
	/*3D type for the visual:
	0: visual is 2D
	1: visual is 2D with 3D acceleration (2D camera)
	2: visual is 3D MPEG-4 (with 3D camera)
	3: visual is 3D X3D (with 3D camera)
	*/
	u32 type_3d;
#endif

#ifndef GPAC_DISABLE_VRML
	/*background stack*/
	GF_List *back_stack;
	/*viewport stack*/
	GF_List *view_stack;
#endif


	/*size in pixels*/
	u32 width, height;

	/*
	 *	Visual Manager part for 2D drawing and dirty rect
	 */

	/*the one and only dirty rect collector for this visual manager*/
	GF_RectArray to_redraw;
	u32 draw_node_index;

	/*display list (list of drawable context). The first context with no drawable attached to 
	it (ctx->drawable==NULL) marks the end of the display list*/
	DrawableContext *context, *cur_context;

	/*keeps track of nodes drawn last frame*/
	struct _drawable_store *prev_nodes, *last_prev_entry;

	/*pixel area in BIFS coords - eg area to fill with background*/
	GF_IRect surf_rect;
	/*top clipper (may be different than surf_rect when a viewport is active)*/
	GF_IRect top_clipper;

	u32 last_had_back;

	/*signals that the hardware surface is attached to buffer/device/stencil*/
	Bool is_attached;
	Bool center_coords;
	Bool has_modif;
	Bool has_overlays;
	Bool has_text_edit;

	/*gets access to graphics handle (either OS-specific or raw memory)*/
	GF_Err (*GetSurfaceAccess)(GF_VisualManager *);
	/*release graphics handle*/
	void (*ReleaseSurfaceAccess)(GF_VisualManager *);

	/*clear given rect or all visual if no rect specified - clear color depends on visual's type:
		BackColor for background nodes
		0x00000000 for composite, 
		compositor clear color otherwise
	*/
	void (*ClearSurface)(GF_VisualManager *visual, GF_IRect *rc, u32 BackColor);
	/*draws specified texture as flat bitmap*/
	Bool (*DrawBitmap)(GF_VisualManager *visual, GF_TraverseState *tr_state, DrawableContext *ctx, GF_ColorKey *col_key);

	/*raster surface interface*/
	GF_SURFACE raster_surface;
	/*raster brush interface*/
	GF_STENCIL raster_brush;

	/*node owning this visual manager (composite textures) - NULL for root visual*/
	GF_Node *offscreen;

	/*value of the flag to use to signal any geometry changes*/
	u32 bounds_tracker_modif_flag;

	u32 num_nodes_prev_frame, num_nodes_current_frame;

	/*list of video overlays sorted from first to last*/
	struct _video_overlay *overlays;

#ifndef GPAC_DISABLE_3D
	/*
	 *	Visual Manager part for 3D drawing 
	 */

#if defined( _LP64 ) && 0 && defined(CONFIG_DARWIN_GL)
#define GF_SHADERID u64
#else
#define GF_SHADERID u32
#endif

#ifndef GPAC_DISABLE_VRML
	/*navigation stack*/
	GF_List *navigation_stack;
	/*fog stack*/
	GF_List *fog_stack;
#endif

	/*the one and only camera associated with the visual*/
	GF_Camera camera;

	/*list of transparent nodes to draw after TRAVERSE_SORT pass*/
	GF_List *alpha_nodes_to_draw;

	/*lighting stuff*/
	u32 num_lights;
	u32 max_lights;
	/*cliping stuff*/
	u32 num_clips;
	u32 max_clips;

	//when using 2D layering in opengl, we store the bounding rect of drawn objects betwwen GL calls, so we
	//can flush only the minimum part of the texture
	GF_RectArray hybgl_drawn;
	u32 nb_objects_on_canvas_since_last_ogl_flush;

	u32 nb_views, current_view, autostereo_type, camera_layout;
	Bool reverse_views;

	GF_SHADERID base_glsl_vertex;

	u32 *gl_textures;
	u32 auto_stereo_width, auto_stereo_height;
	GF_Mesh *autostereo_mesh;
	GF_SHADERID autostereo_glsl_program;
	GF_SHADERID autostereo_glsl_fragment;
	
	GF_SHADERID yuv_glsl_program;
	GF_SHADERID yuv_glsl_fragment;
	GF_SHADERID yuv_rect_glsl_program;
	GF_SHADERID yuv_rect_glsl_fragment;

	GF_SHADERID current_texture_glsl_program;


	Bool has_mat_2d;
	SFColorRGBA mat_2d;

	Bool has_mat;
	SFColorRGBA materials[4];
	Float shininess;

	Bool state_light_on, state_blend_on, state_color_on;

	GF_Plane clippers[GF_MAX_GL_CLIPS];
	GF_Matrix *mx_clippers[GF_MAX_GL_CLIPS];

	Bool has_tx_matrix;
	GF_Matrix tx_matrix;

	Bool needs_projection_matrix_reload;

	GF_LightInfo lights[GF_MAX_GL_LIGHTS];
	Bool has_inactive_lights;

	Bool has_fog;
	u32 fog_type;
	SFColor fog_color;
	Fixed fog_density, fog_visibility;

#endif

#ifdef GF_SR_USE_DEPTH
	Fixed depth_vp_position, depth_vp_range;
#endif

};

/*constructor/destructor*/
GF_VisualManager *visual_new(GF_Compositor *compositor);
void visual_del(GF_VisualManager *visual);

/*draw cycle for the visual*/
Bool visual_draw_frame(GF_VisualManager *visual, GF_Node *root, GF_TraverseState *tr_state, Bool is_root_visual);

/*executes scene event (picks node if needed) - returns FALSE if no scene event handler has been called*/
Bool visual_execute_event(GF_VisualManager *visual, GF_TraverseState *tr_state, GF_Event *ev, GF_ChildNodeItem *children);

Bool visual_get_size_info(GF_TraverseState *tr_state, Fixed *surf_width, Fixed *surf_height);

/*reset all appearance dirty state and visual registration info*/
void visual_clean_contexts(GF_VisualManager *visual);


void visual_reset_graphics(GF_VisualManager *visual);

#endif	/*_VISUAL_MANAGER_H_*/

