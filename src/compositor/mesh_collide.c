/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
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


#include <gpac/internal/mesh.h>

#if !defined(GPAC_DISABLE_3D) && !defined(GPAC_DISABLE_EVG)

/*
	AABB tree syntax&construction code is a quick extract from OPCODE (c) Pierre Terdiman, http://www.codercorner.com/Opcode.htm
*/

typedef struct
{
	/*max tree depth, 0 is unlimited*/
	u32 max_depth;
	/*min triangles at node to split. 0 is full split (one triangle per leaf)*/
	u32 min_tri_limit;
	/*one of the above type*/
	u32 split_type;
	u32 depth, nb_nodes;
} AABSplitParams;



static GFINLINE void update_node_bounds(GF_Mesh *mesh, AABBNode *node)
{
	u32 i, j;
	Fixed mx, my, mz, Mx, My, Mz;
	mx = my = mz = FIX_MAX;
	Mx = My = Mz = FIX_MIN;
	for (i=0; i<node->nb_idx; i++) {
		IDX_TYPE *idx = &mesh->indices[3*node->indices[i]];
		for (j=0; j<3; j++) {
			SFVec3f *v = &mesh->vertices[idx[j]].pos;
			if (mx>v->x) mx=v->x;
			if (Mx<v->x) Mx=v->x;
			if (my>v->y) my=v->y;
			if (My<v->y) My=v->y;
			if (mz>v->z) mz=v->z;
			if (Mz<v->z) Mz=v->z;
		}
	}
	node->min.x = mx;
	node->min.y = my;
	node->min.z = mz;
	node->max.x = Mx;
	node->max.y = My;
	node->max.z = Mz;
}

static GFINLINE u32 gf_vec_main_axis(SFVec3f v)
{
	Fixed *vals = &v.x;
	u32 m = 0;
	if(vals[1] > vals[m]) m = 1;
	if(vals[2] > vals[m]) m = 2;
	return m;
}

static GFINLINE Fixed tri_get_center(GF_Mesh *mesh, u32 tri_idx, u32 axis)
{
	SFVec3f v;
	IDX_TYPE *idx = &mesh->indices[3*tri_idx];
	/*compute center*/
	gf_vec_add(v, mesh->vertices[idx[0]].pos, mesh->vertices[idx[1]].pos);
	gf_vec_add(v, v, mesh->vertices[idx[2]].pos);
	v = gf_vec_scale(v, FIX_ONE/3);
	return ((Fixed *)&v.x)[axis];
}

static GFINLINE u32 aabb_split(GF_Mesh *mesh, AABBNode *node, u32 axis)
{
	SFVec3f v;
	Fixed split_at;
	u32 num_pos, i;

	gf_vec_add(v, node->max, node->min);
	v = gf_vec_scale(v, FIX_ONE/2);

	split_at = ((Fixed *)&v.x)[axis];

	num_pos = 0;

	for (i=0; i<node->nb_idx; i++) {
		IDX_TYPE idx = node->indices[i];
		Fixed tri_val = tri_get_center(mesh, idx, axis);

		if (tri_val > split_at) {
			/*swap*/
			IDX_TYPE tmp_idx = node->indices[i];
			node->indices[i] = node->indices[num_pos];
			node->indices[num_pos] = tmp_idx;
			num_pos++;
		}
	}
	return num_pos;
}

