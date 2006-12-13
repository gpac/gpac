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

#ifndef RENDER3D_NODES_H
#define RENDER3D_NODES_H

#include "visual_surface.h"

/*unregister bindable from all bindable stacks listed, and bind their top if needed*/
void PreDestroyBindable(GF_Node *bindable, GF_List *stack_list);
/*returns isBound*/
Bool Bindable_GetIsBound(GF_Node *bindable);
/*sets isBound*/
void Bindable_SetIsBound(GF_Node *bindable, Bool val);
/*generic on_set_bind for all bindables*/
void Bindable_OnSetBind(GF_Node *bindable, GF_List *stack_list);
/*remove all bindable references to this stack and destroy chain*/
void BindableStackDelete(GF_List *stack);
/*for user-modif of viewport/viewpoint*/
void Bindable_SetSetBind(GF_Node *bindable, Bool val);

/*exported stacks for bindable*/
/*viewpoint/viewport/navigation/fog stack*/
typedef struct
{
	GF_Node *owner;
	GF_Renderer *compositor;
	/*keep track of viewport stacks using us*/
	GF_List *reg_stacks;
	Bool prev_was_bound;
	/*world transform*/
	GF_Matrix world_view_mx;
} ViewStack;

typedef struct
{
	GF_Node *owner;
	GF_Renderer *compositor;
	/*for image background*/
	GF_TextureHandler txh;
	/*keep track of background stacks using us*/
	GF_List *reg_stacks;
	GF_Mesh *mesh;
	GF_BBox prev_bounds;
} Background2DStack;
void R3D_InitBackground2D(Render3D *sr, GF_Node *node);
void R3D_Background2DModified(GF_Node *node);

typedef struct
{
	GF_Node *owner;
	GF_Renderer *compositor;
	/*keep track of background stacks using us*/
	GF_List *reg_stacks;
	GF_Mesh *sky_mesh, *ground_mesh;
	MFFloat sky_col, ground_col;
	MFFloat sky_ang, ground_ang;

	GF_Mesh *front_mesh, *back_mesh, *top_mesh, *bottom_mesh, *left_mesh, *right_mesh;
	GF_TextureHandler txh_front, txh_back, txh_top, txh_bottom, txh_left, txh_right;
} BackgroundStack;
void R3D_InitBackground(Render3D *sr, GF_Node *node);
void R3D_BackgroundModified(GF_Node *node);


