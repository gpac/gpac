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

#include <gpac/internal/renderer_dev.h>
#include "render3d_nodes.h"


void R3D_NodeInit(GF_VisualRenderer *vr, GF_Node *node)
{
	Render3D *sr = (Render3D *)vr->user_priv;

	switch (gf_node_get_tag(node)) {
	/*100% MPEG4*/
	case TAG_MPEG4_Background2D: R3D_InitBackground2D(sr, node); break;
	case TAG_MPEG4_Bitmap: R3D_InitBitmap(sr, node); break;
	case TAG_MPEG4_ColorTransform: R3D_InitColorTransform(sr, node); break;
	case TAG_MPEG4_CompositeTexture2D: R3D_InitCompositeTexture2D(sr, node); break;
	case TAG_MPEG4_CompositeTexture3D: R3D_InitCompositeTexture3D(sr, node); break;
	case TAG_MPEG4_Curve2D: R3D_InitCurve2D(sr, node); break;
	case TAG_MPEG4_DiscSensor: R3D_InitDiscSensor(sr, node); break;
	case TAG_MPEG4_Ellipse: R3D_InitEllipse(sr, node); break;
	case TAG_MPEG4_Form: R3D_InitForm(sr, node); break;
	case TAG_MPEG4_IndexedFaceSet2D: R3D_InitIFS2D(sr, node); break;
	case TAG_MPEG4_IndexedLineSet2D: R3D_InitILS2D(sr, node); break;
	case TAG_MPEG4_Layer2D: R3D_InitLayer2D(sr, node); break;
	case TAG_MPEG4_Layer3D: R3D_InitLayer3D(sr, node); break;
	case TAG_MPEG4_Layout: R3D_InitLayout(sr, node); break;
	case TAG_MPEG4_LinearGradient: R3D_InitLinearGradient(sr, node); break;
	case TAG_MPEG4_LineProperties: R3D_InitLineProps(sr, node); break;
	case TAG_MPEG4_OrderedGroup: R3D_InitOrderedGroup(sr, node); break;
	case TAG_MPEG4_PathLayout: R3D_InitPathLayout(sr, node); break;
	case TAG_MPEG4_PlaneSensor2D: R3D_InitPlaneSensor2D(sr, node); break;
	case TAG_MPEG4_ProximitySensor2D: R3D_InitProximitySensor2D(sr, node); break;
	case TAG_MPEG4_RadialGradient: R3D_InitRadialGradient(sr, node); break;
	case TAG_MPEG4_Sound2D: R3D_InitSound2D(sr, node); break;
	case TAG_MPEG4_Transform2D: R3D_InitTransform2D(sr, node); break;
	case TAG_MPEG4_TransformMatrix2D: R3D_InitTransformMatrix2D(sr, node); break;
	case TAG_MPEG4_XCurve2D: R3D_InitCurve2D(sr, node); break;
	case TAG_MPEG4_XLineProperties: R3D_InitLineProps(sr, node); break;
	case TAG_MPEG4_PointSet2D: R3D_InitPointSet2D(sr, node); break;
	case TAG_MPEG4_Viewport: R3D_InitViewport(sr, node); break;
	case TAG_MPEG4_NonLinearDeformer: R3D_InitNonLinearDeformer(sr, node); break;

	/*Note: we typecast whenever possible the X3D nodes to base MPEG4 nodes (our code generation allows that)*/
	case TAG_MPEG4_Anchor: case TAG_X3D_Anchor: R3D_InitAnchor(sr, node); break;
	case TAG_MPEG4_Background: case TAG_X3D_Background: R3D_InitBackground(sr, node); break;
	case TAG_MPEG4_Billboard: case TAG_X3D_Billboard: R3D_InitBillboard(sr, node); break;
	case TAG_MPEG4_Box: case TAG_X3D_Box: R3D_InitBox(sr, node); break;
	case TAG_MPEG4_Collision: case TAG_X3D_Collision: R3D_InitCollision(sr, node); break;
	case TAG_MPEG4_Circle: case TAG_X3D_Circle2D: R3D_InitCircle(sr, node); break;
	case TAG_MPEG4_Cone: case TAG_X3D_Cone: R3D_InitCone(sr, node); break;
	case TAG_MPEG4_Cylinder: case TAG_X3D_Cylinder: R3D_InitCylinder(sr, node); break;
	case TAG_MPEG4_CylinderSensor: case TAG_X3D_CylinderSensor: R3D_InitCylinderSensor(sr, node); break;
	case TAG_MPEG4_DirectionalLight: case TAG_X3D_DirectionalLight: R3D_InitDirectionalLight(sr, node); break;
	case TAG_MPEG4_ElevationGrid: case TAG_X3D_ElevationGrid: R3D_InitElevationGrid(sr, node); break;
	case TAG_MPEG4_Extrusion: case TAG_X3D_Extrusion: R3D_InitExtrusion(sr, node); break;
	case TAG_MPEG4_Fog: case TAG_X3D_Fog: R3D_InitFog(sr, node); break;
	case TAG_MPEG4_Group: case TAG_X3D_Group: R3D_InitGroup(sr, node); break;
	case TAG_MPEG4_IndexedFaceSet: case TAG_X3D_IndexedFaceSet: R3D_InitIFS(sr, node); break;
	case TAG_MPEG4_IndexedLineSet: case TAG_X3D_IndexedLineSet: R3D_InitILS(sr, node); break;
	case TAG_MPEG4_LOD: case TAG_X3D_LOD: R3D_InitLOD(sr, node); break;
	case TAG_MPEG4_NavigationInfo: case TAG_X3D_NavigationInfo: R3D_InitNavigationInfo(sr, node);  break;
	case TAG_MPEG4_PlaneSensor: case TAG_X3D_PlaneSensor: R3D_InitPlaneSensor(sr, node); break;
	case TAG_MPEG4_PointLight: case TAG_X3D_PointLight: R3D_InitPointLight(sr, node); break;
	case TAG_MPEG4_PointSet: case TAG_X3D_PointSet: R3D_InitPointSet(sr, node); break;
	case TAG_MPEG4_ProximitySensor: case TAG_X3D_ProximitySensor: R3D_InitProximitySensor(sr, node); break;
	case TAG_MPEG4_Rectangle: case TAG_X3D_Rectangle2D: R3D_InitRectangle(sr, node); break;
	case TAG_MPEG4_Shape: case TAG_X3D_Shape: R3D_InitShape(sr, node); break;
	case TAG_MPEG4_Sound: case TAG_X3D_Sound: R3D_InitSound(sr, node); break;
	case TAG_MPEG4_Sphere: case TAG_X3D_Sphere: R3D_InitSphere(sr, node); break;
	case TAG_MPEG4_SphereSensor: case TAG_X3D_SphereSensor: R3D_InitSphereSensor(sr, node); break;
	case TAG_MPEG4_SpotLight: case TAG_X3D_SpotLight: R3D_InitSpotLight(sr, node); break;
	case TAG_MPEG4_Switch: case TAG_X3D_Switch: R3D_InitSwitch(sr, node); break;
	case TAG_MPEG4_Text: case TAG_X3D_Text: R3D_InitText(sr, node); break;
	case TAG_MPEG4_TouchSensor: case TAG_X3D_TouchSensor: R3D_InitTouchSensor(sr, node); break;
	case TAG_MPEG4_Transform: case TAG_X3D_Transform: R3D_InitTransform(sr, node); break;
	case TAG_MPEG4_Viewpoint: case TAG_X3D_Viewpoint: R3D_InitViewpoint(sr, node);  break;
	case TAG_MPEG4_VisibilitySensor: case TAG_X3D_VisibilitySensor: R3D_InitVisibilitySensor(sr, node); break;

	/*100% X3D*/
	case TAG_X3D_Disk2D: R3D_InitDisk2D(sr, node); break;
	case TAG_X3D_Arc2D: case TAG_X3D_ArcClose2D: R3D_InitArc2D(sr, node); break;
	case TAG_X3D_Polyline2D: R3D_InitPolyline2D(sr, node); break;
	case TAG_X3D_Polypoint2D: R3D_InitPolypoint2D(sr, node); break;
	case TAG_X3D_StaticGroup: R3D_InitStaticGroup(sr, node); break;
	case TAG_X3D_TriangleSet2D: R3D_InitTriangleSet2D(sr, node); break;
	case TAG_X3D_LineSet: R3D_InitLineSet(sr, node); break;
	case TAG_X3D_TriangleSet: R3D_InitTriangleSet(sr, node); break;
	case TAG_X3D_TriangleStripSet: R3D_InitTriangleStripSet(sr, node); break;
	case TAG_X3D_TriangleFanSet: R3D_InitTriangleFanSet(sr, node); break;
	case TAG_X3D_IndexedTriangleFanSet: R3D_InitIndexedTriangleFanSet(sr, node); break;
	case TAG_X3D_IndexedTriangleStripSet: R3D_InitIndexedTriangleStripSet(sr, node); break;
	case TAG_X3D_IndexedTriangleSet: R3D_InitIndexedTriangleSet(sr, node); break;

	case TAG_ProtoNode: R3D_InitHardcodedProto(sr, node); break;
	}
}

Bool R3D_NodeChanged(GF_VisualRenderer *vr, GF_Node *byObj)
{
	switch (gf_node_get_tag(byObj)) {
	case TAG_MPEG4_Background2D: R3D_Background2DModified(byObj); break;
	case TAG_MPEG4_Background: case TAG_X3D_Background: R3D_BackgroundModified(byObj); break;
	case TAG_MPEG4_Layout: R3D_LayoutModified(byObj); break;
	/*let the compositor decide what to do*/
	default: return 0;
	}
	return 0;
}
