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
	//return;
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
	case TAG_MPEG4_NavigationInfo: R2D_InitNavigationInfo(sr, node); break;
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
	/* SVG Part */
	case TAG_SVG_svg:				svg_init_svg(sr, node); break;
	case TAG_SVG_g:					svg_init_g(sr, node); break;
	case TAG_SVG_switch:			svg_init_switch(sr, node); break;
	case TAG_SVG_rect:				svg_init_rect(sr, node); break;
	case TAG_SVG_path:				svg_init_path(sr, node); break;
	case TAG_SVG_circle:			svg_init_circle(sr, node); break;
	case TAG_SVG_ellipse:			svg_init_ellipse(sr, node); break;
	case TAG_SVG_line:				svg_init_line(sr, node); break;
	case TAG_SVG_polyline:			svg_init_polyline(sr, node); break;
	case TAG_SVG_polygon:			svg_init_polygon(sr, node); break;
	case TAG_SVG_text:				svg_init_text(sr, node); break;
	case TAG_SVG_tspan:				svg_init_tspan(sr, node); break;
	case TAG_SVG_textArea:			svg_init_textArea(sr, node); break;
	case TAG_SVG_a:					svg_init_a(sr, node); break;
	case TAG_SVG_image:				svg_init_image(sr, node); break;
	case TAG_SVG_video:				svg_init_video(sr, node); break;
	case TAG_SVG_audio:				svg_init_audio(sr, node, 0); break;

	case TAG_SVG_linearGradient:	svg_init_linearGradient(sr, node); break;
	case TAG_SVG_radialGradient:	svg_init_radialGradient(sr, node); break;
	case TAG_SVG_solidColor:		svg_init_solidColor(sr, node); break;
	case TAG_SVG_stop:				svg_init_stop(sr, node); break;

	/*SVG SA part*/
#ifdef GPAC_ENABLE_SVG_SA
	case TAG_SVG_SA_svg:				svg_sa_init_svg(sr, node); break;
	case TAG_SVG_SA_g:					svg_sa_init_g(sr, node); break;
	case TAG_SVG_SA_switch:				svg_sa_init_switch(sr, node); break;
	case TAG_SVG_SA_rect:				svg_sa_init_rect(sr, node); break;
	case TAG_SVG_SA_circle:				svg_sa_init_circle(sr, node); break;
	case TAG_SVG_SA_ellipse:			svg_sa_init_ellipse(sr, node); break;
	case TAG_SVG_SA_line:				svg_sa_init_line(sr, node); break;
	case TAG_SVG_SA_polyline:			svg_sa_init_polyline(sr, node); break;
	case TAG_SVG_SA_polygon:			svg_sa_init_polygon(sr, node); break;
	case TAG_SVG_SA_text:				svg_sa_init_text(sr, node); break;
	case TAG_SVG_SA_path:				svg_sa_init_path(sr, node); break;
	case TAG_SVG_SA_a:					svg_sa_init_a(sr, node); break;
	case TAG_SVG_SA_image:				svg_init_image(sr, node); break;  /* Warning: using the same call as for TAG_SVG_image */
	case TAG_SVG_SA_video:				svg_init_video(sr, node); break;  /* Warning: using the same call as for TAG_SVG_image */
	case TAG_SVG_SA_audio:				svg_init_audio(sr, node); break;  /* Warning: using the same call as for TAG_SVG_image */

	case TAG_SVG_SA_linearGradient:		svg_sa_init_linearGradient(sr, node); break;
	case TAG_SVG_SA_radialGradient:		svg_sa_init_radialGradient(sr, node); break;
	case TAG_SVG_SA_solidColor:			svg_sa_init_solidColor(sr, node); break;
	case TAG_SVG_SA_stop:				svg_sa_init_stop(sr, node); break;
	case TAG_SVG_SA_rectClip:			LASeR_Init_rectClip(sr, node); break;
	case TAG_SVG_SA_selector:			LASeR_Init_selector(sr, node); break;
	case TAG_SVG_SA_simpleLayout:		LASeR_Init_simpleLayout(sr, node); break;
#endif


#ifdef GPAC_ENABLE_SVG_SANI
	/*SVG SANI part*/
	case TAG_SVG_SANI_svg:				svg_sani_init_svg(sr, node); break;
	case TAG_SVG_SANI_g:				svg_sani_init_g(sr, node); break;
	case TAG_SVG_SANI_switch:			svg_sani_init_switch(sr, node); break;
	case TAG_SVG_SANI_rect:				svg_sani_init_rect(sr, node); break;
	case TAG_SVG_SANI_circle:			svg_sani_init_circle(sr, node); break;
	case TAG_SVG_SANI_ellipse:			svg_sani_init_ellipse(sr, node); break;
	case TAG_SVG_SANI_line:				svg_sani_init_line(sr, node); break;
	case TAG_SVG_SANI_polyline:			svg_sani_init_polyline(sr, node); break;
	case TAG_SVG_SANI_polygon:			svg_sani_init_polygon(sr, node); break;
	//case TAG_SVG_SANI_text:				SVG_SANI_Init_text(sr, node); break;
	case TAG_SVG_SANI_path:				svg_sani_init_path(sr, node); break;
	case TAG_SVG_SANI_a:				svg_sani_init_a(sr, node); break;
	//case TAG_SVG_SANI_image:			SVG_SANI_Init_image(sr, node); break;
	//case TAG_SVG_SANI_video:			SVG_SANI_Init_video(sr, node); break;
	//case TAG_SVG_SANI_audio:			SVG_SANI_Init_audio(sr, node); break;
	case TAG_SVG_SANI_linearGradient:	svg_sani_init_linearGradient(sr, node); break;
	case TAG_SVG_SANI_radialGradient:	svg_sani_init_radialGradient(sr, node); break;
	case TAG_SVG_SANI_solidColor:		svg_sani_init_solidColor(sr, node); break;
	case TAG_SVG_SANI_stop:				svg_sani_init_stop(sr, node); break;
#endif

#endif
	default: 
		GF_LOG(GF_LOG_WARNING, GF_LOG_RENDER, ("[Render2D] node %s will not be rendered\n", gf_node_get_class_name(node)));
		break;
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
#ifdef GPAC_ENABLE_SVG_SA
	case TAG_SVG_SA_a:
#endif
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
