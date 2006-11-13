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

#ifndef VISUALSURFACE_H_
#define VISUALSURFACE_H_

#include "render3d.h"


/*when highspeeds, amount of subdivisions for bezier and ellipse will be devided by this factor*/
#define HIGH_SPEED_RATIO	2


typedef struct _visual_surface
{
	/*bindable stacks*/
	GF_List *back_stack;
	GF_List *view_stack;
	GF_List *navigation_stack;
	GF_List *fog_stack;

	/*renderer*/
	Render3D *render;
	/*surface size in pixels*/
	u32 width, height;

	/*the one and only camera associated with the surface*/
	GF_Camera camera;

	/*list of transparent nodes to draw after TRAVERSE_SORT pass*/
	GF_List *alpha_nodes_to_draw;

	/*lighting stuff*/
	u32 num_lights;
	u32 max_lights;
	/*cliping stuff*/
	u32 num_clips;
	u32 max_clips;
} VisualSurface;

VisualSurface *VS_New();
void VS_Delete(VisualSurface *surf);
/*setup effect for this surface (assign camera, setup viewpoint & stacks)*/
void VS_SetupEffects(VisualSurface *surface, RenderEffect3D *eff);
/*inits render: animate camera if needed, setup projection and render background*/
void VS_InitRender(RenderEffect3D *eff);
/*checks Viewpoint stack, updates camera and set surface matrices*/
void VS_SetupProjection(RenderEffect3D *eff);
/*shortcut to render the root node: inits render, performs collision, draw and clear lights*/
void VS_NodeRender(RenderEffect3D *eff, GF_Node *root_node);
/*shortcut to render composite surfaces: inits render, draw and clear lights*/
void VS_RootRenderChildren(RenderEffect3D *eff, GF_List *children);

/*called only by viewpoints to setup navigation modes - if vp is given the pixel_metrics flag is taken from the node
parent graph, otherwise from the render effect*/
void VS_ViewpointChange(RenderEffect3D *eff, GF_Node *vp, Bool animate_change, Fixed fieldOfView, SFVec3f position, SFRotation orientation, SFVec3f local_center_);
/*register a node on the surface for later drawing (eg z-sorting)*/
void VS_RegisterContext(RenderEffect3D *eff, GF_Node *shape, GF_BBox *bounds, Bool is_shape);
/*draws all nodes registered on the surface - this assumes modelview/projection has been setup before*/
void VS_FlushContexts(VisualSurface *surf, RenderEffect3D *eff);
/*main picker function: @node_list: if specified uses nodes in this list, otherwise uses scenegraph root*/
Bool VS_ExecuteEvent(VisualSurface *surf, RenderEffect3D *eff, GF_UserEvent *event, GF_List *node_list);
/*handle user collisions
@node_list: list of node to perform collision. If null, collision is done on scenegraph root*/
void VS_DoCollisions(RenderEffect3D *eff, GF_List *node_list);


/*base drawable stack:
	BASERENDERSTACK
	mesh: 3D object for drawable - all objects (2D and 3D) use this
	IntersectWithRay: checks wether ray and object intersect in local coord system
		may be NULL in which case default mesh routine is used (most 3D primitives do use it)
	ClosestFace: gets closest face on object that is less than min_dist from user_pos in local coord system
		may be NULL in which case default mesh routine is used (most 3D primitives do use it)
		
*/
typedef struct
{
	GF_Node *owner;
	GF_Renderer *compositor;
	GF_Mesh *mesh;
	Bool (*IntersectWithRay)(GF_Node *owner, GF_Ray *ray, SFVec3f *outPoint, SFVec3f *outNormal, SFVec2f *outTexCoords);
	Bool (*ClosestFace)(GF_Node *owner, SFVec3f user_pos, Fixed min_dist, SFVec3f *outPoint);
} DrawableStack;

#define	stack_setup(_object, _owner, _compositor)	\
		_object->owner = _owner;		\
		_object->compositor = _compositor; \
		_object->mesh = new_mesh(); \
		_object->IntersectWithRay = NULL;	\
		_object->ClosestFace = NULL;	\

