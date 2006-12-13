/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / 3D rendering module
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

#ifndef RENDER3D_H
#define RENDER3D_H

#include <gpac/nodes_mpeg4.h>
#include <gpac/nodes_x3d.h>

#include "camera.h"
#include "mesh.h"

/*used with GL only*/
typedef struct 
{
	Bool multisample;
	Bool bgra_texture;
	Bool abgr_texture;
	Bool npot_texture;
	Bool rect_texture;
} HardwareCaps;


typedef struct
{
	/*world->local and local->world transform at hit point*/
	GF_Matrix world_to_local, local_to_world;
	/*hit point in local coord & world coord*/
	SFVec3f local_point, world_point;
	/*stored world ray - this is needed because of layer3D which defines a complete new world coord system*/
	GF_Ray world_ray;
	/*normal at hit point, local coord system*/
	SFVec3f hit_normal;
	/*tex coords at hit point*/
	SFVec2f hit_texcoords;
	/*appearance at hit point - used for composite texture*/
	GF_Node *appear;
} RayHitInfo;

typedef struct _render3d
{
	/*remember main renderer*/
	GF_Renderer *compositor;
	/*all outlines cached*/
	GF_List *strike_bank;
	/*main 3D surface*/
	struct _visual_surface *surface;
	Bool main_surface_setup;

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

	/*top level effect for zoom/pan*/
	struct _render3d_effect *top_effect;

	/*current output info: screen size and top-left point of video surface, and current scaled scene size*/
	u32 out_width, out_height, out_x, out_y, cur_width, cur_height;
	/*needed for bitmap scaling when using DrawPixels rather than texture*/
	Fixed scale_x, scale_y;

	Bool poly_aa;
	u32 wiremode;
	u32 collide_mode;
	Bool gravity_on;
	/*unit box (1.0 size) and unit sphere (1.0 radius)*/
	GF_Mesh *unit_bbox;

	/*type of main display - this is used for user navigation*/
	Bool root_is_3D;

	/*sensitive node storage*/
	Bool is_grabbed;
	GF_List *sensors, *prev_sensors;
	GF_Node *picked;
	RayHitInfo hit_info;
	/*distance from ray origin used to discards further hits - FIXME: may not properly work with transparent layer3D*/
	Fixed sq_dist;
	/*last cursor type user display*/
	u32 last_cursor;
	void *hs_grab;

	/*active layer3D for layer navigation - may be NULL*/
	GF_Node *active_layer;

	Fixed grab_x, grab_y;
	u32 nav_is_grabbed;

	/*specific hw caps (openGL for now)*/
	HardwareCaps hw_caps;

} Render3D;

/*sensor node handler - this is not defined as a stack because Anchor is both a grouping node and a 
sensor node, and we DO need the groupingnode stack...*/
typedef struct _sensor3D_handler
{
	/*sensor enabled or not ?*/
	Bool (*IsEnabled)(GF_Node *node);
	/*user input on sensor:
	is_over: pointing device is over a shape the sensor is attached to
	evt_type: mouse event type
	hit_info: current hit info
	*/
	void (*OnUserEvent)(struct _sensor3D_handler *sh, Bool is_over, GF_Event *ev, RayHitInfo *hit_info);
	/*set the node pointer here*/
	GF_Node *owner;
} SensorHandler;

SensorHandler *r3d_get_sensor_handler(GF_Node *n);

#define MAX_USER_CLIP_PLANES		4

/*rendering modes*/
enum
{
	/*when set objects are drawn as soon as traversed, at each frame - THIS MODE IS AND WILL ALWAYS BE 0
	only called on nodes appearing on screen*/
	TRAVERSE_RENDER = 0,
	/*main prerender routine - needed for true alpha compositing - depth sorting and culling is done there*/
	TRAVERSE_SORT,
	/*set global lights on. Since the model_matrix is not pushed to the target surface in this 
	pass, global lights shall not forget to do it (cf lighting.c)*/
	TRAVERSE_LIGHTING,
	/*picking mode*/
	TRAVERSE_PICK,
	/*collide mode*/
	TRAVERSE_COLLIDE,
	/*recomputes bounds - result shall be stored in effect bbox field*/
	TRAVERSE_GET_BOUNDS,
	/*set to signal bindable render - only called on bindable stack top if present.
	for background (drawing request), viewports/viewpoints fog and navigation (setup)
	all other nodes ARE NEVER SNED THIS EVENT
	*/
	TRAVERSE_RENDER_BINDABLE,
};

enum
{
	/*the subtree indicates no cull shall be performed*/
	TF_DONT_CULL = (1<<2),
};

