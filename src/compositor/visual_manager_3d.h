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

#ifndef _VISUAL_MANAGER_3D_
#define _VISUAL_MANAGER_3D_

#include <gpac/internal/compositor_dev.h>

#ifndef GPAC_DISABLE_3D

//lowering down number of glClips, only used for non window-aligned clip planes, otherwise scisor test used
#define GF_MAX_GL_CLIPS	2
#define GF_MAX_GL_LIGHTS 4

/*
*	Visual 3D functions
*/

/*draw frame, performing collisions, camera displacement and drawing*/
Bool visual_3d_draw_frame(GF_VisualManager *visual, GF_Node *root, GF_TraverseState *tr_state, Bool is_root_visual);

Bool visual_3d_setup_ray(GF_VisualManager *visual, GF_TraverseState *tr_state, s32 ix, s32 iy);

/*traverse the scene and picks the node under the current ray, if any*/
void visual_3d_pick_node(GF_VisualManager *visual, GF_TraverseState *tr_state, GF_Event *ev, GF_ChildNodeItem *children);

/*checks a bounding box against the visual frustum. Returns true if box is visible, false otherwise.
The cull_flag of the traversing state is updated to the box/frustum relation (in/out/intersect)*/
Bool visual_3d_node_cull(GF_TraverseState *tr_state, GF_BBox *bbox, Bool skip_near);

/*modify a viewpoint*/
void visual_3d_viewpoint_change(GF_TraverseState *tr_state, GF_Node *vp, Bool animate_change, Fixed fieldOfView, SFVec3f position, SFRotation orientation, SFVec3f local_center);

#ifndef GPAC_DISABLE_VRML
/*checks if a 3D mesh or a 2D path is under the current ray. Updates hit info if so.*/
void visual_3d_vrml_drawable_pick(GF_Node *n, GF_TraverseState *tr_state, GF_Mesh *mesh, Drawable *drawable) ;
/*performs collision on the given node (2D or 3D object)*/
void visual_3d_vrml_drawable_collide(GF_Node *node, GF_TraverseState *tr_state);
#endif


/*register a geomery node for drawing. If the node is transparent, stacks it for later draw, otherwise draw it directly*/
void visual_3d_register_context(GF_TraverseState *tr_state, GF_Node *node_to_draw);
/*flushes (draw) all pending transparent nodes*/
void visual_3d_flush_contexts(GF_VisualManager *visual, GF_TraverseState *tr_state);


/*draws a 3D object, setting up material and texture*/
void visual_3d_draw(GF_TraverseState *tr_state, GF_Mesh *mesh);
/*draws a 2D object, setting up material and texture with specified 2D aspect*/
void visual_3d_draw_2d_with_aspect(Drawable *st, GF_TraverseState *tr_state, DrawAspect2D *asp);
/*draws a 2D SVG object - the DrawableContext MUST be set in the traversing state*/
void visual_3d_draw_from_context(DrawableContext *ctx, GF_TraverseState *tr_state);

#ifndef GPAC_DISABLE_VRML
/*draws a 2D VRML object, setting up material and texture*/
void visual_3d_draw_2d(Drawable *st, GF_TraverseState *tr_state);
#endif


/*sets 2D strike aspect
	- exported for text drawing*/
void visual_3d_set_2d_strike(GF_TraverseState *tr_state, DrawAspect2D *asp);
/*sets 3D material. Returns false is object is not visible due to appearance
	- exported for text drawing*/
Bool visual_3d_setup_appearance(GF_TraverseState *tr_state);
/*sets 3D texture. Returns true if a texture is found and successfully bound or no texture found, FALSE otherwise (texture failure)
	- exported for text drawing*/
Bool visual_3d_setup_texture(GF_TraverseState *tr_state, Fixed diffuse_alpha);
/*disables texture
	- exported for text drawing*/
void visual_3d_disable_texture(GF_TraverseState *tr_state);

/*check for collisions on a given node or on a list of nodes*/
void visual_3d_check_collisions(GF_TraverseState *tr_state, GF_Node *root_node, GF_ChildNodeItem *node_list);

/*init drawing pass - exported for Layer3D
	@layer_type:
		0: not a layer
		1: 3D layer in 3D context, depth clear but no color clear
		2: 3D layer in 2D context (offscreen rendering), depth and color clear with alpha=0
*/
void visual_3d_init_draw(GF_TraverseState *tr_state, u32 layer_type);
/*setup projection - exported for Layer3D */
void visual_3d_setup_projection(GF_TraverseState *tr_state, Bool is_layer);


