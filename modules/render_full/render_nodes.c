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



#include "node_stacks.h"


void render_node_init(GF_VisualRenderer *vr, GF_Node *node)
{
	Render *sr = (Render *)vr->user_priv;

	switch (gf_node_get_tag(node)) {
	case TAG_MPEG4_Background2D: render_init_background2d(sr, node); break;
	case TAG_MPEG4_Bitmap: render_init_bitmap(sr, node); break;
	case TAG_MPEG4_ColorTransform: render_init_colortransform(sr, node); break;
	case TAG_MPEG4_Circle: render_init_circle(sr, node); break;
	case TAG_MPEG4_Curve2D: render_init_curve2d(sr, node); break;
	case TAG_MPEG4_XCurve2D: render_init_curve2d(sr, node); break;
	case TAG_MPEG4_Ellipse: render_init_ellipse(sr, node); break;
	case TAG_MPEG4_OrderedGroup: render_init_orderedgroup(sr, node); break;
	case TAG_MPEG4_PointSet2D: render_init_pointset2d(sr, node); break;
	case TAG_MPEG4_Transform2D: render_init_transform2d(sr, node); break;
	case TAG_MPEG4_TransformMatrix2D: render_init_transformmatrix2d(sr, node); break;
	case TAG_MPEG4_LineProperties: render_init_lineprops(sr, node); break;
	case TAG_MPEG4_XLineProperties: render_init_lineprops(sr, node); break;
	case TAG_MPEG4_Viewport: render_init_viewport(sr, node); break;

	case TAG_MPEG4_IndexedLineSet2D: render_init_indexed_line_set2d(sr, node); break;
	case TAG_MPEG4_IndexedFaceSet2D: render_init_indexed_face_set2d(sr, node); break;
		
	case TAG_MPEG4_Sound2D: render_init_sound2d(sr, node); break;

	case TAG_MPEG4_LinearGradient: render_init_linear_gradient(sr, node); break;
	case TAG_MPEG4_RadialGradient: render_init_radial_gradient(sr, node); break;

	case TAG_MPEG4_CompositeTexture2D: render_init_compositetexture2d(sr, node); break;

	case TAG_MPEG4_Form: render_init_form(sr, node); break;
	case TAG_MPEG4_Layer2D: render_init_layer2d(sr, node); break;
	case TAG_MPEG4_Layout: render_init_layout(sr, node); break;
	case TAG_MPEG4_PathLayout: render_init_path_layout(sr, node); break;
		
		
	/*sensors*/
	case TAG_MPEG4_Anchor: case TAG_X3D_Anchor: render_init_anchor(sr, node); break;
	case TAG_MPEG4_DiscSensor: render_init_disc_sensor(sr, node); break;
	case TAG_MPEG4_PlaneSensor2D: render_init_plane_sensor2d(sr, node); break;
	case TAG_MPEG4_ProximitySensor2D: render_init_proximity_sensor2d(sr, node); break;
	case TAG_MPEG4_TouchSensor: case TAG_X3D_TouchSensor: render_init_touch_sensor(sr, node); break;

	case TAG_MPEG4_Group: case TAG_X3D_Group: render_init_group(sr, node); break;
	case TAG_MPEG4_Rectangle: case TAG_X3D_Rectangle2D: render_init_rectangle(sr, node); break;
	case TAG_MPEG4_Shape: case TAG_X3D_Shape: render_init_shape(sr, node); break;
	case TAG_MPEG4_Switch: case TAG_X3D_Switch: render_init_switch(sr, node); break;

	case TAG_MPEG4_Text: case TAG_X3D_Text: render_init_text(sr, node); break;

#ifndef GPAC_DISABLE_3D
	case TAG_MPEG4_Background: case TAG_X3D_Background: render_init_background(sr, node); break;

	case TAG_MPEG4_CylinderSensor: case TAG_X3D_CylinderSensor: render_init_cylinder_sensor(sr, node); break;
	case TAG_MPEG4_PlaneSensor: case TAG_X3D_PlaneSensor: render_init_plane_sensor(sr, node); break;
	case TAG_MPEG4_ProximitySensor: case TAG_X3D_ProximitySensor: render_init_proximity_sensor(sr, node); break;
	case TAG_MPEG4_SphereSensor: case TAG_X3D_SphereSensor: render_init_sphere_sensor(sr, node); break;
	case TAG_MPEG4_VisibilitySensor: case TAG_X3D_VisibilitySensor: render_init_visibility_sensor(sr, node); break;

	case TAG_MPEG4_Box: case TAG_X3D_Box: render_init_box(sr, node); break;
	case TAG_MPEG4_Cone: case TAG_X3D_Cone: render_init_cone(sr, node); break;
	case TAG_MPEG4_Cylinder: case TAG_X3D_Cylinder: render_init_cylinder(sr, node); break;
	case TAG_MPEG4_ElevationGrid: case TAG_X3D_ElevationGrid: render_init_elevation_grid(sr, node); break;
	case TAG_MPEG4_Extrusion: case TAG_X3D_Extrusion: render_init_extrusion(sr, node); break;
	case TAG_MPEG4_IndexedFaceSet: case TAG_X3D_IndexedFaceSet: render_init_ifs(sr, node); break;
	case TAG_MPEG4_IndexedLineSet: case TAG_X3D_IndexedLineSet: render_init_ils(sr, node); break;
	case TAG_MPEG4_PointSet: case TAG_X3D_PointSet: render_init_point_set(sr, node); break;
	case TAG_MPEG4_Sphere: case TAG_X3D_Sphere: render_init_sphere(sr, node); break;

	case TAG_MPEG4_Billboard: case TAG_X3D_Billboard: render_init_billboard(sr, node); break;
	case TAG_MPEG4_Collision: case TAG_X3D_Collision: render_init_collision(sr, node); break;
	case TAG_MPEG4_LOD: case TAG_X3D_LOD: render_init_lod(sr, node); break;
	case TAG_MPEG4_Transform: case TAG_X3D_Transform: render_init_transform(sr, node); break;

	case TAG_MPEG4_Sound: case TAG_X3D_Sound: render_init_sound(sr, node); break;

	case TAG_MPEG4_Viewpoint: case TAG_X3D_Viewpoint: render_init_viewpoint(sr, node);  break;
	case TAG_MPEG4_NavigationInfo: case TAG_X3D_NavigationInfo: render_init_navigation_info(sr, node);  break;
	case TAG_MPEG4_Fog: case TAG_X3D_Fog: render_init_fog(sr, node); break;

	case TAG_MPEG4_DirectionalLight: case TAG_X3D_DirectionalLight: render_init_directional_light(sr, node); break;
	case TAG_MPEG4_PointLight: case TAG_X3D_PointLight: render_init_point_light(sr, node); break;
	case TAG_MPEG4_SpotLight: case TAG_X3D_SpotLight: render_init_spot_light(sr, node); break;

	case TAG_MPEG4_NonLinearDeformer: render_init_non_linear_deformer(sr, node); break;

	case TAG_MPEG4_Layer3D: render_init_layer3d(sr, node); break;
	case TAG_MPEG4_CompositeTexture3D: render_init_compositetexture3d(sr, node); break;
#endif


	/*X3D nodes*/
	case TAG_X3D_StaticGroup: render_init_static_group(sr, node); break;
	case TAG_X3D_Disk2D: render_init_disk2d(sr, node); break;
	case TAG_X3D_Arc2D: case TAG_X3D_ArcClose2D: render_init_arc2d(sr, node); break;
	case TAG_X3D_Polyline2D: render_init_polyline2d(sr, node); break;
	case TAG_X3D_TriangleSet2D: render_init_triangle_set2d(sr, node); break;

#ifndef GPAC_DISABLE_3D
	case TAG_X3D_Polypoint2D: render_init_polypoint2d(sr, node); break;
	case TAG_X3D_LineSet: render_init_lineset(sr, node); break;
	case TAG_X3D_TriangleSet: render_init_triangle_set(sr, node); break;
	case TAG_X3D_TriangleStripSet: render_init_triangle_strip_set(sr, node); break;
	case TAG_X3D_TriangleFanSet: render_init_triangle_fan_set(sr, node); break;
	case TAG_X3D_IndexedTriangleFanSet: render_init_indexed_triangle_fan_set(sr, node); break;
	case TAG_X3D_IndexedTriangleStripSet: render_init_indexed_triangle_strip_set(sr, node); break;
	case TAG_X3D_IndexedTriangleSet: render_init_indexed_triangle_set(sr, node); break;
#endif


#ifndef GPAC_DISABLE_SVG
	/* SVG Part */
	case TAG_SVG_svg:				render_init_svg_svg(sr, node); break;
	case TAG_SVG_g:					render_init_svg_g(sr, node); break;
	case TAG_SVG_switch:			render_init_svg_switch(sr, node); break;
	case TAG_SVG_rect:				render_init_svg_rect(sr, node); break;
	case TAG_SVG_path:				render_init_svg_path(sr, node); break;
	case TAG_SVG_circle:			render_init_svg_circle(sr, node); break;
	case TAG_SVG_ellipse:			render_init_svg_ellipse(sr, node); break;
	case TAG_SVG_line:				render_init_svg_line(sr, node); break;
	case TAG_SVG_polyline:			render_init_svg_polyline(sr, node); break;
	case TAG_SVG_polygon:			render_init_svg_polygon(sr, node); break;
	case TAG_SVG_a:					render_init_svg_a(sr, node); break;

	case TAG_SVG_linearGradient:	render_init_svg_linearGradient(sr, node); break;
	case TAG_SVG_radialGradient:	render_init_svg_radialGradient(sr, node); break;
	case TAG_SVG_solidColor:		render_init_svg_solidColor(sr, node); break;
	case TAG_SVG_stop:				render_init_svg_stop(sr, node); break;

	case TAG_SVG_text:				render_init_svg_text(sr, node); break;
	case TAG_SVG_tspan:				render_init_svg_tspan(sr, node); break;
	case TAG_SVG_textArea:			render_init_svg_textarea(sr, node); break;
		
	case TAG_SVG_image:				render_init_svg_image(sr, node); break;
	case TAG_SVG_video:				render_init_svg_video(sr, node); break;
	case TAG_SVG_audio:				render_init_svg_audio(sr, node, 0); break;
#endif

	case TAG_ProtoNode: render_init_hardcoded_proto(sr, node); break;

	default: 
		GF_LOG(GF_LOG_WARNING, GF_LOG_RENDER, ("[Render2D] node %s will not be rendered\n", gf_node_get_class_name(node)));
		break;
	}
}


Bool render_node_changed(GF_VisualRenderer *vr, GF_Node *byObj)
{
	Render *sr = (Render *)vr->user_priv;

	switch (gf_node_get_tag(byObj)) {
	case TAG_MPEG4_Background2D: render_background2d_Modified(byObj); return 1;
#ifndef GPAC_DISABLE_3D
	case TAG_MPEG4_Background: case TAG_X3D_Background: render_background_modified(byObj); return 1;
#endif
	case TAG_MPEG4_Layout: render_layout_modified(sr, byObj); return 1;

	/*let the compositor decide what to do*/
	default: return 0;
	}
}
