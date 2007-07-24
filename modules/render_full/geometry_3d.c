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


#include "visual_manager.h"

#ifndef GPAC_DISABLE_3D

#include <gpac/options.h>


void drawable_3d_base_render(GF_Node *n, void *rs, Bool is_destroy, void (*build_shape)(GF_Node*,Drawable3D *,GF_TraverseState *) )
{
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;
	Drawable3D *stack = (Drawable3D*)gf_node_get_private(n);

	if (is_destroy) {
		drawable_3d_del(n);
		return;
	}
	if (gf_node_dirty_get(n)) {
		mesh_reset(stack->mesh);
		build_shape(n, stack, tr_state);
		gf_node_dirty_clear(n, 0);
	}
	switch (tr_state->traversing_mode) {
	case TRAVERSE_DRAW_3D:
		visual_3d_draw(tr_state, stack->mesh);
		break;
	case TRAVERSE_GET_BOUNDS:
		tr_state->bbox = stack->mesh->bounds;
		break;
	case TRAVERSE_PICK:
		visual_3d_drawable_pick(n, tr_state, stack->mesh, NULL);
		return;
	}
}

static void build_shape_box(GF_Node *n, Drawable3D *stack, GF_TraverseState *tr_state)
{
	mesh_new_box(stack->mesh, ((M_Box *)n)->size);
}

static void RenderBox(GF_Node *n, void *rs, Bool is_destroy)
{
	drawable_3d_base_render(n, rs, is_destroy, build_shape_box);
}

void render_init_box(Render *sr, GF_Node *node)
{
	drawable_3d_new(node);
	gf_node_set_callback_function(node, RenderBox);
}

static void build_shape_cone(GF_Node *n, Drawable3D *stack, GF_TraverseState *tr_state)
{
	M_Cone *co = (M_Cone *)n;
	mesh_new_cone(stack->mesh, co->height, co->bottomRadius, co->bottom, co->side, tr_state->visual->render->compositor->high_speed);
}

static void RenderCone(GF_Node *n, void *rs, Bool is_destroy)
{
	drawable_3d_base_render(n, rs, is_destroy, build_shape_cone);
}

void render_init_cone(Render *sr, GF_Node *node)
{
	drawable_3d_new(node);
	gf_node_set_callback_function(node, RenderCone);
}

static void build_shape_cylinder(GF_Node *n, Drawable3D *stack, GF_TraverseState *tr_state)
{
	M_Cylinder *cy = (M_Cylinder *)n;
	mesh_new_cylinder(stack->mesh, cy->height, cy->radius, cy->bottom, cy->side, cy->top, tr_state->visual->render->compositor->high_speed);
}

static void RenderCylinder(GF_Node *n, void *rs, Bool is_destroy)
{
	drawable_3d_base_render(n, rs, is_destroy, build_shape_cylinder);
}

void render_init_cylinder(Render *sr, GF_Node *node)
{
	drawable_3d_new(node);
	gf_node_set_callback_function(node, RenderCylinder);
}

static void build_shape_sphere(GF_Node *n, Drawable3D *stack, GF_TraverseState *tr_state)
{
	M_Sphere *sp = (M_Sphere *)n;
	mesh_new_sphere(stack->mesh, sp->radius, tr_state->visual->render->compositor->high_speed);
}

static void RenderSphere(GF_Node *n, void *rs, Bool is_destroy)
{
	drawable_3d_base_render(n, rs, is_destroy, build_shape_sphere);
}

void render_init_sphere(Render *sr, GF_Node *node)
{
	drawable_3d_new(node);
	gf_node_set_callback_function(node, RenderSphere);
}

static void build_shape_point_set(GF_Node *n, Drawable3D *stack, GF_TraverseState *tr_state)
{
	M_PointSet *ps = (M_PointSet *)n;
	mesh_new_ps(stack->mesh, ps->coord, ps->color);
}
static void RenderPointSet(GF_Node *n, void *rs, Bool is_destroy)
{
	drawable_3d_base_render(n, rs, is_destroy, build_shape_point_set);
}

void render_init_point_set(Render *sr, GF_Node *node)
{
	drawable_3d_new(node);
	gf_node_set_callback_function(node, RenderPointSet);
}