static void mesh_subdivide_aabbtree(GF_Mesh *mesh, AABBNode *node, AABSplitParams *aab_par)
{
	Bool do_split;
	u32 axis, num_pos, i, j;
	SFVec3f extend;

if (!mesh || !node) return;
	/*update mesh depth*/
	aab_par->depth ++;

	/*done*/
	if (node->nb_idx==1) return;
	if (node->nb_idx <= aab_par->min_tri_limit) return;
	if (aab_par->max_depth == aab_par->depth) {
		aab_par->depth --;
		return;
	}
	do_split = GF_TRUE;

	gf_vec_diff(extend, node->max, node->min);
	extend = gf_vec_scale(extend, FIX_ONE/2);
	axis = gf_vec_main_axis(extend);

	num_pos = 0;
	if (aab_par->split_type==AABB_LONGEST) {
		num_pos = aabb_split(mesh, node, axis);
	}
	else if (aab_par->split_type==AABB_BALANCED) {
		Fixed res[3];
		num_pos = aabb_split(mesh, node, 0);
		res[0] = gf_divfix(INT2FIX(num_pos), INT2FIX(node->nb_idx)) - FIX_ONE/2;
		res[0] = gf_mulfix(res[0], res[0]);
		num_pos = aabb_split(mesh, node, 1);
		res[1] = gf_divfix(INT2FIX(num_pos), INT2FIX(node->nb_idx)) - FIX_ONE/2;
		res[1] = gf_mulfix(res[1], res[1]);
		num_pos = aabb_split(mesh, node, 2);
		res[2] = gf_divfix(INT2FIX(num_pos), INT2FIX(node->nb_idx)) - FIX_ONE/2;
		res[2] = gf_mulfix(res[2], res[2]);

		axis = 0;
		if (res[1] < res[axis])	axis = 1;
		if (res[2] < res[axis])	axis = 2;
		num_pos = aabb_split(mesh, node, axis);
	}
	else if (aab_par->split_type==AABB_BEST_AXIS) {
		u32 sorted[] = { 0, 1, 2 };
		Fixed *Keys = (Fixed *) &extend.x;
		for (j=0; j<3; j++) {
			for (i=0; i<2; i++) {
				if (Keys[sorted[i]] < Keys[sorted[i+1]]) {
					u32 tmp = sorted[i];
					sorted[i] = sorted[i+1];
					sorted[i+1] = tmp;
				}
			}
		}
		axis = 0;
		do_split = GF_FALSE;
		while (!do_split && (axis!=3)) {
			num_pos = aabb_split(mesh, node, sorted[axis]);
			// Check the subdivision has been successful
			if (!num_pos || (num_pos==node->nb_idx)) axis++;
			else do_split = GF_TRUE;
		}
	}
	else if (aab_par->split_type==AABB_SPLATTER) {
		SFVec3f means, vars;
		means.x = means.y = means.z = 0;
		for (i=0; i<node->nb_idx; i++) {
			IDX_TYPE idx = node->indices[i];
			means.x += tri_get_center(mesh, idx, 0);
			means.y += tri_get_center(mesh, idx, 1);
			means.z += tri_get_center(mesh, idx, 2);
		}
		means = gf_vec_scale(means, gf_invfix(node->nb_idx));

		vars.x = vars.y = vars.z = 0;
		for (i=0; i<node->nb_idx; i++) {
			IDX_TYPE idx = node->indices[i];
			Fixed cx = tri_get_center(mesh, idx, 0);
			Fixed cy = tri_get_center(mesh, idx, 1);
			Fixed cz = tri_get_center(mesh, idx, 2);
			vars.x += gf_mulfix(cx - means.x, cx - means.x);
			vars.y += gf_mulfix(cy - means.y, cy - means.y);
			vars.z += gf_mulfix(cz - means.z, cz - means.z);
		}
		vars = gf_vec_scale(vars, gf_invfix(node->nb_idx-1) );
		axis = gf_vec_main_axis(vars);
		num_pos = aabb_split(mesh, node, axis);
	}
	else if (aab_par->split_type==AABB_FIFTY) {
		do_split = GF_FALSE;
	}

	if (!num_pos || (num_pos==node->nb_idx)) do_split = GF_FALSE;

	if (!do_split) {
		if (aab_par->split_type==AABB_FIFTY) {
			num_pos = node->nb_idx/2;
		} else {
			return;
		}
	}
	aab_par->nb_nodes += 2;

	GF_SAFEALLOC(node->pos, AABBNode);
	if (node->pos) {
		node->pos->indices = &node->indices[0];
		node->pos->nb_idx = num_pos;
		update_node_bounds(mesh, node->pos);
		mesh_subdivide_aabbtree(mesh, node->pos, aab_par);
	}

	GF_SAFEALLOC(node->neg, AABBNode);
	if (node->neg) {
		node->neg->indices = &node->indices[num_pos];
		node->neg->nb_idx = node->nb_idx - num_pos;
		update_node_bounds(mesh, node->neg);
		mesh_subdivide_aabbtree(mesh, node->neg, aab_par);
	}
	aab_par->depth --;
}


