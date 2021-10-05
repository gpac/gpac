/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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


#ifndef NODES_STACKS_H
#define NODES_STACKS_H

#include <gpac/nodes_mpeg4.h>
#include <gpac/nodes_x3d.h>

#ifndef GPAC_DISABLE_SVG
#include <gpac/nodes_svg.h>
#endif

#include "drawable.h"


#ifndef GPAC_DISABLE_VRML

void compositor_init_audiosource(GF_Compositor *compositor, GF_Node *node);
void compositor_audiosource_modified(GF_Node *node);

void compositor_init_audioclip(GF_Compositor *compositor, GF_Node *node);
void compositor_audioclip_modified(GF_Node *node);

void compositor_init_audiobuffer(GF_Compositor *compositor, GF_Node *node);
void compositor_audiobuffer_modified(GF_Node *node);

void compositor_init_animationstream(GF_Compositor *compositor, GF_Node *node);
void compositor_animationstream_modified(GF_Node *node);

void compositor_init_timesensor(GF_Compositor *compositor, GF_Node *node);
void compositor_timesensor_modified(GF_Node *node);

void compositor_init_imagetexture(GF_Compositor *compositor, GF_Node *node);
void compositor_imagetexture_modified(GF_Node *node);
GF_TextureHandler *it_get_texture(GF_Node *node);

void compositor_init_movietexture(GF_Compositor *compositor, GF_Node *node);
void compositor_movietexture_modified(GF_Node *node);
GF_TextureHandler *mt_get_texture(GF_Node *node);

void compositor_init_pixeltexture(GF_Compositor *compositor, GF_Node *node);
GF_TextureHandler *pt_get_texture(GF_Node *node);


/*unregister bindable from all bindable stacks listed, and bind their top if needed*/
void PreDestroyBindable(GF_Node *bindable, GF_List *stack_list);
/*returns isBound*/
Bool Bindable_GetIsBound(GF_Node *bindable);
/*sets isBound*/
void Bindable_SetIsBound(GF_Node *bindable, Bool val);
/*generic on_set_bind for all bindables*/
void Bindable_OnSetBind(GF_Node *bindable, GF_List *stack_list, GF_List *for_stack);
/*remove all bindable references to this stack and destroy chain*/
void BindableStackDelete(GF_List *stack);
/*for user-modif of viewport/viewpoint*/
void Bindable_SetSetBind(GF_Node *bindable, Bool val);

/*special user-modif of viewport/viewpoint:
 if stack is not NULL, binding is only performed in this stack
 otherwise,  binding is performed on all stack*/
void Bindable_SetSetBindEx(GF_Node *bindable, Bool val, GF_List *stack);


/*Viewport/Viewpoint/NavigationInfo/Fog stack - exported for generic bindable handling*/
typedef struct
{
	GF_List *reg_stacks;
	Bool prev_was_bound;
	GF_Matrix world_view_mx;
	SFVec2f last_vp_size;
	u32 last_sim_time;
} ViewStack;

typedef struct
{
	/*for image background*/
	GF_TextureHandler txh;
	/*list of all background stacks using us*/
	GF_List *reg_stacks;
	/*list of allocated background status (used to store bounds info & DrawableContext for Layer2D)
	there is one status per entry in the reg_stack*/
	GF_List *status_stack;
	Drawable *drawable;

#ifndef GPAC_DISABLE_3D
	GF_Mesh *mesh;
	GF_BBox prev_bounds;
	Bool hybgl_init;
#endif
	u32 flags;
	char col_tx[12];
} Background2DStack;

#ifndef GPAC_DISABLE_3D
typedef struct
{
	GF_Compositor *compositor;
	/*keep track of background stacks using us*/
	GF_List *reg_stacks;
	GF_Mesh *sky_mesh, *ground_mesh;
	MFFloat sky_col, ground_col;
	MFFloat sky_ang, ground_ang;

	GF_Mesh *front_mesh, *back_mesh, *top_mesh, *bottom_mesh, *left_mesh, *right_mesh;
	GF_TextureHandler txh_front, txh_back, txh_top, txh_bottom, txh_left, txh_right;
	GF_Matrix current_mx;
} BackgroundStack;
#endif