#define	drawablestack_predestroy(_object) \
	if (_object->mesh) mesh_free(_object->mesh);	\


DrawableStack *new_drawable(GF_Node *owner, GF_Renderer *compositor);
void delete_drawable(DrawableStack *d);
/*can be used for Node_PreDestroy with all nodes using only a the base 3D stack*/
void drawable_Node_PreDestroy(GF_Node *n);
/*default setup for box/cylinder... - returns stack*/
DrawableStack *BaseDrawableStack(GF_Renderer *sr, GF_Node *node);

/*returns 1 if node is (partially) in view frustrum, false otherwise, and update effect cull flag
skip_near: if set doesn't cull against near plane (for collision detection)*/
Bool node_cull(RenderEffect3D *eff, GF_BBox *bbox, Bool skip_near);
/*check intersection of drawable (geometry) object & pick ray and queue as sensitive object if needed*/
void drawable_do_pick(GF_Node *n, RenderEffect3D *eff);
/*check collision of user and drawable (geometry) object*/
void drawable_do_collide(GF_Node *node, RenderEffect3D *eff);

/*stored at compositor level and in each 2D drawable node*/
typedef struct
{
	/*if set, mesh is the mesh version of the vectorial outline (triangles)
	otherwise, mesh is the line version of the path (lines)
	*/
	Bool is_vectorial;
	/*outline*/
	GF_Mesh *outline;
	GF_Node *lineProps;
	GF_Node *node2D;
	u32 last_update_time;
	/*we also MUST handle width changes due to scale transforms, most content is designed with width=cst 
	whatever the transformation*/
	Fixed line_scale;
	Bool has_texcoords;
} StrikeInfo;

void delete_strikeinfo(StrikeInfo *info);

/*base 2D stack - same + extensions for striking 
	ALL 2D NODES WITH STRIKE (eg, except bitmap and PS2D) SHALL USE IT
*/
typedef struct
{
	GF_Node *owner;
	GF_Renderer *compositor;
	GF_Mesh *mesh;
	Bool (*IntersectWithRay)(GF_Node *owner, GF_Ray *ray, SFVec3f *outPoint, SFVec3f *outNormal, SFVec2f *outTexCoords);
	Bool (*ClosestFace)(GF_Node *owner, SFVec3f user_pos, Fixed min_dist, SFVec3f *outPoint);
	GF_Path *path;
	GF_List *strike_list;
} stack2D;

typedef struct
{
	/*including alpha*/
	SFColor fill_color, line_color;
	Fixed alpha; /*1-transp*/
	Fixed line_alpha; /*1-transp*/
	Bool filled, is_scalable;
	GF_PenSettings pen_props;
	/*current modelview scale for line = max abs(scaleX), abs(scaleY) - not set if no line*/
	Fixed line_scale;

	/*cached lp node - do not touch*/
	GF_Node *lp;
	/*cached texture handler - do not touch*/
	GF_TextureHandler *txh;
	/*cached texture transform*/
	GF_Node *tx_trans;
} Aspect2D;
/*setup aspect - returns 0 if material used is a 3D one*/
Bool VS_GetAspect2D(RenderEffect3D *eff, Aspect2D *asp);
/*returns line width updated with view scale if needed*/
Fixed Aspect_GetLineWidth(Aspect2D *asp);

/*setup strike (check for texture)*/
void VS_Set2DStrikeAspect(VisualSurface *surf, Aspect2D *asp);

/*get strike info - recomputes outline if needed*/
StrikeInfo *VS_GetStrikeInfo(stack2D *st, Aspect2D *asp, RenderEffect3D *eff);
/*get strike info for IFS2D - reset outline if needed but doesn't recomputes it*/
StrikeInfo *VS_GetStrikeInfoIFS(stack2D *st, Aspect2D *asp, RenderEffect3D *eff);