void gf_mesh_build_aabbtree(GF_Mesh *mesh)
{
	u32 i, nb_idx;
	AABSplitParams pars;

	memset(&pars, 0, sizeof(pars));
	pars.min_tri_limit = 8;
	pars.max_depth = 0;
	pars.split_type = AABB_BALANCED;

	if (mesh->i_count <= pars.min_tri_limit) return;

	nb_idx = mesh->i_count / 3;
	mesh->aabb_indices = (IDX_TYPE*)gf_malloc(sizeof(IDX_TYPE) * nb_idx);
	for (i=0; i<nb_idx; i++) mesh->aabb_indices[i] = i;

	GF_SAFEALLOC(mesh->aabb_root, AABBNode);
	if (mesh->aabb_root) {
		mesh->aabb_root->min = mesh->bounds.min_edge;
		mesh->aabb_root->max = mesh->bounds.max_edge;
		mesh->aabb_root->indices = mesh->aabb_indices;
		mesh->aabb_root->nb_idx = nb_idx;
	}
	pars.nb_nodes = 1;
	pars.depth = 0;
	mesh_subdivide_aabbtree(mesh, mesh->aabb_root, &pars);

	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Mesh] AABB tree done - %d nodes depth %d - size %d bytes\n", pars.nb_nodes, pars.depth, sizeof(AABBNode)*pars.nb_nodes));
}


static void ray_hit_triangle_get_u_v(GF_Ray *ray, GF_Vec *v0, GF_Vec *v1, GF_Vec *v2, Fixed *u, Fixed *v)
{
	Fixed det;
	GF_Vec edge1, edge2, tvec, pvec, qvec;
	/* find vectors for two edges sharing vert0 */
	gf_vec_diff(edge1, *v1, *v0);
	gf_vec_diff(edge2, *v2, *v0);
	/* begin calculating determinant - also used to calculate U parameter */
	pvec = gf_vec_cross(ray->dir, edge2);
	/* if determinant is near zero, ray lies in plane of triangle */
	det = gf_vec_dot(edge1, pvec);
	/* calculate distance from vert0 to ray origin */
	gf_vec_diff(tvec, ray->orig, *v0);
	/* calculate U parameter and test bounds */
	(*u) = gf_divfix(gf_vec_dot(tvec, pvec), det);
	/* prepare to test V parameter */
	qvec = gf_vec_cross(tvec, edge1);
	/* calculate V parameter and test bounds */
	(*v) = gf_divfix(gf_vec_dot(ray->dir, qvec), det);
}

Bool gf_mesh_aabb_ray_hit(GF_Mesh *mesh, AABBNode *n, GF_Ray *ray, Fixed *closest, SFVec3f *outPoint, SFVec3f *outNormal, SFVec2f *outTexCoords)
{
	Bool inters;
	Fixed dist;
	SFVec3f v1, v2;
	u32 i, inters_idx;

	/*check bbox intersection*/
	inters = gf_ray_hit_box(ray, n->min, n->max, NULL);
	if (!inters) return GF_FALSE;

	if (n->pos) {
		/*we really want to check all possible intersections to get the closest point on ray*/
		Bool res = gf_mesh_aabb_ray_hit(mesh, n->pos, ray, closest, outPoint, outNormal, outTexCoords);
		res += gf_mesh_aabb_ray_hit(mesh, n->neg, ray, closest, outPoint, outNormal, outTexCoords);
		return res;

	}


	inters_idx = 0;
	inters = GF_FALSE;
	dist = (*closest);

	/*leaf, check for all faces*/
	for (i=0; i<n->nb_idx; i++) {
		Fixed res;
		IDX_TYPE *idx = &mesh->indices[3*n->indices[i]];
		if (gf_ray_hit_triangle(ray,
		                        &mesh->vertices[idx[0]].pos, &mesh->vertices[idx[1]].pos, &mesh->vertices[idx[2]].pos,
		                        &res)) {
			if ((res>0) && (res<dist)) {
				dist = res;
				inters_idx = i;
				inters = GF_TRUE;
			}
		}
	}

	if (inters) {
		(*closest) = dist;
		if (outPoint) {
			*outPoint = gf_vec_scale(ray->dir, dist);
			gf_vec_add(*outPoint, ray->orig, *outPoint);
		}
		if (outNormal) {
			IDX_TYPE *idx = &mesh->indices[3*n->indices[inters_idx]];
			if (mesh->flags & MESH_IS_SMOOTHED) {
				gf_vec_diff(v1, mesh->vertices[idx[1]].pos, mesh->vertices[idx[0]].pos);
				gf_vec_diff(v2, mesh->vertices[idx[2]].pos, mesh->vertices[idx[0]].pos);
				*outNormal = gf_vec_cross(v1, v2);
				gf_vec_norm(outNormal);
			} else {
				MESH_GET_NORMAL((*outNormal), mesh->vertices[idx[0]]);
			}
		}
		if (outTexCoords) {
			IDX_TYPE *idx = &mesh->indices[3*n->indices[inters_idx]];
			ray_hit_triangle_get_u_v(ray,
			                         &mesh->vertices[idx[0]].pos, &mesh->vertices[idx[1]].pos, &mesh->vertices[idx[2]].pos,
			                         &outTexCoords->x, &outTexCoords->y);
		}
	}
	return inters;
}

