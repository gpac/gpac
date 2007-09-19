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



#ifndef NODE_STACKS_H
#define NODE_STACKS_H

#include "drawable.h"


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


/*Viewport/Viewpoint/NavigationInfo/Fog stack - exported for generic bindable handling*/
typedef struct
{
	GF_List *reg_stacks;
	Bool prev_was_bound;
	GF_Matrix world_view_mx;
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
#endif

} Background2DStack;

#ifndef GPAC_DISABLE_3D
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
#endif


typedef struct
{
	Render *render;
	u32 last_mod_time;
} LinePropStack;
void render_init_lineprops(Render *sr, GF_Node *node);

void render_init_background2d(Render *sr, GF_Node *node);
void render_background2d_Modified(GF_Node *node);
DrawableContext *b2d_get_context(M_Background2D *ptr, GF_List *from_stack);


void render_init_bitmap(Render  *sr, GF_Node *node);
void render_init_colortransform(Render *sr, GF_Node *node);
void render_init_circle(Render *sr, GF_Node *node);
void render_init_curve2d(Render  *sr, GF_Node *node);
void render_init_ellipse(Render  *sr, GF_Node *node);
void render_init_group(Render *sr, GF_Node *node);
void render_init_orderedgroup(Render *sr, GF_Node *node);
void render_init_pointset2d(Render  *sr, GF_Node *node);
void render_init_rectangle(Render  *sr, GF_Node *node);
void render_init_shape(Render *sr, GF_Node *node);
void render_init_switch(Render *sr, GF_Node *node);
void render_init_transform2d(Render *sr, GF_Node *node);
void render_init_transformmatrix2d(Render *sr, GF_Node *node);
void render_init_indexed_line_set2d(Render *sr, GF_Node *node);
void render_init_indexed_face_set2d(Render *sr, GF_Node *node);

void render_init_viewport(Render *sr, GF_Node *node);

void tr_mx2d_get_matrix(GF_Node *n, GF_Matrix2D *mat);

void render_init_anchor(Render *sr, GF_Node *node);
void render_init_disc_sensor(Render *sr, GF_Node *node);
void render_init_plane_sensor2d(Render *sr, GF_Node *node);
void render_init_proximity_sensor2d(Render *sr, GF_Node *node);
void render_init_touch_sensor(Render *sr, GF_Node *node);

void render_init_group(Render *sr, GF_Node *node);
void render_init_static_group(Render *sr, GF_Node *node);

void render_init_sound2d(Render *sr, GF_Node *node);

void render_init_radial_gradient(Render *sr, GF_Node *node);
void render_init_linear_gradient(Render *sr, GF_Node *node);

void render_init_layer2d(Render *sr, GF_Node *node);

void render_init_form(Render *sr, GF_Node *node);

void render_init_layout(Render *sr, GF_Node *node);
void render_layout_modified(Render *sr, GF_Node *node);

void render_init_path_layout(Render *sr, GF_Node *node);

void render_init_compositetexture2d(Render *sr, GF_Node *node);
void render_init_compositetexture3d(Render *sr, GF_Node *node);
GF_TextureHandler *render_get_composite_texture(GF_Node *node);

void render_init_text(Render *sr, GF_Node *node);

#ifndef GPAC_DISABLE_3D

void render_init_billboard(Render *sr, GF_Node *node);
void render_init_collision(Render *sr, GF_Node *node);
void render_init_lod(Render *sr, GF_Node *node);
void render_init_transform(Render *sr, GF_Node *node);

void render_init_proximity_sensor(Render *sr, GF_Node *node);
void render_init_plane_sensor(Render *sr, GF_Node *node);
void render_init_cylinder_sensor(Render *sr, GF_Node *node);
void render_init_sphere_sensor(Render *sr, GF_Node *node);
void render_init_visibility_sensor(Render *sr, GF_Node *node);

