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



#include "stacks2d.h"

#ifndef GPAC_DISABLE_SVG
#include "svg_stacks.h"
#include "laser_stacks.h"
#endif

void R2D_InitLineProps(Render2D *sr, GF_Node *node);

/*hardcoded proto loading - this is mainly used for module development and testing...*/
void R2D_InitHardcodedProto(Render2D *sr, GF_Node *node)
{
	MFURL *proto_url;
	GF_Proto *proto;
	u32 i;
	proto = gf_node_get_proto(node);
	if (!proto) return;
	proto_url = gf_sg_proto_get_extern_url(proto);

	for (i=0; i<proto_url->count; i++) {
		const char *url = proto_url->vals[0].url;
		if (!url) continue;
		/*flash shape emulation node test */
		if (!strnicmp(url, "urn:inet:gpac:builtin:FlashShape", 22 + 10)) {
/*
			void R2D_InitFlashShape(Render2D *sr, GF_Node *node);
			R2D_InitFlashShape(sr, node); 
*/
			return;
		}
		if (!strnicmp(url, "urn:inet:gpac:builtin:TextureText", 22 + 11)) {
			R2D_InitTextureText(sr, node); 
			return;
		}
		if (!strnicmp(url, "urn:inet:gpac:builtin:PathExtrusion", 22 + 13)) {
			void R2D_InitPathExtrusion(Render2D *sr, GF_Node *node);

			R2D_InitPathExtrusion(sr, node); 
			return;
		}
	}
}

void R2D_NodeInit(GF_VisualRenderer *vr, GF_Node *node)
{
	Render2D *sr = (Render2D *)vr->user_priv;

	switch (gf_node_get_tag(node)) {
	case TAG_MPEG4_Background2D: R2D_InitBackground2D(sr, node); break;
	case TAG_MPEG4_Bitmap: R2D_InitBitmap(sr, node); break;
	case TAG_MPEG4_Circle: R2D_InitCircle(sr, node); break;
	case TAG_MPEG4_ColorTransform: R2D_InitColorTransform(sr, node); break;
	case TAG_MPEG4_CompositeTexture2D: R2D_InitCompositeTexture2D(sr, node); break;
	case TAG_MPEG4_Curve2D: R2D_InitCurve2D(sr, node); break;
	case TAG_MPEG4_XCurve2D: R2D_InitCurve2D(sr, node); break;
	case TAG_MPEG4_DiscSensor: R2D_InitDiscSensor(sr, node); break;
	case TAG_MPEG4_Ellipse: R2D_InitEllipse(sr, node); break;
	case TAG_MPEG4_IndexedFaceSet2D: R2D_InitIFS2D(sr, node); break;
	case TAG_MPEG4_IndexedLineSet2D: R2D_InitILS2D(sr, node); break;
	case TAG_MPEG4_Form: R2D_InitForm(sr, node); break;
	case TAG_MPEG4_Layer2D: R2D_InitLayer2D(sr, node); break;
	case TAG_MPEG4_Layout: R2D_InitLayout(sr, node); break;
	case TAG_MPEG4_OrderedGroup: R2D_InitOrderedGroup(sr, node); break;
	case TAG_MPEG4_MatteTexture: R2D_InitMatteTexture(sr, node); break;
	case TAG_MPEG4_PlaneSensor2D: R2D_InitPlaneSensor2D(sr, node); break;
	case TAG_MPEG4_PointSet2D: R2D_InitPointSet2D(sr, node); break;
	case TAG_MPEG4_ProximitySensor2D: R2D_InitProximitySensor2D(sr, node); break;
	case TAG_MPEG4_Transform2D: R2D_InitTransform2D(sr, node); break;
	case TAG_MPEG4_TransformMatrix2D: R2D_InitTransformMatrix2D(sr, node); break;
	case TAG_MPEG4_Viewport: R2D_InitViewport(sr, node); break;
	case TAG_MPEG4_LineProperties: R2D_InitLineProps(sr, node); break;
	case TAG_MPEG4_XLineProperties: R2D_InitLineProps(sr, node); break;
	case TAG_MPEG4_Sound2D: R2D_InitSound2D(sr, node); break;
	case TAG_MPEG4_LinearGradient: R2D_InitLinearGradient(sr, node); break;
	case TAG_MPEG4_RadialGradient: R2D_InitRadialGradient(sr, node); break;
	case TAG_MPEG4_PathLayout: R2D_InitPathLayout(sr, node); break;

	case TAG_MPEG4_Anchor: case TAG_X3D_Anchor: R2D_InitAnchor(sr, node); break;
	case TAG_MPEG4_Group: case TAG_X3D_Group: case TAG_X3D_StaticGroup: R2D_InitGroup(sr, node); break;
	case TAG_MPEG4_Rectangle: case TAG_X3D_Rectangle2D: R2D_InitRectangle(sr, node); break;
	case TAG_MPEG4_Shape: case TAG_X3D_Shape: R2D_InitShape(sr, node); break;
	case TAG_MPEG4_Switch: case TAG_X3D_Switch: R2D_InitSwitch(sr, node); break;
	case TAG_MPEG4_Text: case TAG_X3D_Text: R2D_InitText(sr, node); break;
	case TAG_MPEG4_TouchSensor: case TAG_X3D_TouchSensor: R2D_InitTouchSensor(sr, node); break;


	case TAG_ProtoNode: R2D_InitHardcodedProto(sr, node); break;
		
#ifndef GPAC_DISABLE_SVG
	/*SVG part*/
	case TAG_SVG_svg:		SVG_Init_svg(sr, node); break;
	case TAG_SVG_g:			SVG_Init_g(sr, node); break;
	case TAG_SVG_switch:	SVG_Init_switch(sr, node); break;
	case TAG_SVG_rect:		SVG_Init_rect(sr, node); break;
	case TAG_SVG_circle:	SVG_Init_circle(sr, node); break;
	case TAG_SVG_ellipse:	SVG_Init_ellipse(sr, node); break;
	case TAG_SVG_line:		SVG_Init_line(sr, node); break;
	case TAG_SVG_polyline:	SVG_Init_polyline(sr, node); break;
	case TAG_SVG_polygon:	SVG_Init_polygon(sr, node); break;
	case TAG_SVG_text:		SVG_Init_text(sr, node); break;
	case TAG_SVG_path:		SVG_Init_path(sr, node); break;
	case TAG_SVG_use:		SVG_Init_use(sr, node); break;

	case TAG_SVG_a:			SVG_Init_a(sr, node); break;

	case TAG_SVG_image:		SVG_Init_image(sr, node); break;
	case TAG_SVG_video:		SVG_Init_video(sr, node); break;
	case TAG_SVG_audio:		SVG_Init_audio(sr, node); break;
	case TAG_SVG_linearGradient:			SVG_Init_linearGradient(sr, node); break;
	case TAG_SVG_radialGradient:			SVG_Init_radialGradient(sr, node); break;

	case TAG_SVG_rectClip: LASeR_Init_rectClip(sr, node); break;
	case TAG_SVG_selector: LASeR_Init_selector(sr, node); break;
	case TAG_SVG_simpleLayout: LASeR_Init_simpleLayout(sr, node); break;
#endif
	default: break;
	}
}