Bool gf_mesh_intersect_ray(GF_Mesh *mesh, GF_Ray *ray, SFVec3f *outPoint, SFVec3f *outNormal, SFVec2f *outTexCoords)
{
	Bool inters;
	u32 i, inters_idx;
	Fixed closest;
	/*no intersection on linesets/pointsets*/
	if (mesh->mesh_type != MESH_TRIANGLES) return GF_FALSE;

	/*use aabbtree*/
	if (mesh->aabb_root) {
		closest = FIX_MAX;
		return gf_mesh_aabb_ray_hit(mesh, mesh->aabb_root, ray, &closest, outPoint, outNormal, outTexCoords);
	}

	/*check bbox intersection*/
	inters = gf_ray_hit_box(ray, mesh->bounds.min_edge, mesh->bounds.max_edge, NULL);
	if (!inters) return GF_FALSE;

	inters_idx = 0;
	inters = GF_FALSE;
	closest = FIX_MAX;
	for (i=0; i<mesh->i_count; i+=3) {
		Fixed res;
		IDX_TYPE *idx = &mesh->indices[i];
		if (gf_ray_hit_triangle(ray,
		                        &mesh->vertices[idx[0]].pos, &mesh->vertices[idx[1]].pos, &mesh->vertices[idx[2]].pos,
		                        &res)) {
			if ((res>0) && (res<closest)) {
				closest = res;
				inters_idx = i;
				inters = GF_TRUE;
			}
		}
	}

	if (inters) {
		if (outPoint) {
			*outPoint = gf_vec_scale(ray->dir, closest);
			gf_vec_add(*outPoint, ray->orig, *outPoint);
		}
		if (outNormal) {
			IDX_TYPE *idx = &mesh->indices[inters_idx];
			if (mesh->flags & MESH_IS_SMOOTHED) {
				SFVec3f v1, v2;
				gf_vec_diff(v1, mesh->vertices[idx[1]].pos, mesh->vertices[idx[0]].pos);
				gf_vec_diff(v2, mesh->vertices[idx[2]].pos, mesh->vertices[idx[0]].pos);
				*outNormal = gf_vec_cross(v1, v2);
				gf_vec_norm(outNormal);
			} else {
				MESH_GET_NORMAL((*outNormal), mesh->vertices[idx[0]]);
			}
		}
		if (outTexCoords) {
			SFVec2f txres;
			IDX_TYPE *idx = &mesh->indices[inters_idx];
			txres.x = txres.y = 0;
			txres.x += mesh->vertices[idx[0]].texcoords.x;
			txres.x += mesh->vertices[idx[1]].texcoords.x;
			txres.x += mesh->vertices[idx[2]].texcoords.x;
			txres.y += mesh->vertices[idx[0]].texcoords.y;
			txres.y += mesh->vertices[idx[1]].texcoords.y;
			txres.y += mesh->vertices[idx[2]].texcoords.y;
			outTexCoords->x = txres.x / 3;
			outTexCoords->y = txres.y / 3;
		}
	}
	return inters;
}