/*the traversing context: set_up at top-level and passed through SFNode_Render*/
typedef struct _render3d_effect
{
	BASE_EFFECT_CLASS

	u32 traversing_mode;

	/*the one and only surface currently traversed*/
	struct _visual_surface *surface;
	/*the current camera*/
	GF_Camera *camera;
	/*is graph traversed in pixel metrics*/
	Bool is_pixel_metrics;
	/*half of min (width, height) of current graph traversed. If graph has no size info
	the size is inherited from parent graph*/
	Fixed min_hsize;

	/*current object (model) transformation from top-level, view is NOT included*/
	GF_Matrix model_matrix;
	/*current color transformation from top-level*/
	GF_ColorMatrix color_mat;

	/*current appearance when traversing geometry nodes*/
	GF_Node *appear;
	/*parent group for composition: can be Form, Layout or Layer2D*/
	struct _parent_group *parent;

	/*current bindable stacks*/
	GF_List *backgrounds;	/*holds 2D and 3D backgrounds*/
	GF_List *viewpoints; /*holds viewpoints (3D) and viewports (2D)*/
	GF_List *fogs; /*holds fogs info*/
	GF_List *navigations; /*holds navigation info*/

	/*bounds for TRAVERSE_GET_BOUNDS*/
	GF_BBox bbox;
	/*cull flag (used to skip culling of children when parent bbox is completely inside/outside frustum)*/
	u32 cull_flag;

	/*text splitting*/
	u32 text_split_mode;
	/*1-based idx of text element drawn*/
	u32 split_text_idx;

	/*all sensors for the current level - only valid in TRAVERSE_SORT*/
	GF_List *sensors;

	/*ray used for TRAVERSE_PICK*/
	GF_Ray ray;

	/*toggle local lights on/off - field is ONLY valid in TRAVERSE_RENDER mode, and local lights
	are always set off in reverse order as when set on*/
	Bool local_light_on;

	/*current directional ligths contexts - only valid in TRAVERSE_SORT*/
	GF_List *local_lights;

	/*we have 2 clippers, one for regular clipping (layout, form if clipping) which is maintained in world coord system
	and one for layer2D which is maintained in parent coord system (cf layer rendering). The layer clipper
	is used only when cascading layers - layer3D doesn't use clippers*/
	Bool has_clip, has_layer_clip;
	/*active clipper in world coord system */
	GF_Rect clipper, layer_clipper;

	/*set to the first traversed layers when picking*/
	GF_Node *collect_layer;

	Bool mesh_is_transparent, mesh_has_texture;

	/*clip planes in world coords*/
	GF_Plane clip_planes[MAX_USER_CLIP_PLANES];
	u32 num_clip_planes;
} RenderEffect3D;

RenderEffect3D *effect3d_new();
void effect3d_reset(RenderEffect3D *eff);
void effect3d_delete(RenderEffect3D *eff);

/*get current surface size in scene metrics (eg, pixel or meter) and returns 1 if pixelMetrics, 0 otherwise*/
Bool R3D_GetSurfaceSizeInfo(RenderEffect3D *eff, Fixed *surf_width, Fixed *surf_height);

void R3D_LoadExtensions(Render3D *sr);
void R3D_SensorDeleted(GF_Renderer *rend, SensorHandler *hdl);
void R3D_SetGrabbed(GF_Renderer *rend, Bool bOn);

void R3D_LinePropsRemoved(Render3D *sr, GF_Node *n);
Bool R3D_NodeChanged(GF_VisualRenderer *vr, GF_Node *byObj);
void R3D_NodeInit(GF_VisualRenderer *vr, GF_Node *node);


/*user vp modifs*/
Bool R3D_HandleUserEvent(Render3D *sr, GF_Event *event);
void R3D_ResetCamera(Render3D *sr);

GF_TextureHandler *R3D_GetTextureHandler(GF_Node *n);



GF_Err tx_allocate(GF_TextureHandler *txh);
void tx_delete(GF_TextureHandler *txh);
GF_Err R3D_SetTextureData(GF_TextureHandler *hdl);
void R3D_TextureHWReset(GF_TextureHandler *hdl);
Bool tx_enable(GF_TextureHandler *txh, GF_Node *tx_transform);
void tx_disable(GF_TextureHandler *txh);
char *tx_get_data(GF_TextureHandler *txh, u32 *pix_format);
Bool tx_needs_reload(GF_TextureHandler *hdl);
Bool tx_check_bitmap_load(GF_TextureHandler *hdl);
void tx_copy_to_texture(GF_TextureHandler *txh);
Bool tx_setup_format(GF_TextureHandler *txh);
Bool tx_set_image(GF_TextureHandler *txh, Bool generate_mipmaps);
/*gets texture transform matrix - returns 1 if not identity
@tx_transform: texture transform node from appearance*/
Bool tx_get_transform(GF_TextureHandler *txh, GF_Node *tx_transform, GF_Matrix *mx);

Bool tx_is_transparent(GF_TextureHandler *txh);

/*set blending mode*/
enum
{
	TX_DECAL = 0,
	TX_MODULATE,
	TX_REPLACE,
	TX_BLEND,
};
void tx_set_blend_mode(GF_TextureHandler *txh, u32 mode);

/*returns 1 if intersection of ray and plane z=0, storinf intersection point. returns 0 otherwise*/
Bool R3D_Get2DPlaneIntersection(GF_Ray *ray, SFVec3f *res);
/*returns 1 if pick ray is in clipper, 0 otherwise*/
Bool R3D_PickInClipper(RenderEffect3D *eff, GF_Rect *clip);
/*update the effect clipper with the given clipper in local coords system. The return value
is the clipper in the local coord system. The effect clipper is ALWAYS retranslated to world coord
@for_layer: indcates the update concerns layer clippers, not regular ones*/
GF_Rect R3D_UpdateClipper(RenderEffect3D *eff, GF_Rect this_clip, Bool *need_restore, GF_Rect *original, Bool for_layer);


/*returns 1 if node is a light source. @local_only: only returns 1 if local source (which only lights its parent sub-graph)*/
Bool r3d_is_light(GF_Node *n, Bool local_only);



typedef struct 
{
	/*the directional light*/
	GF_Node *dlight;
	/*light matrix*/
	GF_Matrix light_matrix;
} DLightContext;

typedef struct
{
	/*the one and only node to draw. Can be
		1 - a Shape (to keep track of texturing and appearance) most of the time
		2 - a LayerND which is rendered as a single element, without depth sorting. The layer is then 
		responsible of applying clipping & co
	*/
	GF_Node *node_to_draw;
	/*model matrix at this node*/
	GF_Matrix model_matrix;
	/*current color transformation*/
	GF_ColorMatrix color_mat;
	/*1-based idx of text element drawn*/
	u32 split_text_idx;
	/*needed for bitmap*/
	Bool is_pixel_metrics;
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
} Draw3DContext;


#endif

