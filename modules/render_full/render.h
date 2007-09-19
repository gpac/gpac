/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005 
 *					All rights reserved
 *
 *  This file is part of GPAC / 2D+3D rendering module
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

#ifndef _RENDER_H_
#define _RENDER_H_

#include <gpac/internal/renderer_dev.h>
#include <gpac/options.h>

#include <gpac/nodes_mpeg4.h>
#include <gpac/nodes_x3d.h>

#ifndef GPAC_DISABLE_SVG
#include <gpac/nodes_svg_da.h>
#endif

/*forward definition of the visual manager*/
typedef struct _visual_manager GF_VisualManager;
typedef struct _draw_aspect_2d DrawAspect2D;

//#define GPAC_DISABLE_3D

#define GPAC_RENDER_USE_CACHE

#ifndef GPAC_DISABLE_3D
#include "camera.h"
#include "mesh.h"

typedef struct 
{
	Bool multisample;
	Bool bgra_texture;
	Bool abgr_texture;
	Bool npot_texture;
	Bool rect_texture;
} GLCaps;

#endif

/*structure used for 2D and 3D picking*/
typedef struct
{
	/*picked node*/
	GF_Node *picked;
	/*appearance at hit point - used for composite texture*/
	GF_Node *appear;

	/*parent use if any - SVG only*/
	GF_Node *use;

	/*picked node uses DOM event or VRML events ?*/
	Bool use_dom_events;

	/*world->local and local->world transform at hit point*/
	GF_Matrix world_to_local, local_to_world;
	/*hit point in local coord & world coord*/
	SFVec3f local_point, world_point;
	/*tex coords at hit point*/
	SFVec2f hit_texcoords;
	/*picking ray in world coord system*/
	GF_Ray world_ray;

	/*normal at hit point, local coord system*/
	SFVec3f hit_normal;
	/*distance from ray origin used to discards further hits - FIXME: may not properly work with transparent layer3D*/
	Fixed picked_square_dist;
} PickingInfo;


typedef struct _render_2d_3d
{
	/*remember main renderer*/
	GF_Renderer *compositor;
	/*all visual managers created*/
	GF_List *visuals;
	/*all outlines cached*/
	GF_List *strike_bank;

	/*main visual manager (the one managing a video buffer (the video output or offscreen one)*/
	GF_VisualManager *visual;
	/*set to false whenever a new scene is attached to renderer*/
	Bool root_visual_setup;
	
	/*indicates whether the aspect ratio shall be recomputed:
		1: AR changed
		2: AR changed and root visual type changed between 2D and 3D
	*/
	u32 recompute_ar;

	/*traversing context*/
	struct _traversing_state *traverse_state;

	/*current picked node if any*/
	GF_Node *grab_node;
	/*current picked node's parent use if any*/
	GF_Node *grab_use;
	/*current focus node if any*/
	GF_Node *focus_node;
	/*current sensor type*/
	u32 sensor_type;
	/*list of VRML sensors active before the picking phase (eg active at the previous pass)*/
	GF_List *previous_sensors;
	/*list of VRML sensors active after the picking phase*/
	GF_List *sensors;
	/*indicates a sensor is currently active*/
	Bool grabbed_sensor;

	/*visual output location on window (we may draw background outside of it) */
	u32 vp_x, vp_y, vp_width, vp_height;
	/*current scaled scene size*/
	u32 cur_width, cur_height;

	/*hardware handle for 2D screen access - currently only used with win32 (HDC) */
	void *hw_context;
	/*indicates whether HW is locked*/
	Bool hw_locked;
	/*screen buffer for direct access*/
	GF_VideoSurface hw_surface;

	/*options*/
	Bool scalable_zoom;
	Bool enable_yuv_hw;
	/*disables partial hardware blit (eg during dirty rect) to avoid artefacts*/
	Bool disable_partial_hw_blit;

	/*root node uses dom events*/
	Bool root_uses_dom_events;

	/*user navigation mode*/
	u32 navigate_mode;
	/*set if content doesn't allow navigation*/
	Bool navigation_disabled;

	/*user mouse navigation is currently active*/
	Bool navigation_grabbed;
	/*navigation x & y grab point in scene coord system*/
	Fixed grab_x, grab_y;
	/*aspect ratio scale factor*/
	Fixed scale_x, scale_y;
	/*user zoom level*/
	Fixed zoom;
	/*user pan*/
	Fixed trans_x, trans_y;
	/*user rotation angle - ALWAYS CENTERED*/
	Fixed rotation;

#ifndef GPAC_DISABLE_SVG
	u32 num_clicks;
#endif

	/*a dedicated drawable for focus highlight */
	struct _drawable *focus_highlight;
	/*highlight fill and stroke colors (ARGB)*/
	u32 highlight_fill, highlight_stroke;

	/*picking info*/
	PickingInfo hit_info;

#ifndef GPAC_DISABLE_3D
	/*options*/
	/*emulate power-of-2 for video texturing by using a po2 texture and texture scaling. If any textureTransform
	is applied to this texture, black stripes will appear on the borders.
	If not set video is written through glDrawPixels with bitmap (slow scaling), or converted to
	po2 texture*/
	Bool emul_pow2;
	/*use openGL for outline rather than vectorial ones*/
	Bool raster_outlines;
	/*disable RECT extensions (except for Bitmap...)*/
	Bool disable_rect_ext;
	/*disable RECT extensions (except for Bitmap...)*/
	Bool bitmap_use_pixels;
	/*disable RECT extensions (except for Bitmap...)*/
	u32 draw_normals;
	/*backface cull type: 0 off, 1: on, 2: on with transparency*/
	u32 backcull;
	/*polygon atialiasing*/
	Bool poly_aa;
	/*wireframe/solid mode*/
	u32 wiremode;
	/*collision detection mode*/
	u32 collide_mode;
	/*gravity enabled*/
	Bool gravity_on;

	/*unit box (1.0 size) and unit sphere (1.0 radius)*/
	GF_Mesh *unit_bbox;

	/*active layer3D for layer navigation - may be NULL*/
	GF_Node *active_layer;

	GLCaps gl_caps;

	u32 offscreen_width, offscreen_height;
#endif

} Render;