/*base 3D drawable*/
typedef struct
{
	/*3D object for drawable if needed - ALLOCATED BY DEFAULT*/
	GF_Mesh *mesh;

} Drawable3D;

/*generic Drawable3D constructor*/
Drawable3D *drawable_3d_new(GF_Node *node);
/*generic Drawable3D destructor*/
void drawable_3d_del(GF_Node *n);

void drawable_3d_base_traverse(GF_Node *n, void *rs, Bool is_destroy, void (*build_shape)(GF_Node*,Drawable3D *,GF_TraverseState *) );

void drawable3d_check_focus_highlight(GF_Node *node, GF_TraverseState *tr_state, GF_BBox *orig_bounds);

typedef struct
{
	/*the directional light*/
	GF_Node *dlight;
	/*light matrix*/
	GF_Matrix light_matrix;
} DirectionalLightContext;

typedef struct
{
	/*the one and only geometry node to draw*/
	GF_Node *geometry;
	GF_Node *appearance;
	/*model matrix at this node*/
	GF_Matrix model_matrix;
	/*current color transformation*/
	GF_ColorMatrix color_mat;
	/*1-based idx of text element drawn*/
	u32 text_split_idx;
	/*needed for bitmap*/
	Bool pixel_metrics;
	/*cull flag - needed for AABB tree culling in case object is 100% inside frustum*/
	u32 cull_flag;

	/*directional lights at this node*/
	GF_List *directional_lights;
	/*z-depth for sorting*/
	Fixed zmax;

	/*clipper in world coords*/
	GF_Rect clipper;
	Bool has_clipper;

	/*clip planes in world coords*/
	GF_Plane clip_planes[MAX_USER_CLIP_PLANES];
	u32 num_clip_planes;

#ifdef GF_SR_USE_DEPTH
	Fixed depth_offset;
#endif
} Drawable3DContext;


typedef struct
{
	//0: directional - 1: spot - 2: point
	u32 type;
	SFVec3f direction, position, attenuation;
	Fixed ambientIntensity, intensity, beamWidth, cutOffAngle;
	SFColor color;
	GF_Matrix light_mx;
} GF_LightInfo;


typedef struct
{
	GF_Plane p;
	Bool is_2d_clip;
	GF_Matrix *mx_clipper;
} GF_ClipInfo;


/*
	till end of file: all 3D specific calls.
*/

/*setup visual (hint & co)*/
void visual_3d_setup(GF_VisualManager *visual);
/*turns depth buffer on/off*/
void visual_3d_enable_depth_buffer(GF_VisualManager *visual, Bool on);
/*turns 2D AA on/off*/
void visual_3d_enable_antialias(GF_VisualManager *visual, Bool bOn);
/*turns headlight on/off*/
void visual_3d_enable_headlight(GF_VisualManager *visual, Bool bOn, GF_Camera *cam);

enum
{
	/*lighting flag*/
	V3D_STATE_LIGHT = 1,
	/*blending flag*/
	V3D_STATE_BLEND = (1<<1),
	/*color (material3D) flag*/
	V3D_STATE_COLOR = (1<<2)
};

/*enable/disable one of the above feature*/
void visual_3d_set_state(GF_VisualManager *visual, u32 flag_mask, Bool setOn);
/*clear visual with given color - alpha should only be specified for composite textures and Layer3D*/
void visual_3d_clear(GF_VisualManager *visual, SFColor color, Fixed alpha);
/*clear depth*/
void visual_3d_clear_depth(GF_VisualManager *visual);

/*turns background state on/off. When on, all quality options are disabled in order to draw as fast as possible*/
void visual_3d_set_background_state(GF_VisualManager *visual, Bool on);


void visual_3d_set_texture_matrix(GF_VisualManager *visual, GF_Matrix *mx);
//depending on platforms / GL rendering mode, the projection matrix is not loaded at each object Draw(). This function is used to notify a change
//in the projection matrix to force a reload of the camera's projection
void visual_3d_projection_matrix_modified(GF_VisualManager *visual);


/*setup viewport (vp: top-left, width, height)*/
void visual_3d_set_viewport(GF_VisualManager *visual, GF_Rect vp);

/*setup scissors region (vp: top-left, width, height) - if vp is NULL, disables scissors*/
void visual_3d_set_scissor(GF_VisualManager *visual, GF_Rect *vp);