static void build_shape_ifs(GF_Node *n, Drawable3D *stack, GF_TraverseState *tr_state)
{
	mesh_new_ifs(stack->mesh, n);
}
static void RenderIFS(GF_Node *n, void *rs, Bool is_destroy)
{
	drawable_3d_base_render(n, rs, is_destroy, build_shape_ifs);
}

static void IFS_SetColorIndex(GF_Node *node)
{
	M_IndexedFaceSet *ifs = (M_IndexedFaceSet *)node;
	gf_sg_vrml_field_copy(&ifs->colorIndex, &ifs->set_colorIndex, GF_SG_VRML_MFINT32);
	gf_sg_vrml_mf_reset(&ifs->set_colorIndex, GF_SG_VRML_MFINT32);
}

static void IFS_SetCoordIndex(GF_Node *node)
{
	M_IndexedFaceSet *ifs = (M_IndexedFaceSet *)node;
	gf_sg_vrml_field_copy(&ifs->coordIndex, &ifs->set_coordIndex, GF_SG_VRML_MFINT32);
	gf_sg_vrml_mf_reset(&ifs->set_coordIndex, GF_SG_VRML_MFINT32);
}

static void IFS_SetNormalIndex(GF_Node *node)
{
	M_IndexedFaceSet *ifs = (M_IndexedFaceSet *)node;
	gf_sg_vrml_field_copy(&ifs->normalIndex, &ifs->set_normalIndex, GF_SG_VRML_MFINT32);
	gf_sg_vrml_mf_reset(&ifs->set_normalIndex, GF_SG_VRML_MFINT32);
}

static void IFS_SetTexCoordIndex(GF_Node *node)
{
	M_IndexedFaceSet *ifs = (M_IndexedFaceSet *)node;
	gf_sg_vrml_field_copy(&ifs->texCoordIndex, &ifs->set_texCoordIndex, GF_SG_VRML_MFINT32);
	gf_sg_vrml_mf_reset(&ifs->set_texCoordIndex, GF_SG_VRML_MFINT32);
}

void render_init_ifs(Render *sr, GF_Node *node)
{
	M_IndexedFaceSet *ifs = (M_IndexedFaceSet *)node;
	drawable_3d_new(node);
	gf_node_set_callback_function(node, RenderIFS);
	ifs->on_set_colorIndex = IFS_SetColorIndex;
	ifs->on_set_coordIndex = IFS_SetCoordIndex;
	ifs->on_set_normalIndex = IFS_SetNormalIndex;
	ifs->on_set_texCoordIndex = IFS_SetTexCoordIndex;
}

static void build_shape_ils(GF_Node *n, Drawable3D *stack, GF_TraverseState *tr_state)
{
	M_IndexedLineSet *ils = (M_IndexedLineSet *)n;
	mesh_new_ils(stack->mesh, ils->coord, &ils->coordIndex, ils->color, &ils->colorIndex, ils->colorPerVertex, 0);
}

static void RenderILS(GF_Node *n, void *rs, Bool is_destroy)
{
	drawable_3d_base_render(n, rs, is_destroy, build_shape_ils);
}

static void ILS_SetColorIndex(GF_Node *node)
{
	M_IndexedLineSet *ils = (M_IndexedLineSet *)node;
	gf_sg_vrml_field_copy(&ils->colorIndex, &ils->set_colorIndex, GF_SG_VRML_MFINT32);
	gf_sg_vrml_mf_reset(&ils->set_colorIndex, GF_SG_VRML_MFINT32);
}

static void ILS_SetCoordIndex(GF_Node *node)
{
	M_IndexedLineSet *ils = (M_IndexedLineSet *)node;
	gf_sg_vrml_field_copy(&ils->coordIndex, &ils->set_coordIndex, GF_SG_VRML_MFINT32);
	gf_sg_vrml_mf_reset(&ils->set_coordIndex, GF_SG_VRML_MFINT32);
}

void render_init_ils(Render *sr, GF_Node *node)
{
	M_IndexedLineSet *ils = (M_IndexedLineSet *)node;
	drawable_3d_new(node);
	gf_node_set_callback_function(node, RenderILS);
	ils->on_set_colorIndex = ILS_SetColorIndex;
	ils->on_set_coordIndex = ILS_SetCoordIndex;
}


static void build_shape_elevation_grid(GF_Node *n, Drawable3D *stack, GF_TraverseState *tr_state)
{
	mesh_new_elevation_grid(stack->mesh, n);
}