/*sensor node handler - this is not defined as a stack because Anchor is both a grouping node and a 
sensor node, and we DO need the groupingnode stack...*/
typedef struct _sensor_handler
{
	/*sensor enabled or not ?*/
	Bool (*IsEnabled)(GF_Node *node);
	/*user input on sensor:
	is_over: pointing device is over a shape the sensor is attached to
	evt_type: mouse event type
	render: pointer to renderer - hit info is stored at render level
	*/
	void (*OnUserEvent)(struct _sensor_handler *sh, Bool is_over, GF_Event *ev, Render *render);
	/*pointer to the sensor node*/
	GF_Node *sensor;
} SensorHandler;

/*returns TRUE if the node is a pointing device sensor node that can be stacked during traversing (all sensor except anchor)*/
Bool render_is_sensor_node(GF_Node *node);
/*returns associated sensor handler from traversable stack (the node handler is always responsible for creation/deletion)
returns NULL if not a sensor or sensor is not activated*/
SensorHandler *render_get_sensor_handler(GF_Node *n);

/*rendering modes*/
enum
{
	/*regular traversing mode - this usually sorts node:
		- 2D mode: builds the display list
		- 3D mode: sort & queue transparent objects
	*/
	TRAVERSE_RENDER = 0,
	/*explicit draw routine used when flushing 2D display list*/
	TRAVERSE_DRAW_2D,
	/*pick routine*/
	TRAVERSE_PICK,
	/*get bounds routine: returns bounds in local coord system (including node transform if any)*/
	TRAVERSE_GET_BOUNDS,
	/*set to signal bindable render - only called on bindable stack top if present.
	for background (drawing request), viewports/viewpoints fog and navigation (setup)
	all other nodes SHALL NOT RESPOND TO THIS CALL
	*/
	TRAVERSE_RENDER_BINDABLE,

#ifndef GPAC_DISABLE_3D
	/*explicit draw routine used when flushing 3D display list*/
	TRAVERSE_DRAW_3D,
	/*set global lights on. Since the model_matrix is not pushed to the target in this 
	pass, global lights shall not forget to do it (cf lighting.c)*/
	TRAVERSE_LIGHTING,
	/*collision routine*/
	TRAVERSE_COLLIDE,
#endif
};


#define MAX_USER_CLIP_PLANES		4

enum
{
	/*when set objects are drawn as soon as traversed, at each frame*/
	TF_RENDER_DIRECT		= (1<<2),
	/*the subtree indicates no cull shall be performed*/
	TF_DONT_CULL = (1<<3),
};