static GFINLINE Bool mesh_collide_triangle(GF_Ray *ray, SFVec3f *v0, SFVec3f *v1, SFVec3f *v2, Fixed *dist)
{
	Fixed u, v, det;
	SFVec3f edge1, edge2, tvec, pvec, qvec;
	/* find vectors for two edges sharing vert0 */
	gf_vec_diff(edge1, *v1, *v0);
	gf_vec_diff(edge2, *v2, *v0);
	/* begin calculating determinant - also used to calculate U parameter */
	pvec = gf_vec_cross(ray->dir, edge2);
	/* if determinant is near zero, ray lies in plane of triangle */
	det = gf_vec_dot(edge1, pvec);
	if (ABS(det) < FIX_EPSILON) return GF_FALSE;
	/* calculate distance from vert0 to ray origin */
	gf_vec_diff(tvec, ray->orig, *v0);
	/* calculate U parameter and test bounds */
	u = gf_divfix(gf_vec_dot(tvec, pvec), det);
	if ((u < 0) || (u > FIX_ONE)) return GF_FALSE;
	/* prepare to test V parameter */
	qvec = gf_vec_cross(tvec, edge1);
	/* calculate V parameter and test bounds */
	v = gf_divfix(gf_vec_dot(ray->dir, qvec), det);
	if ((v < 0) || (u + v > FIX_ONE)) return GF_FALSE;
	/* calculate t, ray intersects triangle */
	*dist = gf_divfix(gf_vec_dot(edge2, qvec), det);
	return GF_TRUE;
}


static GFINLINE Bool sphere_box_overlap(SFVec3f sc, Fixed sq_rad, SFVec3f bmin, SFVec3f bmax)
{
	Fixed tmp, s, d, ext;
	d = 0;
	tmp = sc.x  - (bmin.x + bmax.x)/2;
	ext = (bmax.x-bmin.x)/2;
	s = tmp + ext;
	if (s<0) d += gf_mulfix(s, s);
	else {
		s = tmp - ext;
		if (s>0) d += gf_mulfix(s, s);
	}
	tmp = sc.y  - (bmin.y + bmax.y)/2;
	ext = (bmax.y-bmin.y)/2;
	s = tmp + ext;
	if (s<0) d += gf_mulfix(s, s);
	else {
		s = tmp - ext;
		if (s>0) d += gf_mulfix(s, s);
	}
	tmp = sc.z  - (bmin.z + bmax.z)/2;
	ext = (bmax.z-bmin.z)/2;
	s = tmp + ext;
	if (s<0) d += gf_mulfix(s, s);
	else {
		s = tmp - ext;
		if (s>0) d += gf_mulfix(s, s);
	}
	return (d<=sq_rad) ? GF_TRUE : GF_FALSE;
}