static void RenderElevationGrid(GF_Node *n, void *rs, Bool is_destroy)
{
	drawable_3d_base_render(n, rs, is_destroy, build_shape_elevation_grid);
}

static void ElevationGrid_SetHeight(GF_Node *node)
{
	M_ElevationGrid *eg = (M_ElevationGrid *)node;
	gf_sg_vrml_field_copy(&eg->height, &eg->set_height, GF_SG_VRML_MFFLOAT);
	gf_sg_vrml_mf_reset(&eg->set_height, GF_SG_VRML_MFFLOAT);
}

void render_init_elevation_grid(Render *sr, GF_Node *node)
{
	M_ElevationGrid *eg = (M_ElevationGrid *)node;
	drawable_3d_new(node);
	gf_node_set_callback_function(node, RenderElevationGrid);
	eg->on_set_height = ElevationGrid_SetHeight;
}

static void build_shape_extrusion(GF_Node *n, Drawable3D *stack, GF_TraverseState *tr_state)
{
	mesh_new_extrusion(stack->mesh, n);
}

static void RenderExtrusion(GF_Node *n, void *rs, Bool is_destroy)
{
	drawable_3d_base_render(n, rs, is_destroy, build_shape_extrusion);
}

static void Extrusion_SetCrossSection(GF_Node *node)
{
	M_Extrusion *eg = (M_Extrusion *)node;
	gf_sg_vrml_field_copy(&eg->crossSection, &eg->set_crossSection, GF_SG_VRML_MFVEC2F);
	gf_sg_vrml_mf_reset(&eg->set_crossSection, GF_SG_VRML_MFVEC2F);
}
static void Extrusion_SetOrientation(GF_Node *node)
{
	M_Extrusion *eg = (M_Extrusion *)node;
	gf_sg_vrml_field_copy(&eg->orientation, &eg->set_orientation, GF_SG_VRML_MFROTATION);
	gf_sg_vrml_mf_reset(&eg->set_orientation, GF_SG_VRML_MFROTATION);
}
static void Extrusion_SetScale(GF_Node *node)
{
	M_Extrusion *eg = (M_Extrusion *)node;
	gf_sg_vrml_field_copy(&eg->scale, &eg->set_scale, GF_SG_VRML_MFVEC2F);
	gf_sg_vrml_mf_reset(&eg->set_scale, GF_SG_VRML_MFVEC2F);
}
static void Extrusion_SetSpine(GF_Node *node)
{
	M_Extrusion *eg = (M_Extrusion *)node;
	gf_sg_vrml_field_copy(&eg->spine, &eg->set_spine, GF_SG_VRML_MFVEC3F);
	gf_sg_vrml_mf_reset(&eg->set_spine, GF_SG_VRML_MFVEC3F);
}
void render_init_extrusion(Render *sr, GF_Node *node)
{
	M_Extrusion *ext = (M_Extrusion *)node;
	drawable_3d_new(node);
	gf_node_set_callback_function(node, RenderExtrusion);
	ext->on_set_crossSection = Extrusion_SetCrossSection;
	ext->on_set_orientation = Extrusion_SetOrientation;
	ext->on_set_scale = Extrusion_SetScale;
	ext->on_set_spine = Extrusion_SetSpine;
}


/*
			NonLinearDeformer

	NOTE: AFX spec is just hmm, well, hmm. NonLinearDeformer.extend interpretation differ from type to 
	type within the spec, and between the spec and the ref soft. This is GPAC interpretation
	* all params are specified with default transform axis (Z axis)
	* NLD.type = 0 (taper):
		* taping radius = NLD.param
		* extend = N * [diff, perc] with 
			- diff : relative position along taper axis (for default, diff=0: z min, diff=1: z max)
			- perc: mult ratio for base taper radius
		extend works like key/keyValue for a scalar interpolator
		final radius: r(z) = LinearInterp(extend[min key], extend[min key + 1]) * param

	* NLD.type = 1 (twister):
		* twisting angle = NLD.param
		* extend = N * [diff, perc] with 
			- diff : relative position along twister axis (for default, diff=0: z min, diff=1: z max)
			- perc: mult ratio for base twister angle
		extend works like key/keyValue for a scalar interpolator
		final angle: a(z) = LinearInterp(extend[min key], extend[min key + 1]) * param

	* NLD.type = 2 (bender):
		* bending curvature = NLD.param
		* extend = N * [diff, perc] with 
			- diff : relative position along bender axis (for default, diff=0: z min, diff=1: z max)
			- perc: mult ratio for base bender curvature
		extend works like key/keyValue for a scalar interpolator
		final curvature: c(z) = LinearInterp(extend[min key], extend[min key + 1]) * param

  Another pb of NLD is that the spec says nothing about object/axis alignment: should we center
  the object at 0,0,0 (local coords) or not? the results are quite different. Here we don't 
  recenter the object before transform
*/