#ifndef GPAC_DISABLE_SVG
typedef struct
{
	GF_TextureHandler txh;
	Drawable *drawable;
	MFURL txurl;
	Bool first_frame_fetched;
	GF_Node *audio;
	Bool audio_dirty;
	Bool stop_requested;
} SVG_video_stack;

typedef struct
{
	GF_AudioInput input;
	Bool is_active, is_error;
	MFURL aurl;
} SVG_audio_stack;

#endif


typedef struct
{
	GF_Compositor *compositor;
	u32 last_mod_time;
	Bool is_dirty;
} LinePropStack;
void compositor_init_lineprops(GF_Compositor *compositor, GF_Node *node);

void compositor_init_background2d(GF_Compositor *compositor, GF_Node *node);
void compositor_background2d_modified(GF_Node *node);
DrawableContext *b2d_get_context(M_Background2D *ptr, GF_List *from_stack);


void compositor_init_bitmap(GF_Compositor  *compositor, GF_Node *node);
void compositor_init_colortransform(GF_Compositor *compositor, GF_Node *node);
void compositor_init_circle(GF_Compositor *compositor, GF_Node *node);
void compositor_init_curve2d(GF_Compositor  *compositor, GF_Node *node);
void compositor_init_ellipse(GF_Compositor  *compositor, GF_Node *node);
void compositor_init_group(GF_Compositor *compositor, GF_Node *node);
void compositor_init_orderedgroup(GF_Compositor *compositor, GF_Node *node);
void compositor_init_pointset2d(GF_Compositor  *compositor, GF_Node *node);
void compositor_init_rectangle(GF_Compositor  *compositor, GF_Node *node);
void compositor_init_shape(GF_Compositor *compositor, GF_Node *node);
void compositor_init_switch(GF_Compositor *compositor, GF_Node *node);
void compositor_init_transform2d(GF_Compositor *compositor, GF_Node *node);
void compositor_init_transformmatrix2d(GF_Compositor *compositor, GF_Node *node);
void compositor_init_indexed_line_set2d(GF_Compositor *compositor, GF_Node *node);
void compositor_init_indexed_face_set2d(GF_Compositor *compositor, GF_Node *node);

void compositor_init_bitwrapper(GF_Compositor *compositor, GF_Node *node);

void compositor_init_viewport(GF_Compositor *compositor, GF_Node *node);

void tr_mx2d_get_matrix(GF_Node *n, GF_Matrix2D *mat);

void compositor_init_anchor(GF_Compositor *compositor, GF_Node *node);
void compositor_init_disc_sensor(GF_Compositor *compositor, GF_Node *node);
void compositor_init_plane_sensor2d(GF_Compositor *compositor, GF_Node *node);
void compositor_init_proximity_sensor2d(GF_Compositor *compositor, GF_Node *node);
void compositor_init_touch_sensor(GF_Compositor *compositor, GF_Node *node);

void compositor_init_group(GF_Compositor *compositor, GF_Node *node);
void compositor_init_static_group(GF_Compositor *compositor, GF_Node *node);

void compositor_init_sound2d(GF_Compositor *compositor, GF_Node *node);

void compositor_init_radial_gradient(GF_Compositor *compositor, GF_Node *node);
void compositor_init_linear_gradient(GF_Compositor *compositor, GF_Node *node);
void compositor_init_mattetexture(GF_Compositor *compositor, GF_Node *node);

void compositor_init_layer2d(GF_Compositor *compositor, GF_Node *node);

void compositor_init_form(GF_Compositor *compositor, GF_Node *node);

void compositor_init_layout(GF_Compositor *compositor, GF_Node *node);
void compositor_layout_modified(GF_Compositor *compositor, GF_Node *node);
GF_SensorHandler *compositor_mpeg4_layout_get_sensor_handler(GF_Node *node);

void compositor_init_path_layout(GF_Compositor *compositor, GF_Node *node);

void compositor_init_compositetexture2d(GF_Compositor *compositor, GF_Node *node);
void compositor_init_compositetexture3d(GF_Compositor *compositor, GF_Node *node);
GF_TextureHandler *compositor_get_composite_texture(GF_Node *node);
void compositor_adjust_scale(GF_Node *node, Fixed *sx, Fixed *sy);
Bool compositor_is_composite_texture(GF_Node *appear);
Bool compositor_compositetexture_handle_event(GF_Compositor *compositor, GF_Node *composite_appear, GF_Event *ev, Bool is_flush);
void compositor_compositetexture_sensor_delete(GF_Node *composite_appear, GF_SensorHandler *hdl);