void R3D_InitAnchor(Render3D *sr, GF_Node *node);
SensorHandler *r3d_anchor_get_handler(GF_Node *n);
void R3D_InitBillboard(Render3D *sr, GF_Node *node);
void R3D_InitBitmap(Render3D *sr, GF_Node *node);
void R3D_InitBox(Render3D *sr, GF_Node *node);
void R3D_InitCircle(Render3D *sr, GF_Node *node);
void R3D_InitCollision(Render3D *sr, GF_Node *node);
void R3D_InitColorTransform(Render3D *sr, GF_Node *node);
void R3D_InitCompositeTexture2D(Render3D *sr, GF_Node *node);
void R3D_InitCompositeTexture3D(Render3D *sr, GF_Node *node);
GF_TextureHandler *r3d_composite_get_texture(GF_Node *node);
/*returns true if appearance has a composite texture*/
Bool r3d_has_composite_texture(GF_Node *appear);
/*handle event when hit object has a composite texture*/
Bool r3d_handle_composite_event(Render3D *sr, GF_Event *ev);
/*updates scaling if composite texture has been rescaled for HW support (need for bitamp)*/
void R3D_CompositeAdjustScale(GF_Node *node, Fixed *sx, Fixed *sy);
void R3D_InitCone(Render3D *sr, GF_Node *node);
void R3D_InitCurve2D(Render3D *sr, GF_Node *node);
void R3D_InitCylinder(Render3D *sr, GF_Node *node);
SensorHandler *r3d_cs_get_handler(GF_Node *n);
void R3D_InitCylinderSensor(Render3D *sr, GF_Node *node);
void R3D_InitDirectionalLight(Render3D *sr, GF_Node *node);
void R3D_InitDiscSensor(Render3D *sr, GF_Node *node);
SensorHandler *r3d_ds_get_handler(GF_Node *n);
void R3D_InitElevationGrid(Render3D *sr, GF_Node *node);
void R3D_InitEllipse(Render3D *sr, GF_Node *node);
void R3D_InitExtrusion(Render3D *sr, GF_Node *node);
void R3D_InitFog(Render3D *sr, GF_Node *node);
void R3D_InitForm(Render3D *sr, GF_Node *node);
void R3D_InitGroup(Render3D *sr, GF_Node *node);
void R3D_InitIFS2D(Render3D *sr, GF_Node *node);
void R3D_InitILS2D(Render3D *sr, GF_Node *node);
void R3D_InitIFS(Render3D *sr, GF_Node *node);
void R3D_InitILS(Render3D *sr, GF_Node *node);
void R3D_InitLinearGradient(Render3D *sr, GF_Node *node);
GF_TextureHandler *r3d_lg_get_texture(GF_Node *node);
void R3D_InitLayer2D(Render3D *sr, GF_Node *node);
void R3D_InitLayer3D(Render3D *sr, GF_Node *node);
/*returns associated camera*/
GF_Camera *l3d_get_camera(GF_Node *node);
/*(un)bind layer navinfo stack top - if no top overrides navigation_mode*/
void l3d_bind_camera(GF_Node *node, Bool do_bind, u32 nav_value);
void R3D_InitLayout(Render3D *sr, GF_Node *node);
void R3D_LayoutModified(GF_Node *node);
void R3D_InitLOD(Render3D *sr, GF_Node *node);
void R3D_InitNavigationInfo(Render3D *sr, GF_Node *node);
void R3D_InitOrderedGroup(Render3D *sr, GF_Node *node);
void R3D_InitPathLayout(Render3D *sr, GF_Node *node);
void R3D_InitPlaneSensor2D(Render3D *sr, GF_Node *node);
SensorHandler *r3d_ps2D_get_handler(GF_Node *n);
void R3D_InitPlaneSensor(Render3D *sr, GF_Node *node);
SensorHandler *r3d_ps_get_handler(GF_Node *n);
void R3D_InitPointLight(Render3D *sr, GF_Node *node);
void R3D_InitPointSet2D(Render3D *sr, GF_Node *node);
void R3D_InitPointSet(Render3D *sr, GF_Node *node);
void R3D_InitProximitySensor2D(Render3D *sr, GF_Node *node);
SensorHandler *r3d_prox2D_get_handler(GF_Node *n);
void R3D_InitProximitySensor(Render3D *sr, GF_Node *node);
void R3D_InitRadialGradient(Render3D *sr, GF_Node *node);
GF_TextureHandler *r3d_rg_get_texture(GF_Node *node);
void R3D_InitRectangle(Render3D *sr, GF_Node *node);
void R3D_InitShape(Render3D *sr, GF_Node *node);
void R3D_InitSound2D(Render3D *sr, GF_Node *node);
void R3D_InitSound(Render3D *sr, GF_Node *node);
void R3D_InitSphere(Render3D *sr, GF_Node *node);
void R3D_InitSphereSensor(Render3D *sr, GF_Node *node);
SensorHandler *r3d_sphere_get_handler(GF_Node *n);
void R3D_InitSpotLight(Render3D *sr, GF_Node *node);
void R3D_InitSwitch(Render3D *sr, GF_Node *node);
void R3D_InitText(Render3D *sr, GF_Node *node);
void Text_Extrude(GF_Node *node, RenderEffect3D *eff, GF_Mesh *mesh, MFVec3f *thespine, Fixed creaseAngle, Bool begin_cap, Bool end_cap, MFRotation *spine_ori, MFVec2f *spine_scale, Bool txAlongSpine);;
void R3D_InitTouchSensor(Render3D *sr, GF_Node *node);
SensorHandler *r3d_touch_sensor_get_handler(GF_Node *n);
void R3D_InitTransform2D(Render3D *sr, GF_Node *node);
void R3D_InitTransformMatrix2D(Render3D *sr, GF_Node *node);
void TM2D_GetMatrix(GF_Node *n, GF_Matrix *mx);
void R3D_InitTransform(Render3D *sr, GF_Node *node);
void R3D_InitVisibilitySensor(Render3D *sr, GF_Node *node);
void R3D_InitViewport(Render3D *sr, GF_Node *node);
void R3D_InitViewpoint(Render3D *sr, GF_Node *node);
void R3D_InitNonLinearDeformer(Render3D *sr, GF_Node *node);


typedef struct
{
	GF_TextureHandler txh;
} MatteTextureStack;
void R3D_InitMatteTexture(Render3D *sr, GF_Node *node);

/*for lineproperties/XLineProperties*/
typedef struct
{
	Render3D *sr;
	u32 last_mod_time;
} LinePropStack;
void R3D_InitLineProps(Render3D *sr, GF_Node *node);
u32 R3D_LP_GetLastUpdateTime(GF_Node *node);

/*hardcoded proto loader (cf hardcoded_protos.c)*/
void R3D_InitHardcodedProto(Render3D *sr, GF_Node *node);

/*X3D nodes*/
void R3D_InitDisk2D(Render3D *sr, GF_Node *node);
void R3D_InitArc2D(Render3D *sr, GF_Node *node);
void R3D_InitPolyline2D(Render3D *sr, GF_Node *node);
void R3D_InitPolypoint2D(Render3D *sr, GF_Node *node);
void R3D_InitStaticGroup(Render3D *sr, GF_Node *node);
void R3D_InitLineSet(Render3D *sr, GF_Node *node);
void R3D_InitTriangleSet2D(Render3D *sr, GF_Node *node);
void R3D_InitIndexedTriangleFanSet(Render3D *sr, GF_Node *node);
void R3D_InitIndexedTriangleSet(Render3D *sr, GF_Node *node);
void R3D_InitIndexedTriangleStripSet(Render3D *sr, GF_Node *node);
void R3D_InitTriangleFanSet(Render3D *sr, GF_Node *node);
void R3D_InitTriangleSet(Render3D *sr, GF_Node *node);
void R3D_InitTriangleStripSet(Render3D *sr, GF_Node *node);

#endif