Bool R2D_NodeChanged(GF_VisualRenderer *vr, GF_Node *byObj)
{
	Render2D *sr = (Render2D*)vr->user_priv;
	assert(byObj);

	switch (gf_node_get_tag(byObj)) {
	case TAG_MPEG4_Background2D: R2D_Background2DModified(byObj); return 1;
	case TAG_MPEG4_MatteTexture: R2D_MatteTextureModified(byObj); return 1;
	case TAG_MPEG4_Layout: R2D_LayoutModified(byObj); return 1;
	case TAG_MPEG4_Anchor:
	{
		/*mark node as dirty - since anchor acts as a sensor, mark it as dirty as a parent node*/
		gf_node_dirty_set(byObj, GF_SG_NODE_DIRTY | GF_SG_CHILD_DIRTY, 0);
		gf_sr_invalidate(sr->compositor, NULL);
	}
		return 1;
	case TAG_MPEG4_LineProperties:
	case TAG_MPEG4_XLineProperties:
		gf_node_dirty_set(byObj, GF_SG_NODE_DIRTY , 0);
		gf_sr_invalidate(sr->compositor, NULL);
		return 1;
		
#ifndef GPAC_DISABLE_SVG
	case TAG_SVG_animateMotion:
	case TAG_SVG_set: 
	case TAG_SVG_animate: 
	case TAG_SVG_animateColor: 
	case TAG_SVG_animateTransform: 
		//SMIL_Modified_Animation(byObj); return 1;
		return 0;
	case TAG_SVG_a:
	{
		/*mark node as dirty - since a acts as a sensor, mark it as dirty as a parent node*/
		gf_node_dirty_set(byObj, GF_SG_NODE_DIRTY | GF_SG_CHILD_DIRTY, 0);
		gf_sr_invalidate(sr->compositor, NULL);
	}
#endif
	
	/*let the compositor decide what to do*/
	default: return 0;
	}
}
