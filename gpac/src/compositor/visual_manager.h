/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2022
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


//startof GL3/ES2.0 specifics

/* number of preprocessor flags for GL3/ES2.0 */
#define GF_GL_NUM_OF_FLAGS			6
#ifdef GPAC_CONFIG_ANDROID
#define GF_GL_NB_FRAG_SHADERS		(1<<(GF_GL_NUM_OF_FLAGS) )	//=2^GF_GL_NUM_OF_FLAGS
#else
#define GF_GL_NB_FRAG_SHADERS		(1<<(GF_GL_NUM_OF_FLAGS-1) )	//=2^GF_GL_NUM_OF_FLAGS-1 ( ExternalOES ignored in fragment shader when the platform is not Android)
#endif // GPAC_CONFIG_ANDROID
#define GF_GL_NB_VERT_SHADERS		(1<<(GF_GL_NUM_OF_FLAGS-2) )	//=2^GF_GL_NUM_OF_FLAGS-2 (YUV and ExternalOES ignored in vertex shader)

/* setting preprocessor flags for GL3/ES2.0 shaders */
enum {
	GF_GL_HAS_TEXTURE = 1,
	GF_GL_HAS_LIGHT = (1<<1),
	GF_GL_HAS_COLOR = (1<<2),
	GF_GL_HAS_CLIP = (1<<3),
	//only for fragment shaders
	GF_GL_IS_YUV = (1<<4),
	GF_GL_IS_ExternalOES = (1<<5),
};
//endof

enum
{
	GF_3D_CAMERA_STRAIGHT = 0,
	GF_3D_CAMERA_OFFAXIS,
	GF_3D_CAMERA_LINEAR,
	GF_3D_CAMERA_CIRCULAR,
};


#ifndef GPAC_DISABLE_3D

#if defined( _LP64 ) && defined(CONFIG_DARWIN_GL)
#define GF_SHADERID u64
#else
#define GF_SHADERID u32
#endif

typedef struct _gl_prog
{
	GF_SHADERID vertex;
	GF_SHADERID fragment;
	GF_SHADERID prog;
	u32 flags;
	u32 pix_fmt;
} GF_GLProgInstance;
#endif


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

	GF_IRect frame_bounds;

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
		offscreen_clear is set to 1 when the clear targets the entire canvas buffer in hybrid GL mode (full redraw)
		                       to 2 when the clear targets a part of the canvas buffer in hybrid GL mode (dirty area clean)
	*/
	void (*ClearSurface)(GF_VisualManager *visual, GF_IRect *rc, u32 BackColor, u32 offscreen_clear);
	/*draws specified texture as flat bitmap*/
	Bool (*DrawBitmap)(GF_VisualManager *visual, GF_TraverseState *tr_state, DrawableContext *ctx);
	/*checks if the visual is ready for being drawn on. Returns GF_FALSE if no draw operation can be sent*/
	Bool (*CheckAttached)(GF_VisualManager *visual);

	/*raster surface interface*/
	GF_EVGSurface *raster_surface;
	/*raster brush interface*/
	GF_EVGStencil *raster_brush;

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

#ifndef GPAC_DISABLE_VRML
	/*navigation stack*/
	GF_List *navigation_stack;
	/*fog stack*/
	GF_List *fog_stack;
#endif

	Bool gl_setup;

	GF_IRect clipper_2d;
	Bool has_clipper_2d;
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

	//when using 2D layering in opengl, we store the bounding rect of drawn objects between GL calls, so we
	//can flush only the minimum part of the texture
	GF_RectArray hybgl_drawn;
	u32 nb_objects_on_canvas_since_last_ogl_flush;
	//for defer rendering
	u32 prev_hybgl_canvas_not_empty;

	u32 nb_views, current_view, autostereo_type, camlay;

	GF_SHADERID base_glsl_vertex;

	u32 *gl_textures;
	u32 auto_stereo_width, auto_stereo_height;
	GF_Mesh *autostereo_mesh;
	GF_SHADERID autostereo_glsl_program;
	GF_SHADERID autostereo_glsl_fragment;

	Bool needs_projection_matrix_reload;

	/*GL state to emulate with GLSL [ES2.0]*/
	Bool has_material_2d;
	SFColorRGBA mat_2d;

	Bool has_material;
	SFColorRGBA materials[4];
	Fixed shininess;

	Bool state_light_on, state_blend_on, state_color_on;

	GF_ClipInfo clippers[GF_MAX_GL_CLIPS];

	Bool has_tx_matrix;
	GF_Matrix tx_matrix;

	GF_LightInfo lights[GF_MAX_GL_LIGHTS];
	Bool has_inactive_lights;

	Bool has_fog;
	u32 fog_type;
	SFColor fog_color;
	Fixed fog_density, fog_visibility;

	/*end of GL state to emulate with GLSL*/

	/* shaders used for shader-only drawing */
	GF_SHADERID glsl_program;
	GF_List *compiled_programs;
	u32 bound_tx_pix_fmt; //only one texture currently supported

	u32 active_glsl_flags;
#endif	//!GPAC_DISABLE_3D

#ifdef GF_SR_USE_DEPTH
	Fixed depth_vp_position, depth_vp_range;
#endif

	u32 yuv_pixelformat_type;
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

#ifndef GPAC_DISABLE_3D
GF_GLProgInstance *visual_3d_check_program_exists(GF_VisualManager *root_visual, u32 flags, u32 pix_fmt);
#endif

#endif	/*_VISUAL_MANAGER_H_*/