void compositor_init_text(GF_Compositor *compositor, GF_Node *node);

#ifndef GPAC_DISABLE_3D

void compositor_init_billboard(GF_Compositor *compositor, GF_Node *node);
void compositor_init_collision(GF_Compositor *compositor, GF_Node *node);
void compositor_init_lod(GF_Compositor *compositor, GF_Node *node);
void compositor_init_transform(GF_Compositor *compositor, GF_Node *node);

void compositor_init_proximity_sensor(GF_Compositor *compositor, GF_Node *node);
void compositor_init_plane_sensor(GF_Compositor *compositor, GF_Node *node);
void compositor_init_cylinder_sensor(GF_Compositor *compositor, GF_Node *node);
void compositor_init_sphere_sensor(GF_Compositor *compositor, GF_Node *node);
void compositor_init_visibility_sensor(GF_Compositor *compositor, GF_Node *node);

void compositor_init_box(GF_Compositor *compositor, GF_Node *node);
void compositor_init_cone(GF_Compositor *compositor, GF_Node *node);
void compositor_init_cylinder(GF_Compositor *compositor, GF_Node *node);
void compositor_init_sphere(GF_Compositor *compositor, GF_Node *node);
void compositor_init_point_set(GF_Compositor *compositor, GF_Node *node);
void compositor_init_ifs(GF_Compositor *compositor, GF_Node *node);
void compositor_init_ils(GF_Compositor *compositor, GF_Node *node);
void compositor_init_elevation_grid(GF_Compositor *compositor, GF_Node *node);
void compositor_init_extrusion(GF_Compositor *compositor, GF_Node *node);
void compositor_init_non_linear_deformer(GF_Compositor *compositor, GF_Node *node);

void compositor_init_sound(GF_Compositor *compositor, GF_Node *node);

void compositor_init_viewpoint(GF_Compositor *compositor, GF_Node *node);
void compositor_init_navigation_info(GF_Compositor *compositor, GF_Node *node);
void compositor_init_fog(GF_Compositor *compositor, GF_Node *node);

void compositor_init_spot_light(GF_Compositor *compositor, GF_Node *node);
void compositor_init_point_light(GF_Compositor *compositor, GF_Node *node);
void compositor_init_directional_light(GF_Compositor *compositor, GF_Node *node);

void compositor_init_layer3d(GF_Compositor *compositor, GF_Node *node);

void compositor_init_background(GF_Compositor *compositor, GF_Node *node);
void compositor_background_modified(GF_Node *node);
#endif

/*X3D nodes*/
void compositor_init_disk2d(GF_Compositor *compositor, GF_Node *node);
void compositor_init_arc2d(GF_Compositor *compositor, GF_Node *node);
void compositor_init_polyline2d(GF_Compositor *compositor, GF_Node *node);
void compositor_init_triangle_set2d(GF_Compositor *compositor, GF_Node *node);


#ifndef GPAC_DISABLE_3D
void compositor_init_polypoint2d(GF_Compositor *compositor, GF_Node *node);
void compositor_init_lineset(GF_Compositor *compositor, GF_Node *node);
void compositor_init_triangle_set(GF_Compositor *compositor, GF_Node *node);
void compositor_init_indexed_triangle_set(GF_Compositor *compositor, GF_Node *node);
void compositor_init_triangle_strip_set(GF_Compositor *compositor, GF_Node *node);
void compositor_init_indexed_triangle_strip_set(GF_Compositor *compositor, GF_Node *node);
void compositor_init_triangle_fan_set(GF_Compositor *compositor, GF_Node *node);
void compositor_init_indexed_triangle_fan_set(GF_Compositor *compositor, GF_Node *node);
#endif

GF_TextureHandler *compositor_mpeg4_get_gradient_texture(GF_Node *node);

/*hardcoded protos*/
void gf_sc_init_hardcoded_proto(GF_Compositor *compositor, GF_Node *node);
GF_TextureHandler *gf_sc_hardcoded_proto_get_texture_handler(GF_Node *n);

#ifndef GPAC_DISABLE_3D
void compositor_extrude_text(GF_Node *node, GF_TraverseState *tr_state, GF_Mesh *mesh, MFVec3f *thespine, Fixed creaseAngle, Bool begin_cap, Bool end_cap, MFRotation *spine_ori, MFVec2f *spine_scale, Bool txAlongSpine);
#endif