static Bool NLD_GetMatrix(M_NonLinearDeformer *nld, GF_Matrix *mx)
{
	SFVec3f v1, v2;
	SFRotation r;
	Fixed l1, l2, dot;

	/*compute rotation matrix from NLD axis to 0 0 1*/
	v1 = nld->axis;
	gf_vec_norm(&v1);
	v2.x = v2.y = 0; v2.z = FIX_ONE;
	if (gf_vec_equal(v1, v2)) return 0;

	l1 = gf_vec_len(v1);
	l2 = gf_vec_len(v2);
	dot = gf_divfix(gf_vec_dot(v1, v2), gf_mulfix(l1, l2));

	r.x = gf_mulfix(v1.y, v2.z) - gf_mulfix(v2.y, v1.z);
	r.y = gf_mulfix(v1.z, v2.x) - gf_mulfix(v2.z, v1.x);
	r.z = gf_mulfix(v1.x, v2.y) - gf_mulfix(v2.x, v1.y);
	r.q = gf_atan2(gf_sqrt(FIX_ONE - gf_mulfix(dot, dot)), dot);
	gf_mx_init(*mx);
	gf_mx_add_rotation(mx, r.q, r.x, r.y, r.z);
	return 1;
}

static GFINLINE void NLD_GetKey(M_NonLinearDeformer *nld, Fixed frac, Fixed *f_min, Fixed *min, Fixed *f_max, Fixed *max)
{
	u32 i, count;
	count = nld->extend.count;
	if (count%2) count--;

	*f_min = 0;
	*min = 0;
	for (i=0; i<count; i+=2) {
		if (frac>=nld->extend.vals[i]) {
			*f_min = nld->extend.vals[i];
			*min = nld->extend.vals[i+1];
		} 
		if ((i+2<count) && (frac<=nld->extend.vals[i+2])) {
			*f_max = nld->extend.vals[i+2];
			*max = nld->extend.vals[i+3];
			return;
		}
	}
	if (count) {
		*f_max = nld->extend.vals[count-2];
		*max = nld->extend.vals[count-1];
	} else {
		*f_max = FIX_ONE;
		*max = GF_PI;
	}
}