stack2D *new_stack2D(GF_Node *owner, GF_Renderer *compositor);
void delete_stack2D(stack2D *d);
/*use to destroy stacks inheriting from stack2D - doesn't free structure. Must be called
by all 2D nodes*/
void stack2D_predestroy(stack2D *d);
/*can be used for Node_PreDestroy with all nodes using only a the base 2D stack*/
void stack2D_node_predestroy(GF_Node *n);
/*reset the path/dlist*/
void stack2D_reset(stack2D *d);
/*default setup for rect/ellipse/circle/etc. returns created stack*/
stack2D *BaseStack2D(GF_Renderer *sr, GF_Node *node);
/*setup and draw 2D primitive*/
void stack2D_draw(stack2D *d, RenderEffect3D *eff);
/*setup base 2D stack*/
void stack2D_setup(stack2D *st, GF_Renderer *sr, GF_Node *node);
/*setup appearance and draws mesh for 3D objects*/
void VS_DrawMesh(RenderEffect3D *eff, GF_Mesh *mesh);
/*setup appearance return 0 if material is fully transparent - exported for text only*/
Bool VS_SetupAppearance(RenderEffect3D *eff);

/*setup texture - return 0 if texture could not be setup (not available or error)*/
Bool VS_setup_texture(RenderEffect3D *eff);
/*setup texture in 2D nodes taking care of Material2D fill & transparency - returns NULL if texture not setup*/
GF_TextureHandler *VS_setup_texture_2d(RenderEffect3D *eff, Aspect2D *asp);
/*disable effect texture*/
void VS_disable_texture(RenderEffect3D *eff);


/*
	till end of file: all 3D specific calls
*/

/*setup surface (hint & co)*/
void VS3D_Setup(VisualSurface *surf);
/*turns depth buffer on/off*/
void VS3D_SetDepthBuffer(VisualSurface *surf, Bool on);
/*turns 2D AA on/off*/
void VS3D_SetAntiAlias(VisualSurface *surf, Bool bOn);
/*turns headlight on/off*/
void VS3D_SetHeadlight(VisualSurface *surf, Bool bOn, GF_Camera *cam);

enum
{
	/*lighting flag*/
	F3D_LIGHT = 1,
	/*blending flag*/
	F3D_BLEND = (1<<1),
	/*color (material3D) flag*/
	F3D_COLOR = (1<<2)
};

/*enable/disable one of the above feature*/
void VS3D_SetState(VisualSurface *surf, u32 flag_mask, Bool setOn);
/*clear surface with given color - alpha should only be specified for composite textures*/
void VS3D_ClearSurface(VisualSurface *surf, SFColor color, Fixed alpha);
/*clear depth*/
void VS3D_ClearDepth(VisualSurface *surf);

/*matrix mode types*/
enum
{
	MAT_MODELVIEW,
	MAT_PROJECTION,
	MAT_TEXTURE,
};
/*set current matrix type*/
void VS3D_SetMatrixMode(VisualSurface *surf, u32 mat_type);
/*push matrix stack*/
void VS3D_PushMatrix(VisualSurface *surf);
/*reset current matrix (identity)*/
void VS3D_ResetMatrix(VisualSurface *surf);
/*multiply current matrix with given matrix (16 coefs)*/
void VS3D_MultMatrix(VisualSurface *surf, Fixed *mat);
/*loads given matrix (16 coefs) as current one*/
void VS3D_LoadMatrix(VisualSurface *surf, Fixed *mat);
/*pop matrix stack*/
void VS3D_PopMatrix(VisualSurface *surf);

/*setup viewport (vp: top-left, width, height)*/
void VS3D_SetViewport(VisualSurface *surf, GF_Rect vp);
/*setup rectangular cliper (clip: top-left, width, height)
NOTE: 2D clippers can only be set from a 2D context, hence will always take the 4 first GL clip planes.
In order to allow multiple Layer2D in Layer2D, THERE IS ALWAYS AT MOST ONE 2D CLIPPER USED AT ANY TIME, 
it is the caller responsability to restore previous 2D clipers*/
void VS3D_SetClipper2D(VisualSurface *surf, GF_Rect clip);
/*remove 2D clipper*/
void VS3D_ResetClipper2D(VisualSurface *surf);
/*set clipping plane*/
void VS3D_SetClipPlane(VisualSurface *surf, GF_Plane p);
/*reset last clipping plane set*/
void VS3D_ResetClipPlane(VisualSurface *surf);

