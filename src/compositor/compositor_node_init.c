/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
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

#include "nodes_stacks.h"

#include <gpac/nodes_mpeg4.h>
#include <gpac/nodes_x3d.h>

void gf_sc_on_node_init(GF_Compositor *compositor, GF_Node *node)
{
	switch (gf_node_get_tag(node)) {
	case TAG_MPEG4_AnimationStream: compositor_init_animationstream(compositor, node); break;
	case TAG_MPEG4_AudioBuffer: compositor_init_audiobuffer(compositor, node); break;
	case TAG_MPEG4_AudioSource: compositor_init_audiosource(compositor, node); break;
	case TAG_MPEG4_AudioClip: case TAG_X3D_AudioClip: compositor_init_audioclip(compositor, node); break;
	case TAG_MPEG4_TimeSensor: case TAG_X3D_TimeSensor: compositor_init_timesensor(compositor, node); break;
	case TAG_MPEG4_ImageTexture: case TAG_X3D_ImageTexture: compositor_init_imagetexture(compositor, node); break;
	case TAG_MPEG4_PixelTexture: case TAG_X3D_PixelTexture: compositor_init_pixeltexture(compositor, node); break;
	case TAG_MPEG4_MovieTexture: case TAG_X3D_MovieTexture: compositor_init_movietexture(compositor, node); break;		

	case TAG_MPEG4_Background2D: compositor_init_background2d(compositor, node); break;
	case TAG_MPEG4_Bitmap: compositor_init_bitmap(compositor, node); break;
	case TAG_MPEG4_ColorTransform: compositor_init_colortransform(compositor, node); break;
	case TAG_MPEG4_Circle: compositor_init_circle(compositor, node); break;
	case TAG_MPEG4_Curve2D: compositor_init_curve2d(compositor, node); break;
	case TAG_MPEG4_XCurve2D: compositor_init_curve2d(compositor, node); break;
	case TAG_MPEG4_Ellipse: compositor_init_ellipse(compositor, node); break;
	case TAG_MPEG4_OrderedGroup: compositor_init_orderedgroup(compositor, node); break;
	case TAG_MPEG4_PointSet2D: compositor_init_pointset2d(compositor, node); break;
	case TAG_MPEG4_Transform2D: compositor_init_transform2d(compositor, node); break;
	case TAG_MPEG4_TransformMatrix2D: compositor_init_transformmatrix2d(compositor, node); break;
	case TAG_MPEG4_LineProperties: compositor_init_lineprops(compositor, node); break;
	case TAG_MPEG4_XLineProperties: compositor_init_lineprops(compositor, node); break;
	case TAG_MPEG4_Viewport: compositor_init_viewport(compositor, node); break;

	case TAG_MPEG4_IndexedLineSet2D: compositor_init_indexed_line_set2d(compositor, node); break;
	case TAG_MPEG4_IndexedFaceSet2D: compositor_init_indexed_face_set2d(compositor, node); break;
		
	case TAG_MPEG4_Sound2D: compositor_init_sound2d(compositor, node); break;

	case TAG_MPEG4_LinearGradient: compositor_init_linear_gradient(compositor, node); break;
	case TAG_MPEG4_RadialGradient: compositor_init_radial_gradient(compositor, node); break;

	case TAG_MPEG4_CompositeTexture2D: compositor_init_compositetexture2d(compositor, node); break;
	case TAG_MPEG4_MatteTexture: compositor_init_mattetexture(compositor, node); break;

	case TAG_MPEG4_Form: compositor_init_form(compositor, node); break;
	case TAG_MPEG4_Layer2D: compositor_init_layer2d(compositor, node); break;
	case TAG_MPEG4_Layout: compositor_init_layout(compositor, node); break;
	case TAG_MPEG4_PathLayout: compositor_init_path_layout(compositor, node); break;
		
		
	/*sensors*/
	case TAG_MPEG4_Anchor: case TAG_X3D_Anchor: compositor_init_anchor(compositor, node); break;
	case TAG_MPEG4_DiscSensor: compositor_init_disc_sensor(compositor, node); break;
	case TAG_MPEG4_PlaneSensor2D: compositor_init_plane_sensor2d(compositor, node); break;
	case TAG_MPEG4_ProximitySensor2D: compositor_init_proximity_sensor2d(compositor, node); break;
	case TAG_MPEG4_TouchSensor: case TAG_X3D_TouchSensor: compositor_init_touch_sensor(compositor, node); break;

	case TAG_MPEG4_Group: case TAG_X3D_Group: compositor_init_group(compositor, node); break;
	case TAG_MPEG4_Rectangle: case TAG_X3D_Rectangle2D: compositor_init_rectangle(compositor, node); break;
	case TAG_MPEG4_Shape: case TAG_X3D_Shape: compositor_init_shape(compositor, node); break;
	case TAG_MPEG4_Switch: case TAG_X3D_Switch: compositor_init_switch(compositor, node); break;

	case TAG_MPEG4_Text: case TAG_X3D_Text: compositor_init_text(compositor, node); break;

#ifndef GPAC_DISABLE_3D
	case TAG_MPEG4_Background: case TAG_X3D_Background: compositor_init_background(compositor, node); break;

	case TAG_MPEG4_CylinderSensor: case TAG_X3D_CylinderSensor: compositor_init_cylinder_sensor(compositor, node); break;
	case TAG_MPEG4_PlaneSensor: case TAG_X3D_PlaneSensor: compositor_init_plane_sensor(compositor, node); break;
	case TAG_MPEG4_ProximitySensor: case TAG_X3D_ProximitySensor: compositor_init_proximity_sensor(compositor, node); break;
	case TAG_MPEG4_SphereSensor: case TAG_X3D_SphereSensor: compositor_init_sphere_sensor(compositor, node); break;
	case TAG_MPEG4_VisibilitySensor: case TAG_X3D_VisibilitySensor: compositor_init_visibility_sensor(compositor, node); break;

	case TAG_MPEG4_Box: case TAG_X3D_Box: compositor_init_box(compositor, node); break;
	case TAG_MPEG4_Cone: case TAG_X3D_Cone: compositor_init_cone(compositor, node); break;
	case TAG_MPEG4_Cylinder: case TAG_X3D_Cylinder: compositor_init_cylinder(compositor, node); break;
	case TAG_MPEG4_ElevationGrid: case TAG_X3D_ElevationGrid: compositor_init_elevation_grid(compositor, node); break;
	case TAG_MPEG4_Extrusion: case TAG_X3D_Extrusion: compositor_init_extrusion(compositor, node); break;
	case TAG_MPEG4_IndexedFaceSet: case TAG_X3D_IndexedFaceSet: compositor_init_ifs(compositor, node); break;
	case TAG_MPEG4_IndexedLineSet: case TAG_X3D_IndexedLineSet: compositor_init_ils(compositor, node); break;
	case TAG_MPEG4_PointSet: case TAG_X3D_PointSet: compositor_init_point_set(compositor, node); break;
	case TAG_MPEG4_Sphere: case TAG_X3D_Sphere: compositor_init_sphere(compositor, node); break;

	case TAG_MPEG4_Billboard: case TAG_X3D_Billboard: compositor_init_billboard(compositor, node); break;
	case TAG_MPEG4_Collision: case TAG_X3D_Collision: compositor_init_collision(compositor, node); break;
	case TAG_MPEG4_LOD: case TAG_X3D_LOD: compositor_init_lod(compositor, node); break;
	case TAG_MPEG4_Transform: case TAG_X3D_Transform: compositor_init_transform(compositor, node); break;

	case TAG_MPEG4_Sound: case TAG_X3D_Sound: compositor_init_sound(compositor, node); break;

	case TAG_MPEG4_Viewpoint: case TAG_X3D_Viewpoint: compositor_init_viewpoint(compositor, node);  break;
	case TAG_MPEG4_NavigationInfo: case TAG_X3D_NavigationInfo: compositor_init_navigation_info(compositor, node);  break;
	case TAG_MPEG4_Fog: case TAG_X3D_Fog: compositor_init_fog(compositor, node); break;

	case TAG_MPEG4_DirectionalLight: case TAG_X3D_DirectionalLight: compositor_init_directional_light(compositor, node); break;
	case TAG_MPEG4_PointLight: case TAG_X3D_PointLight: compositor_init_point_light(compositor, node); break;
	case TAG_MPEG4_SpotLight: case TAG_X3D_SpotLight: compositor_init_spot_light(compositor, node); break;

	case TAG_MPEG4_NonLinearDeformer: compositor_init_non_linear_deformer(compositor, node); break;

	case TAG_MPEG4_Layer3D: compositor_init_layer3d(compositor, node); break;
	case TAG_MPEG4_CompositeTexture3D: compositor_init_compositetexture3d(compositor, node); break;
#endif


	/*X3D nodes*/
	case TAG_X3D_StaticGroup: compositor_init_static_group(compositor, node); break;
	case TAG_X3D_Disk2D: compositor_init_disk2d(compositor, node); break;
	case TAG_X3D_Arc2D: case TAG_X3D_ArcClose2D: compositor_init_arc2d(compositor, node); break;
	case TAG_X3D_Polyline2D: compositor_init_polyline2d(compositor, node); break;
	case TAG_X3D_TriangleSet2D: compositor_init_triangle_set2d(compositor, node); break;

#ifndef GPAC_DISABLE_3D
	case TAG_X3D_Polypoint2D: compositor_init_polypoint2d(compositor, node); break;
	case TAG_X3D_LineSet: compositor_init_lineset(compositor, node); break;
	case TAG_X3D_TriangleSet: compositor_init_triangle_set(compositor, node); break;
	case TAG_X3D_TriangleStripSet: compositor_init_triangle_strip_set(compositor, node); break;
	case TAG_X3D_TriangleFanSet: compositor_init_triangle_fan_set(compositor, node); break;
	case TAG_X3D_IndexedTriangleFanSet: compositor_init_indexed_triangle_fan_set(compositor, node); break;
	case TAG_X3D_IndexedTriangleStripSet: compositor_init_indexed_triangle_strip_set(compositor, node); break;
	case TAG_X3D_IndexedTriangleSet: compositor_init_indexed_triangle_set(compositor, node); break;
#endif


#ifndef GPAC_DISABLE_SVG
	/* SVG Part */
	case TAG_SVG_svg:				compositor_init_svg_svg(compositor, node); break;
	case TAG_SVG_g:					compositor_init_svg_g(compositor, node); break;
	case TAG_SVG_switch:			compositor_init_svg_switch(compositor, node); break;
	case TAG_SVG_rect:				compositor_init_svg_rect(compositor, node); break;
	case TAG_SVG_path:				compositor_init_svg_path(compositor, node); break;
	case TAG_SVG_circle:			compositor_init_svg_circle(compositor, node); break;
	case TAG_SVG_ellipse:			compositor_init_svg_ellipse(compositor, node); break;
	case TAG_SVG_line:				compositor_init_svg_line(compositor, node); break;
	case TAG_SVG_polyline:			compositor_init_svg_polyline(compositor, node); break;
	case TAG_SVG_polygon:			compositor_init_svg_polygon(compositor, node); break;
	case TAG_SVG_a:					compositor_init_svg_a(compositor, node); break;

	case TAG_SVG_linearGradient:	compositor_init_svg_linearGradient(compositor, node); break;
	case TAG_SVG_radialGradient:	compositor_init_svg_radialGradient(compositor, node); break;
	case TAG_SVG_solidColor:		compositor_init_svg_solidColor(compositor, node); break;
	case TAG_SVG_stop:				compositor_init_svg_stop(compositor, node); break;

	case TAG_SVG_text:				compositor_init_svg_text(compositor, node); break;
	case TAG_SVG_tspan:				compositor_init_svg_tspan(compositor, node); break;
	case TAG_SVG_textArea:			compositor_init_svg_textarea(compositor, node); break;
	case TAG_SVG_tbreak:			compositor_init_svg_tbreak(compositor, node); break;
		
	case TAG_SVG_image:				compositor_init_svg_image(compositor, node); break;
	case TAG_SVG_video:				compositor_init_svg_video(compositor, node); break;
	case TAG_SVG_audio:				compositor_init_svg_audio(compositor, node, 0); break;

	/*SVG font support - note that we initialize the font when parsing the font-face element, not the font element*/
	case TAG_SVG_font_face:			compositor_init_svg_font(compositor, node); break;
	case TAG_SVG_missing_glyph:
	case TAG_SVG_glyph:
		compositor_init_svg_glyph(compositor, node); 
		break;
	case TAG_SVG_font_face_uri:
		compositor_init_svg_font_face_uri(compositor, node); break;

	case TAG_SVG_use:				compositor_init_svg_use(compositor, node); break;
	case TAG_SVG_animation:			compositor_init_svg_animation(compositor, node); break;
	case TAG_SVG_foreignObject:		compositor_init_svg_foreign_object(compositor, node); break;
#endif

	case TAG_ProtoNode: compositor_init_hardcoded_proto(compositor, node); break;

	default:
		GF_LOG(GF_LOG_WARNING, GF_LOG_COMPOSE, ("[Compositor] node %s will not be rendered\n", gf_node_get_class_name(node)));
		break;
	}
}