static void NLD_Apply(M_NonLinearDeformer *nld, GF_Mesh *mesh)
{
	u32 i;
	GF_Matrix mx;
	Fixed param, z_min, z_max, v_min, v_max, f_min, f_max, frac, val, a_cos, a_sin;
	Bool needs_transform = NLD_GetMatrix(nld, &mx);

	param = nld->param;
	if (!param) param = 1;
	
	if (mesh->bounds.min_edge.z == mesh->bounds.max_edge.z) return;

	z_min = FIX_MAX;
	z_max = -FIX_MAX;
	if (needs_transform) {
		for (i=0; i<mesh->v_count; i++) {
			gf_mx_apply_vec(&mx, &mesh->vertices[i].pos);
			gf_mx_rotate_vector(&mx, &mesh->vertices[i].normal);
			if (mesh->vertices[i].pos.z<z_min) z_min = mesh->vertices[i].pos.z;
			if (mesh->vertices[i].pos.z>z_max) z_max = mesh->vertices[i].pos.z;
		}
	} else {
		z_min = mesh->bounds.min_edge.z;
		z_max = mesh->bounds.max_edge.z;
	}
	
	for (i=0; i<mesh->v_count; i++) {
		SFVec3f old = mesh->vertices[i].pos;
		frac = gf_divfix(old.z - z_min, z_max - z_min);
		NLD_GetKey(nld, frac, &f_min, &v_min, &f_max, &v_max);
		if (f_max == f_min) {
			val = v_min;
		} else {
			frac = gf_divfix(frac-f_min, f_max - f_min);
			val = gf_mulfix(v_max - v_min, frac) + v_min;
		}
		val = gf_mulfix(val, param);

		switch (nld->type) {
		/*taper*/
		case 0:
			mesh->vertices[i].pos.x = gf_mulfix(mesh->vertices[i].pos.x, val);
			mesh->vertices[i].pos.y = gf_mulfix(mesh->vertices[i].pos.y, val);
			old = mesh->vertices[i].normal;
			mesh->vertices[i].normal.x = gf_mulfix(mesh->vertices[i].normal.x, val);
			mesh->vertices[i].normal.y = gf_mulfix(mesh->vertices[i].normal.y, val);
			gf_vec_norm(&mesh->vertices[i].normal);
			break;
		/*twist*/
		case 1:
			a_cos = gf_cos(val);
			a_sin = gf_sin(val);
			mesh->vertices[i].pos.x = gf_mulfix(a_cos, old.x) - gf_mulfix(a_sin, old.y);
			mesh->vertices[i].pos.y = gf_mulfix(a_sin, old.x) + gf_mulfix(a_cos, old.y);
			old = mesh->vertices[i].normal;
			mesh->vertices[i].normal.x = gf_mulfix(a_cos, old.x) -  gf_mulfix(a_sin, old.y);
			mesh->vertices[i].normal.y = gf_mulfix(a_sin, old.x) + gf_mulfix(a_cos, old.y);
			gf_vec_norm(&mesh->vertices[i].normal);
			break;
		/*bend*/
		case 2:
			a_cos = gf_cos(val);
			a_sin = gf_sin(val);
			mesh->vertices[i].pos.x = gf_mulfix(a_sin, old.z) + gf_mulfix(a_cos, old.x);
			mesh->vertices[i].pos.z = gf_mulfix(a_cos, old.z) - gf_mulfix(a_sin, old.x);
			old = mesh->vertices[i].normal;
			mesh->vertices[i].normal.x = gf_mulfix(a_sin, old.z) +  gf_mulfix(a_cos, old.x);
			mesh->vertices[i].normal.z = gf_mulfix(a_cos, old.z) - gf_mulfix(a_sin, old.x);
			gf_vec_norm(&mesh->vertices[i].normal);
			break;
		/*pinch, not standard  (taper on X dim only)*/
		case 3:
			mesh->vertices[i].pos.x = gf_mulfix(mesh->vertices[i].pos.x, val);
			old = mesh->vertices[i].normal;
			mesh->vertices[i].normal.x = gf_mulfix(mesh->vertices[i].normal.x, val);
			gf_vec_norm(&mesh->vertices[i].normal);
			break;
		}
	}
	if (needs_transform) {
		gf_mx_inverse(&mx);
		for (i=0; i<mesh->v_count; i++) {
			gf_mx_apply_vec(&mx, &mesh->vertices[i].pos);
			gf_mx_rotate_vector(&mx, &mesh->vertices[i].normal);
		}
	}
	mesh_update_bounds(mesh);
	gf_mesh_build_aabbtree(mesh);
}

static void build_shape_nld(GF_Node *n, Drawable3D *stack, GF_TraverseState *tr_state)
{
	M_NonLinearDeformer *nld = (M_NonLinearDeformer*)n;
	Drawable3D *geo_st = (Drawable3D *)gf_node_get_private(nld->geometry);

	if (!nld->geometry) return;
	if (!geo_st) return;

	mesh_clone(stack->mesh, geo_st->mesh);
	/*apply deforms*/
	NLD_Apply(nld, stack->mesh);
}

static void RenderNonLinearDeformer(GF_Node *n, void *rs, Bool is_destroy)
{
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;

	/*call render on geometry for get_bounds to make sure geometry is up to date*/
	if (!is_destroy && (tr_state->traversing_mode==TRAVERSE_GET_BOUNDS)) {
		gf_node_render(((M_NonLinearDeformer*)n)->geometry, tr_state);
	}

	drawable_3d_base_render(n, rs, is_destroy, build_shape_nld);
}

void render_init_non_linear_deformer(Render *sr, GF_Node *node)
{
	drawable_3d_new(node);
	gf_node_set_callback_function(node, RenderNonLinearDeformer);
}

#endif /*GPAC_DISABLE_3D*/