/*the traversing context: set_up at top-level and passed through SFNode_Render. Each node is responsible for 
restoring the context state before returning*/
typedef struct _traversing_state
{
	BASE_EFFECT_CLASS

	/*current traversing mode*/
	u32 traversing_mode;

	/*current graph traversed is in pixel metrics*/
	Bool pixel_metrics;
	/*current graph traversed uses centered coordinates*/
	Bool center_coords;
	/*minimal half-dimension (w/2, h/2)*/
	Fixed min_hsize;

	/*current size of viewport being traverse (root scene, layers)*/
	SFVec2f vp_size;

	/*the one and only visual manager currently being traversed*/
	GF_VisualManager *visual;
	
	/*current background and viewport stacks*/
	GF_List *backgrounds;
	GF_List *viewpoints;

	/*current transformation from top-level*/
	GF_Matrix2D transform;
	/*current color transformation from top-level*/
	GF_ColorMatrix color_mat;
	/* Contains the viewbox transform, used for svg ref() transform */
	GF_Matrix2D vb_transform;

	/*if set all nodes shall be redrawn - set only at specific places in the tree*/
	Bool invalidate_all;

	/*text splitting: 0: no splitting, 1: word by word, 2:letter by letter*/
	u32 text_split_mode;
	/*1-based idx of text element drawn*/
	u32 text_split_idx;

	/*all VRML sensors for the current level*/
	GF_List *vrml_sensors;

	/*current appearance when traversing geometry nodes*/
	GF_Node *appear;
	/*parent group for composition: can be Form, Layout or Layer2D*/
	struct _parent_node_2d *parent;

	/*group/object bounds in local coordinate system*/
	GF_Rect bounds;

	/*node for which bounds should be fetched - SVG only*/
	GF_Node *for_node;

	/* Styling Property and others for SVG context */
#ifndef GPAC_DISABLE_SVG
	GF_Node *parent_use;
	SVGAllAttributes *parent_vp_atts;
#endif

	/*SVG text rendering state*/
	/* variables pour noeud text, tspan et textArea*/
	/* position of last placed text chunk*/
	u32 chunk_index;
	Fixed text_end_x, text_end_y;

	/* text & tspan state*/
	GF_List *x_anchors;
	SVG_Coordinates *text_x, *text_y;
	u32 count_x, count_y;

	/* textArea state*/
	Fixed max_length, max_height;
	Fixed base_x, base_y;
	Fixed y_step;

	/*current context to be drawn - only set when drawing in 2D mode or 3D for SVG*/
	struct _drawable_context *ctx;

	/*world ray for picking - in 2D, orig is 2D mouse pos and direction is -z*/
	GF_Ray ray;

	/*we have 2 clippers, one for regular clipping (layout, form if clipping) which is maintained in world coord system
	and one for layer2D which is maintained in parent coord system (cf layer rendering). The layer clipper
	is used only when cascading layers - layer3D doesn't use clippers*/
	Bool has_clip, has_layer_clip;
	/*active clipper in world coord system */
	GF_Rect clipper, layer_clipper;

	
#ifdef GPAC_RENDER_USE_CACHE
	Bool in_group_cache;
#endif


#ifndef GPAC_DISABLE_3D
	/*the current camera*/
	GF_Camera *camera;

	/*current object (model) transformation from top-level, view is NOT included*/
	GF_Matrix model_matrix;

	/*fog bind stack*/
	GF_List *fogs; /*holds fogs info*/
	/*navigation bind stack*/
	GF_List *navigations;

	/*when drawing, signals the mesh is transparent (enables blending)*/
	Bool mesh_is_transparent;
	/*when drawing, signals the mesh has texture*/
	Bool mesh_has_texture;

	/*bounds for TRAVERSE_GET_BOUNDS and background rendering*/
	GF_BBox bbox;

	/*cull flag (used to skip culling of children when parent bbox is completely inside/outside frustum)*/
	u32 cull_flag;

	/*toggle local lights on/off - field is ONLY valid in TRAVERSE_RENDER mode, and local lights
	are always set off in reverse order as when set on*/
	Bool local_light_on;
	/*current directional ligths contexts - only valid in TRAVERSE_RENDER*/
	GF_List *local_lights;

	/*clip planes in world coords*/
	GF_Plane clip_planes[MAX_USER_CLIP_PLANES];
	u32 num_clip_planes;


	/*layer traversal state:
		set to the first traversed layer3D when picking
		set to the current layer3D traversed when rendering 3D to an offscreen bitmap. This alows other 
			nodes (typically bindables) seting the layer dirty flags to force a redraw 
	*/
	GF_Node *layer3d;
#endif

} GF_TraverseState;