/*setup rectangular cliper (clip: top-left, width, height)
NOTE: 2D clippers can only be set from a 2D context, and will always use glScissor.
In order to allow multiple Layer2D in Layer2D, THERE IS ALWAYS AT MOST ONE 2D CLIPPER USED AT ANY TIME,
it is the caller responsibility to restore previous 2D clipers

the matrix is not copied, care should be taken to keep it unmodified until the cliper is reset (unless desired otherwise)
if NULL, no specific clipping transform will be used*/
void visual_3d_set_clipper_2d(GF_VisualManager *visual, GF_Rect clip, GF_Matrix *mx_at_clipper);
/*remove 2D clipper*/
void visual_3d_reset_clipper_2d(GF_VisualManager *visual);

/*set clipping plane
the matrix is not copied, care should be taken to keep it unmodified until the cliper is reset (unless desired otherwise)
if NULL, no specific clipping transform will be used*/
void visual_3d_set_clip_plane(GF_VisualManager *visual, GF_Plane p, GF_Matrix *mx_at_clipper, Bool is_2d_clip);

/*reset last clipping plane set*/
void visual_3d_reset_clip_plane(GF_VisualManager *visual);

/*draw mesh*/
void visual_3d_mesh_paint(GF_TraverseState *tr_state, GF_Mesh *mesh);
/*only used for ILS/ILS2D and IFS2D/Text outline*/
void visual_3d_mesh_strike(GF_TraverseState *tr_state, GF_Mesh *mesh, Fixed width, Fixed line_scale, u32 dash_style);

/*material types*/
enum
{
	V3D_MATERIAL_AMBIENT=0,
	V3D_MATERIAL_DIFFUSE,
	V3D_MATERIAL_SPECULAR,
	V3D_MATERIAL_EMISSIVE,
};
/*set material*/
void visual_3d_set_material(GF_VisualManager *visual, u32 material_type, Fixed *rgba);
/*set shininess (between 0 and 1.0)*/
void visual_3d_set_shininess(GF_VisualManager *visual, Fixed shininess);
/*set 2D material (eq to disable lighting and set material (none))*/
void visual_3d_set_material_2d(GF_VisualManager *visual, SFColor col, Fixed alpha);

/*set 2D material (eq to disable lighting and set material (none))*/
void visual_3d_set_material_2d_argb(GF_VisualManager *visual, u32 col);

/*disables last light created - for directional lights only*/
void visual_3d_remove_last_light(GF_VisualManager *visual);
/*disables all lights*/
void visual_3d_clear_all_lights(GF_VisualManager *visual);
/*insert spot light - returns 0 if too many lights*/
Bool visual_3d_add_spot_light(GF_VisualManager *visual, Fixed ambientIntensity, SFVec3f attenuation, Fixed beamWidth,
                              SFColor color, Fixed cutOffAngle, SFVec3f direction, Fixed intensity, SFVec3f location, GF_Matrix *light_mx);
/*insert point light - returns 0 if too many lights*/
Bool visual_3d_add_point_light(GF_VisualManager *visual, Fixed ambientIntensity, SFVec3f attenuation, SFColor color, Fixed intensity, SFVec3f location, GF_Matrix *light_mx);
/*insert directional light - returns 0 if too many lights. If light_mx is null, this is the headlight*/
Bool visual_3d_add_directional_light(GF_VisualManager *visual, Fixed ambientIntensity, SFColor color, Fixed intensity, SFVec3f direction, GF_Matrix *light_mx);

void visual_3d_has_inactive_light(GF_VisualManager *visual);

/*set fog*/
void visual_3d_set_fog(GF_VisualManager *visual, const char *type, SFColor color, Fixed density, Fixed visibility);
/*fill given rect with given color (used for text highlighting only) - context shall not be altered*/
void visual_3d_fill_rect(GF_VisualManager *visual, GF_Rect rc, SFColorRGBA color);

void visual_3d_point_sprite(GF_VisualManager *visual, Drawable *stack, GF_TextureHandler *txh, GF_TraverseState *tr_state);

/*non-oglES functions*/
#ifndef GPAC_USE_GLES1X
/*X3D hatching*/
void visual_3d_mesh_hatch(GF_TraverseState *tr_state, GF_Mesh *mesh, u32 hatchStyle, SFColor hatchColor);
#endif


void visual_3d_draw_bbox(GF_TraverseState *tr_state, GF_BBox *box, Bool is_debug);


GF_Err visual_3d_init_autostereo(GF_VisualManager *visual);
void visual_3d_end_auto_stereo_pass(GF_VisualManager *visual);

void visual_3d_init_shaders(GF_VisualManager *visual);
void visual_3d_reset_graphics(GF_VisualManager *visual);

void visual_3d_clean_state(GF_VisualManager *visual);

#endif /*GPAC_DISABLE_3D*/


#endif	/*_VISUAL_MANAGER_3D_*/