GF_EXPORT
void gf_sc_invalidate(GF_Compositor *compositor, GF_Node *byObj)
{

	if (!byObj) {
		compositor->draw_next_frame = 1;
		return;
	}
	switch (gf_node_get_tag(byObj)) {
	case TAG_MPEG4_AnimationStream: compositor_animationstream_modified(byObj); break;
	case TAG_MPEG4_AudioBuffer: compositor_audiobuffer_modified(byObj); break;
	case TAG_MPEG4_AudioSource: compositor_audiosource_modified(byObj); break;
	case TAG_MPEG4_AudioClip: case TAG_X3D_AudioClip: compositor_audioclip_modified(byObj); break;
	case TAG_MPEG4_TimeSensor: case TAG_X3D_TimeSensor: compositor_timesensor_modified(byObj); break;
	case TAG_MPEG4_ImageTexture: case TAG_X3D_ImageTexture: compositor_imagetexture_modified(byObj); break;
	case TAG_MPEG4_MovieTexture: case TAG_X3D_MovieTexture: compositor_movietexture_modified(byObj); break;

	case TAG_MPEG4_Background2D: compositor_background2d_modified(byObj); break;
#ifndef GPAC_DISABLE_3D
	case TAG_MPEG4_Background: case TAG_X3D_Background: compositor_background_modified(byObj); break;
#endif
	case TAG_MPEG4_Layout: compositor_layout_modified(compositor, byObj); break;

	default:
		/*for all nodes, invalidate parent graph - note we do that for sensors as well to force recomputing
		sensor list cached at grouping node level*/
		gf_node_dirty_set(byObj, 0, 1);
		compositor->draw_next_frame = 1;
		break;
	}
}