void compositor_init_envtest(GF_Compositor *compositor, GF_Node *node);
void compositor_envtest_modified(GF_Node *node);
void compositor_evaluate_envtests(GF_Compositor *compositor, u32 param_type);

#ifdef GPAC_ENABLE_FLASHSHAPE
void compositor_init_hc_flashshape(GF_Compositor *compositor, GF_Node *node);
#endif

#endif /*GPAC_DISABLE_VRML*/


#ifndef GPAC_DISABLE_SVG
void compositor_init_svg_svg(GF_Compositor *compositor, GF_Node *node);
void compositor_init_svg_g(GF_Compositor *compositor, GF_Node *node);
void compositor_init_svg_defs(GF_Compositor *compositor, GF_Node *node);
void compositor_init_svg_switch(GF_Compositor *compositor, GF_Node *node);
void compositor_init_svg_rect(GF_Compositor *compositor, GF_Node *node);
void compositor_init_svg_circle(GF_Compositor *compositor, GF_Node *node);
void compositor_init_svg_ellipse(GF_Compositor *compositor, GF_Node *node);
void compositor_init_svg_line(GF_Compositor *compositor, GF_Node *node);
void compositor_init_svg_polyline(GF_Compositor *compositor, GF_Node *node);
void compositor_init_svg_polygon(GF_Compositor *compositor, GF_Node *node);
void compositor_init_svg_path(GF_Compositor *compositor, GF_Node *node);
void compositor_init_svg_a(GF_Compositor *compositor, GF_Node *node);
void compositor_init_svg_linearGradient(GF_Compositor *compositor, GF_Node *node);
void compositor_init_svg_radialGradient(GF_Compositor *compositor, GF_Node *node);
void compositor_init_svg_solidColor(GF_Compositor *compositor, GF_Node *node);
Bool compositor_svg_solid_color_dirty(GF_Compositor *compositor, GF_Node *node);
void compositor_init_svg_stop(GF_Compositor *compositor, GF_Node *node);
void compositor_init_svg_clip_path(GF_Compositor *compositor, GF_Node *node);

void compositor_init_svg_image(GF_Compositor *compositor, GF_Node *node);
void compositor_init_svg_video(GF_Compositor *compositor, GF_Node *node);
void compositor_init_svg_audio(GF_Compositor *compositor, GF_Node *node, Bool slaved_timing);
void compositor_init_svg_use(GF_Compositor *compositor, GF_Node *node);
void compositor_init_svg_animation(GF_Compositor *compositor, GF_Node *node);
void compositor_init_svg_foreign_object(GF_Compositor *compositor, GF_Node *node);


void compositor_init_svg_text(GF_Compositor *compositor, GF_Node *node);
void compositor_init_svg_tspan(GF_Compositor *compositor, GF_Node *node);
void compositor_init_svg_textarea(GF_Compositor *compositor, GF_Node *node);
void compositor_init_svg_tbreak(GF_Compositor *compositor, GF_Node *node);

void compositor_init_svg_font(GF_Compositor *compositor, GF_Node *node);
void compositor_init_svg_glyph(GF_Compositor *compositor, GF_Node *node);
void compositor_init_svg_font_face_uri(GF_Compositor *compositor, GF_Node *node);

void compositor_init_svg_updates(GF_Compositor *compositor, GF_Node *node);

#ifdef GPAC_ENABLE_SVG_FILTERS
void compositor_init_svg_filter(GF_Compositor *compositor, GF_Node *node);
void svg_draw_filter(GF_Node *filter, GF_Node *node, GF_TraverseState *tr_state);
#endif


GF_TextureHandler *compositor_svg_get_gradient_texture(GF_Node *node);
GF_TextureHandler *compositor_svg_get_image_texture(GF_Node *node);

Bool compositor_svg_get_viewport(GF_Node *n, GF_Rect *rc);
void svg_pause_animation(GF_Node *n, Bool pause);
void svg_pause_audio(GF_Node *n, Bool pause);
void svg_pause_video(GF_Node *n, Bool pause);

void compositor_svg_video_modified(GF_Compositor *compositor, GF_Node *node);

#endif



#endif	/*NODES_STACKS_H*/