void render_init_box(Render *sr, GF_Node *node);
void render_init_cone(Render *sr, GF_Node *node);
void render_init_cylinder(Render *sr, GF_Node *node);
void render_init_sphere(Render *sr, GF_Node *node);
void render_init_point_set(Render *sr, GF_Node *node);
void render_init_ifs(Render *sr, GF_Node *node);
void render_init_ils(Render *sr, GF_Node *node);
void render_init_elevation_grid(Render *sr, GF_Node *node);
void render_init_extrusion(Render *sr, GF_Node *node);
void render_init_non_linear_deformer(Render *sr, GF_Node *node);

void render_init_sound(Render *sr, GF_Node *node);

void render_init_viewpoint(Render *sr, GF_Node *node);
void render_init_navigation_info(Render *sr, GF_Node *node);
void render_init_fog(Render *sr, GF_Node *node);

void render_init_spot_light(Render *sr, GF_Node *node);
void render_init_point_light(Render *sr, GF_Node *node);
void render_init_directional_light(Render *sr, GF_Node *node);

void render_init_layer3d(Render *sr, GF_Node *node);

void render_init_background(Render *sr, GF_Node *node);
void render_background_modified(GF_Node *node);
#endif

/*X3D nodes*/
void render_init_disk2d(Render *sr, GF_Node *node);
void render_init_arc2d(Render *sr, GF_Node *node);
void render_init_polyline2d(Render *sr, GF_Node *node);
void render_init_triangle_set2d(Render *sr, GF_Node *node);


#ifndef GPAC_DISABLE_3D
void render_init_polypoint2d(Render *sr, GF_Node *node);
void render_init_lineset(Render *sr, GF_Node *node);
void render_init_triangle_set(Render *sr, GF_Node *node);
void render_init_indexed_triangle_set(Render *sr, GF_Node *node);
void render_init_triangle_strip_set(Render *sr, GF_Node *node);
void render_init_indexed_triangle_strip_set(Render *sr, GF_Node *node);
void render_init_triangle_fan_set(Render *sr, GF_Node *node);
void render_init_indexed_triangle_fan_set(Render *sr, GF_Node *node);
#endif


#ifndef GPAC_DISABLE_SVG
void render_init_svg_svg(Render *sr, GF_Node *node);
void render_init_svg_g(Render *sr, GF_Node *node);
void render_init_svg_switch(Render *sr, GF_Node *node);
void render_init_svg_rect(Render *sr, GF_Node *node);
void render_init_svg_circle(Render *sr, GF_Node *node);
void render_init_svg_ellipse(Render *sr, GF_Node *node);
void render_init_svg_line(Render *sr, GF_Node *node);
void render_init_svg_polyline(Render *sr, GF_Node *node);
void render_init_svg_polygon(Render *sr, GF_Node *node);
void render_init_svg_path(Render *sr, GF_Node *node);
void render_init_svg_a(Render *sr, GF_Node *node);
void render_init_svg_linearGradient(Render *sr, GF_Node *node);
void render_init_svg_radialGradient(Render *sr, GF_Node *node);
void render_init_svg_solidColor(Render *sr, GF_Node *node);
void render_init_svg_stop(Render *sr, GF_Node *node);

void render_init_svg_image(Render *sr, GF_Node *node);
void render_init_svg_video(Render *sr, GF_Node *node);
void render_init_svg_audio(Render *sr, GF_Node *node, Bool slaved_timing);
void render_svg_render_animation(GF_Node *node, GF_Node *sub_root, void *rs);

void render_init_svg_text(Render *sr, GF_Node *node);
void render_init_svg_tspan(Render *sr, GF_Node *node);
void render_init_svg_textarea(Render *sr, GF_Node *node);


#endif

/*hardcoded protos*/
void render_init_hardcoded_proto(Render *sr, GF_Node *node);

#ifndef GPAC_DISABLE_3D
void render_text_extrude(GF_Node *node, GF_TraverseState *tr_state, GF_Mesh *mesh, MFVec3f *thespine, Fixed creaseAngle, Bool begin_cap, Bool end_cap, MFRotation *spine_ori, MFVec2f *spine_scale, Bool txAlongSpine);
#endif

void render_init_texture_text(Render *sr, GF_Node *node);

#endif