void render_visual_register(Render *sr, GF_VisualManager *);
void render_visual_unregister(Render *sr, GF_VisualManager *);
Bool render_visual_is_registered(Render *sr, GF_VisualManager *);

void render_node_init(GF_VisualRenderer *vr, GF_Node *node);
Bool render_node_changed(GF_VisualRenderer *vr, GF_Node *byObj);

void render_set_aspect_ratio_scaling(Render *sr, Fixed scaleX, Fixed scaleY);
Bool visual_get_size_info(GF_TraverseState *tr_state, Fixed *surf_width, Fixed *surf_height);
Bool render_pick_in_clipper(GF_TraverseState *tr_state, GF_Rect *clip);

Bool render_has_composite_texture(GF_Node *appear);
Bool render_composite_texture_handle_event(Render *sr, GF_Event *ev);
void render_adjust_composite_scale(GF_Node *node, Fixed *sx, Fixed *sy);

Bool render_execute_event(Render *sr, GF_TraverseState *tr_state, GF_Event *ev, GF_ChildNodeItem *children);
Bool render_handle_navigation(Render *sr, GF_Event *ev);


GF_Err render_2d_set_aspect_ratio(Render *sr);
void render_2d_set_user_transform(Render *sr, Fixed zoom, Fixed tx, Fixed ty, Bool is_resize) ;
GF_Err render_2d_get_video_access(GF_VisualManager *surf);
void render_2d_release_video_access(GF_VisualManager *surf);
Bool render_2d_pixel_format_supported(GF_VisualManager *surf, u32 pixel_format);
void render_2d_draw_bitmap(GF_VisualManager *surf, struct _gf_sr_texture_handler *txh, GF_IRect *clip, GF_Rect *unclip, u8 alpha, u32 *col_key, GF_ColorMatrix *cmat);
GF_Rect render_2d_update_clipper(GF_TraverseState *tr_state, GF_Rect this_clip, Bool *need_restore, GF_Rect *original, Bool for_layer);
Bool render_get_2d_plane_intersection(GF_Ray *ray, SFVec3f *res);


void render_gradient_update(GF_TextureHandler *txh);

#ifndef GPAC_DISABLE_3D

GF_Err render_3d_set_aspect_ratio(Render *sr);
GF_Camera *render_3d_get_camera(Render *sr);
void render_3d_reset_camera(Render *sr);
GF_Camera *render_layer3d_get_camera(GF_Node *node);
void render_layer3d_bind_camera(GF_Node *node, Bool do_bind, u32 nav_value);
void render_3d_draw_bitmap(struct _drawable *stack, DrawAspect2D *asp, GF_TraverseState *tr_state, Fixed width, Fixed height, Fixed bmp_scale_x, Fixed bmp_scale_y);

void render_load_opengl_extensions(Render *sr);

GF_Err render_3d_get_screen_buffer(Render *sr, GF_VideoSurface *fb);
GF_Err render_3d_release_screen_buffer(Render *sr, GF_VideoSurface *framebuffer);
#endif

GF_TextureHandler *render_get_texture_handler(GF_Node *n);
GF_TextureHandler *render_get_gradient_texture(GF_Node *node);

#ifndef GPAC_DISABLE_SVG
void render_svg_render_use(GF_Node *anim, GF_Node *sub_root, void *rs);

GF_TextureHandler *render_svg_get_image_texture(GF_Node *node);
GF_TextureHandler *render_svg_get_gradient_texture(GF_Node *node);
void render_svg_build_gradient_texture(GF_TextureHandler *txh);

void render_svg_node_list(GF_ChildNodeItem *children, GF_TraverseState *tr_state);

void render_svg_render_base(GF_Node *node, SVGAllAttributes *all_atts, GF_TraverseState *tr_state, SVGPropertiesPointers *backup_props, u32 *backup_flags);
void render_svg_apply_local_transformation(GF_TraverseState *tr_state, SVGAllAttributes *atts, GF_Matrix2D *backup_matrix_2d, GF_Matrix *backup_matrix);
void render_svg_restore_parent_transformation(GF_TraverseState *tr_state, GF_Matrix2D *backup_matrix_2d, GF_Matrix *backup_matrix);
Bool render_svg_is_display_off(SVGPropertiesPointers *props);
void render_svg_get_nodes_bounds(GF_Node *self, GF_ChildNodeItem *children, GF_TraverseState *tr_state);
u32 render_svg_focus_switch_ring(Render *sr, Bool move_prev);
u32 render_svg_focus_navigate(Render *sr, u32 key_code);

#endif


#endif	/*_RENDER_H_*/