/*draw mesh*/
void VS3D_DrawMesh(RenderEffect3D *eff, GF_Mesh *mesh);
/*only used for ILS/ILS2D or IFS2D outline*/
void VS3D_StrikeMesh(RenderEffect3D *eff, GF_Mesh *mesh, Fixed width, u32 dash_style);

/*material types*/
enum
{
	/*default material*/
	MATERIAL_NONE,
	MATERIAL_AMBIENT,
	MATERIAL_DIFFUSE,
	MATERIAL_SPECULAR,
	MATERIAL_EMISSIVE,
};
/*set material*/
void VS3D_SetMaterial(VisualSurface *surf, u32 material_type, Fixed *rgba);
/*set shininess (between 0 and 1.0)*/
void VS3D_SetShininess(VisualSurface *surf, Fixed shininess);
/*set 2D material (eq to disable lighting and set material (none))*/
void VS3D_SetMaterial2D(VisualSurface *surf, SFColor col, Fixed alpha);

/*disables last light created - for directional lights only*/
void VS3D_RemoveLastLight(VisualSurface *surf);
/*disables all lights*/
void VS3D_ClearAllLights(VisualSurface *surf);
/*insert spot light - returns 0 if too many lights*/
Bool VS3D_AddSpotLight(VisualSurface *surf, Fixed ambientIntensity, SFVec3f attenuation, Fixed beamWidth, 
					   SFColor color, Fixed cutOffAngle, SFVec3f direction, Fixed intensity, SFVec3f location);
/*insert point light - returns 0 if too many lights*/
Bool VS3D_AddPointLight(VisualSurface *surf, Fixed ambientIntensity, SFVec3f attenuation, SFColor color, Fixed intensity, SFVec3f location);
/*insert directional light - returns 0 if too many lights*/
Bool VS3D_AddDirectionalLight(VisualSurface *surf, Fixed ambientIntensity, SFColor color, Fixed intensity, SFVec3f direction);
/*set fog*/
void VS3D_SetFog(VisualSurface *surf, const char *type, SFColor color, Fixed density, Fixed visibility);
/*fill given rect with given color (used for text hilighting only) - context shall not be altered*/
void VS3D_FillRect(VisualSurface *surf, GF_Rect rc, SFColorRGBA color);

/*non-oglES functions*/
#ifndef GPAC_USE_OGL_ES

/*draws image data:
	pos_x, pos_y: top-left pos of image
	width, height: size of image
	pixelformat: image pixel format
	data: image data
	scale_x, scale_y: x & y scale
*/
void VS3D_DrawImage(VisualSurface *surf, Fixed pos_x, Fixed pos_y, u32 width, u32 height, u32 pixelformat, char *data, Fixed scale_x, Fixed scale_y);
/*get matrix for the desired mode*/
void VS3D_GetMatrix(VisualSurface *surf, u32 mat_type, Fixed *mat);
/*X3D hatching*/
void VS3D_HatchMesh(RenderEffect3D *eff, GF_Mesh *mesh, u32 hatchStyle, SFColor hatchColor);

#endif


/*tesselation functions*/
/*appends given face (and tesselate if needed) to the mesh. Only vertices are used in the face
indices are ignored. 
partially implemented on ogl-ES*/
void TesselateFaceMesh(GF_Mesh *mesh, GF_Mesh *face);

#ifndef GPAC_USE_OGL_ES
/*converts 2D path into a polygon - these are only partially implemented when using oglES
for_outline:
	 0, regular odd/even windining rule with texCoords
	 1, zero-non-zero windining rule without texCoords
	 2, zero-non-zero windining rule with texCoords
*/
void TesselatePath(GF_Mesh *mesh, GF_Path *path, u32 outline_style);

/*appends given face (and tesselate if needed) to the mesh. Only vertices are used in the face
indices are ignored. 
Same as TesselateFaceMesh + faces info to determine where are the polygons in the face - used by extruder only
*/
void TesselateFaceMeshComplex(GF_Mesh *dest, GF_Mesh *orig, u32 nbFaces, u32 *ptsPerFaces);

#endif



#endif