Bool gf_mesh_closest_face_aabb(GF_Mesh *mesh, AABBNode *node, SFVec3f pos, Fixed min_dist, Fixed min_sq_dist, Fixed *min_col_dist, SFVec3f *outPoint)
{
	GF_Ray r;
	u32 i;
	SFVec3f v1, v2, n, resn;
	Bool inters, has_inter, need_norm = GF_FALSE;
	Fixed d;
	if (!sphere_box_overlap(pos, min_sq_dist, node->min, node->max)) return GF_FALSE;
	if (node->pos) {
		if (gf_mesh_closest_face_aabb(mesh, node->pos, pos, min_dist, min_sq_dist, min_col_dist, outPoint)) return GF_TRUE;
		return gf_mesh_closest_face_aabb(mesh, node->neg, pos, min_dist, min_sq_dist, min_col_dist, outPoint);
	}

	need_norm = (mesh->flags & MESH_IS_SMOOTHED) ? GF_TRUE : GF_FALSE;
	r.orig = pos;
	has_inter = GF_FALSE;
	for (i=0; i<node->nb_idx; i++) {
		IDX_TYPE *idx = &mesh->indices[3*node->indices[i]];
		if (need_norm) {
			gf_vec_diff(v1, mesh->vertices[idx[1]].pos, mesh->vertices[idx[0]].pos);
			gf_vec_diff(v2, mesh->vertices[idx[2]].pos, mesh->vertices[idx[0]].pos);
			n = gf_vec_cross(v1, v2);
			gf_vec_norm(&n);
		} else {
			MESH_GET_NORMAL(n, mesh->vertices[idx[0]]);
		}

		/*intersect inverse normal from position to face with face*/
		r.dir = n;
		gf_vec_rev(r.dir);
		inters = mesh_collide_triangle(&r,
		                               &mesh->vertices[idx[0]].pos, &mesh->vertices[idx[1]].pos, &mesh->vertices[idx[2]].pos,
		                               &d);

		if (inters) {
			/*we're behind the face, get inverse normal*/
			if (d<0) {
				d*= -1;
				n = r.dir;
			}
			if (d<=(*min_col_dist)) {
				has_inter = GF_TRUE;
				(*min_col_dist) = d;
				resn = n;
			}
		}
	}
	if (has_inter) {
		resn = gf_vec_scale(resn, -(*min_col_dist));
		gf_vec_add(*outPoint, pos, resn);
	}
	return has_inter;
}

Bool gf_mesh_closest_face(GF_Mesh *mesh, SFVec3f pos, Fixed min_dist, SFVec3f *outPoint)
{
	GF_Ray r;
	u32 i;
	SFVec3f v1, v2, n, resn;
	Bool inters, has_inter, need_norm;
	Fixed d, dmax;

	/*check bounds*/
	gf_vec_diff(v1, mesh->bounds.center, pos);
	if (gf_vec_len(v1)>min_dist+mesh->bounds.radius) return GF_FALSE;

	if (mesh->aabb_root) {
		d = min_dist * min_dist;
		dmax = min_dist;
		return gf_mesh_closest_face_aabb(mesh, mesh->aabb_root, pos, min_dist, d, &dmax, outPoint);
	}

	need_norm = (mesh->flags & MESH_IS_SMOOTHED) ? GF_TRUE : GF_FALSE;

	r.orig = pos;
	has_inter = GF_FALSE;
	dmax = min_dist;
	for (i=0; i<mesh->i_count; i+=3) {
		IDX_TYPE *idx = &mesh->indices[i];
		if (need_norm) {
			gf_vec_diff(v1, mesh->vertices[idx[1]].pos, mesh->vertices[idx[0]].pos);
			gf_vec_diff(v2, mesh->vertices[idx[2]].pos, mesh->vertices[idx[0]].pos);
			n = gf_vec_cross(v1, v2);
			gf_vec_norm(&n);
		} else {
			MESH_GET_NORMAL(n, mesh->vertices[idx[0]]);
			n.x = mesh->vertices[idx[0]].normal.x;
			n.y = mesh->vertices[idx[0]].normal.y;
			n.z = mesh->vertices[idx[0]].normal.z;
		}

		d = -gf_vec_dot(mesh->vertices[idx[0]].pos, n);
		d += gf_vec_dot(r.orig, n);
		if (fabs(d)>min_dist) continue;

		/*intersect inverse normal from position to face with face*/
		r.dir = n;
		gf_vec_rev(r.dir);
		inters = mesh_collide_triangle(&r,
		                               &mesh->vertices[idx[0]].pos, &mesh->vertices[idx[1]].pos, &mesh->vertices[idx[2]].pos,
		                               &d);

		if (inters) {
			/*we're behind the face, get inverse normal*/
			if (d<0) {
				d*= -1;
				n = r.dir;
			}
			if (d<=dmax) {
				has_inter = GF_TRUE;
				dmax = d;
				resn = n;
			}
		}
	}
	if (has_inter) {
		resn = gf_vec_scale(resn, -dmax);
		gf_vec_add(*outPoint, pos, resn);
	}
	return has_inter;
}

#endif //!defined(GPAC_DISABLE_3D) && !defined(GPAC_DISABLE_EVG)
