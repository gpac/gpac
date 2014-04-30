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

#include "nodes_stacks.h"
#include "visual_manager.h"

#include "offscreen_cache.h"
#include "mpeg4_grouping.h"

#include <gpac/modules/hardcoded_proto.h>
#include <gpac/internal/terminal_dev.h>

#ifndef GPAC_DISABLE_VRML

#ifndef GPAC_DISABLE_3D

/*PathExtrusion hardcoded proto*/

typedef struct
{
	GF_Node *geometry;
	MFVec3f *spine;
	Bool beginCap;
	Bool endCap;
	Fixed creaseAngle;
	MFRotation *orientation;
	MFVec2f *scale;
	Bool txAlongSpine;
} PathExtrusion;

static Bool PathExtrusion_GetNode(GF_Node *node, PathExtrusion *path_ext)
{
	GF_FieldInfo field;
	memset(path_ext, 0, sizeof(PathExtrusion));
	if (gf_node_get_field(node, 0, &field) != GF_OK) return 0;
	if (field.fieldType != GF_SG_VRML_SFNODE) return 0;
	path_ext->geometry = * (GF_Node **) field.far_ptr;
	if (gf_node_get_field(node, 1, &field) != GF_OK) return 0;
	if (field.fieldType != GF_SG_VRML_MFVEC3F) return 0;
	path_ext->spine = (MFVec3f *) field.far_ptr;
	if (gf_node_get_field(node, 2, &field) != GF_OK) return 0;
	if (field.fieldType != GF_SG_VRML_SFBOOL) return 0;
	path_ext->beginCap = *(SFBool *) field.far_ptr;
	if (gf_node_get_field(node, 3, &field) != GF_OK) return 0;
	if (field.fieldType != GF_SG_VRML_SFBOOL) return 0;
	path_ext->endCap = *(SFBool *) field.far_ptr;
	if (gf_node_get_field(node, 4, &field) != GF_OK) return 0;
	if (field.fieldType != GF_SG_VRML_SFFLOAT) return 0;
	path_ext->creaseAngle = *(SFFloat *) field.far_ptr;
	if (gf_node_get_field(node, 5, &field) != GF_OK) return 0;
	if (field.fieldType != GF_SG_VRML_MFROTATION) return 0;
	path_ext->orientation = (MFRotation *) field.far_ptr;
	if (gf_node_get_field(node, 6, &field) != GF_OK) return 0;
	if (field.fieldType != GF_SG_VRML_MFVEC2F) return 0;
	path_ext->scale = (MFVec2f *) field.far_ptr;
	if (gf_node_get_field(node, 7, &field) != GF_OK) return 0;
	if (field.fieldType != GF_SG_VRML_SFBOOL) return 0;
	path_ext->txAlongSpine = *(SFBool *) field.far_ptr;
	return 1;
}

static void TraversePathExtrusion(GF_Node *node, void *rs, Bool is_destroy)
{
	PathExtrusion path_ext;
	Drawable *stack_2d;
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;
	Drawable3D *stack = (Drawable3D *)gf_node_get_private(node);

	if (is_destroy) {
		drawable_3d_del(node);
		return;
	}
	if (!PathExtrusion_GetNode(node, &path_ext)) return;
	if (!path_ext.geometry) return;


	if (gf_node_dirty_get(node)) {
		u32 mode = tr_state->traversing_mode;
		tr_state->traversing_mode = TRAVERSE_GET_BOUNDS;
		gf_node_traverse(path_ext.geometry, tr_state);
		tr_state->traversing_mode = mode;


		switch (gf_node_get_tag(path_ext.geometry) ) {
		case TAG_MPEG4_Circle:
		case TAG_MPEG4_Ellipse:
		case TAG_MPEG4_Rectangle:
		case TAG_MPEG4_Curve2D:
		case TAG_MPEG4_XCurve2D:
		case TAG_MPEG4_IndexedFaceSet2D:
		case TAG_MPEG4_IndexedLineSet2D:
			stack_2d = (Drawable*)gf_node_get_private(path_ext.geometry);
			if (!stack_2d) return;
			mesh_extrude_path(stack->mesh, stack_2d->path, path_ext.spine, path_ext.creaseAngle, path_ext.beginCap, path_ext.endCap, path_ext.orientation, path_ext.scale, path_ext.txAlongSpine);
			break;
		case TAG_MPEG4_Text:
			compositor_extrude_text(path_ext.geometry, tr_state, stack->mesh, path_ext.spine, path_ext.creaseAngle, path_ext.beginCap, path_ext.endCap, path_ext.orientation, path_ext.scale, path_ext.txAlongSpine);
			break;
		}
	}

	if (tr_state->traversing_mode==TRAVERSE_DRAW_3D) {
		visual_3d_draw(tr_state, stack->mesh);
	} else if (tr_state->traversing_mode==TRAVERSE_GET_BOUNDS) {
		tr_state->bbox = stack->mesh->bounds;
	}
}

static void compositor_init_path_extrusion(GF_Compositor *compositor, GF_Node *node)
{
	drawable_3d_new(node);
	gf_node_set_callback_function(node, TraversePathExtrusion);
}


/*PlanarExtrusion hardcoded proto*/
typedef struct
{
	GF_Node *geometry;
	GF_Node *spine;
	Bool beginCap;
	Bool endCap;
	Fixed creaseAngle;
	MFFloat *orientationKeys;
	MFRotation *orientation;
	MFFloat *scaleKeys;
	MFVec2f *scale;
	Bool txAlongSpine;
} PlanarExtrusion;

static Bool PlanarExtrusion_GetNode(GF_Node *node, PlanarExtrusion *path_ext)
{
	GF_FieldInfo field;
	memset(path_ext, 0, sizeof(PathExtrusion));
	if (gf_node_get_field(node, 0, &field) != GF_OK) return 0;
	if (field.fieldType != GF_SG_VRML_SFNODE) return 0;
	path_ext->geometry = * (GF_Node **) field.far_ptr;
	if (gf_node_get_field(node, 1, &field) != GF_OK) return 0;
	if (field.fieldType != GF_SG_VRML_SFNODE) return 0;
	path_ext->spine = * (GF_Node **) field.far_ptr;
	if (gf_node_get_field(node, 2, &field) != GF_OK) return 0;
	if (field.fieldType != GF_SG_VRML_SFBOOL) return 0;
	path_ext->beginCap = *(SFBool *) field.far_ptr;
	if (gf_node_get_field(node, 3, &field) != GF_OK) return 0;
	if (field.fieldType != GF_SG_VRML_SFBOOL) return 0;
	path_ext->endCap = *(SFBool *) field.far_ptr;
	if (gf_node_get_field(node, 4, &field) != GF_OK) return 0;
	if (field.fieldType != GF_SG_VRML_SFFLOAT) return 0;
	path_ext->creaseAngle = *(SFFloat *) field.far_ptr;
	if (gf_node_get_field(node, 5, &field) != GF_OK) return 0;
	if (field.fieldType != GF_SG_VRML_MFFLOAT) return 0;
	path_ext->orientationKeys = (MFFloat *) field.far_ptr;
	if (gf_node_get_field(node, 6, &field) != GF_OK) return 0;
	if (field.fieldType != GF_SG_VRML_MFROTATION) return 0;
	path_ext->orientation = (MFRotation *) field.far_ptr;
	if (gf_node_get_field(node, 7, &field) != GF_OK) return 0;
	if (field.fieldType != GF_SG_VRML_MFFLOAT) return 0;
	path_ext->scaleKeys = (MFFloat *) field.far_ptr;
	if (gf_node_get_field(node, 8, &field) != GF_OK) return 0;
	if (field.fieldType != GF_SG_VRML_MFVEC2F) return 0;
	path_ext->scale = (MFVec2f *) field.far_ptr;
	if (gf_node_get_field(node, 9, &field) != GF_OK) return 0;
	if (field.fieldType != GF_SG_VRML_SFBOOL) return 0;
	path_ext->txAlongSpine = *(SFBool *) field.far_ptr;
	return 1;
}

static void TraversePlanarExtrusion(GF_Node *node, void *rs, Bool is_destroy)
{
	PlanarExtrusion plane_ext;
	Drawable *stack_2d;
	u32 i, j, k;
	MFVec3f spine_vec;
	SFVec3f d;
	Fixed spine_len;
	GF_Rect bounds;
	GF_Path *geo, *spine;
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;
	Drawable3D *stack = (Drawable3D *)gf_node_get_private(node);

	if (is_destroy) {
		drawable_3d_del(node);
		return;
	}

	if (!PlanarExtrusion_GetNode(node, &plane_ext)) return;
	if (!plane_ext.geometry || !plane_ext.spine) return;


	if (gf_node_dirty_get(node)) {
		u32 cur, nb_pts;
		u32 mode = tr_state->traversing_mode;
		geo = spine = NULL;

		tr_state->traversing_mode = TRAVERSE_GET_BOUNDS;
		gf_node_traverse(plane_ext.geometry, tr_state);
		gf_node_traverse(plane_ext.spine, tr_state);
		tr_state->traversing_mode = mode;

		switch (gf_node_get_tag(plane_ext.geometry) ) {
		case TAG_MPEG4_Circle:
		case TAG_MPEG4_Ellipse:
		case TAG_MPEG4_Rectangle:
		case TAG_MPEG4_Curve2D:
		case TAG_MPEG4_XCurve2D:
		case TAG_MPEG4_IndexedFaceSet2D:
		case TAG_MPEG4_IndexedLineSet2D:
			stack_2d = (Drawable*)gf_node_get_private(plane_ext.geometry);
			if (stack_2d) geo = stack_2d->path;
			break;
		default:
			return;
		}
		switch (gf_node_get_tag(plane_ext.spine) ) {
		case TAG_MPEG4_Circle:
		case TAG_MPEG4_Ellipse:
		case TAG_MPEG4_Rectangle:
		case TAG_MPEG4_Curve2D:
		case TAG_MPEG4_XCurve2D:
		case TAG_MPEG4_IndexedFaceSet2D:
		case TAG_MPEG4_IndexedLineSet2D:
			stack_2d = (Drawable*)gf_node_get_private(plane_ext.spine);
			if (stack_2d) spine = stack_2d->path;
			break;
		default:
			return;
		}
		if (!geo || !spine) return;

		mesh_reset(stack->mesh);
		gf_path_flatten(spine);
		gf_path_get_bounds(spine, &bounds);
		gf_path_flatten(geo);
		gf_path_get_bounds(geo, &bounds);

		cur = 0;
		for (i=0; i<spine->n_contours; i++) {
			nb_pts = 1 + spine->contours[i] - cur;
			spine_vec.vals = NULL;
			gf_sg_vrml_mf_alloc(&spine_vec, GF_SG_VRML_MFVEC3F, nb_pts);
			spine_len = 0;
			for (j=cur; j<nb_pts; j++) {
				spine_vec.vals[j].x = spine->points[j].x;
				spine_vec.vals[j].y = spine->points[j].y;
				spine_vec.vals[j].z = 0;
				if (j) {
					gf_vec_diff(d, spine_vec.vals[j], spine_vec.vals[j-1]);
					spine_len += gf_vec_len(d);
				}
			}
			cur += nb_pts;
			if (!plane_ext.orientation->count && !plane_ext.scale->count) {
				mesh_extrude_path_ext(stack->mesh, geo, &spine_vec, plane_ext.creaseAngle,
				                      bounds.x, bounds.y-bounds.height, bounds.width, bounds.height,
				                      plane_ext.beginCap, plane_ext.endCap, NULL, NULL, plane_ext.txAlongSpine);
			}
			/*interpolate orientation and scale along subpath line*/
			else {
				MFRotation ori;
				MFVec2f scale;
				Fixed cur_len, frac;

				ori.vals = NULL;
				gf_sg_vrml_mf_alloc(&ori, GF_SG_VRML_MFROTATION, nb_pts);
				scale.vals = NULL;
				gf_sg_vrml_mf_alloc(&scale, GF_SG_VRML_MFVEC2F, nb_pts);
				cur_len = 0;
				if (!plane_ext.orientation->count) ori.vals[0].y = FIX_ONE;
				if (!plane_ext.scale->count) scale.vals[0].x = scale.vals[0].y = FIX_ONE;
				for (j=0; j<nb_pts; j++) {
					if (j) {
						gf_vec_diff(d, spine_vec.vals[j], spine_vec.vals[j-1]);
						cur_len += gf_vec_len(d);
						ori.vals[j] = ori.vals[j-1];
						scale.vals[j] = scale.vals[j-1];
					}

					if (plane_ext.orientation->count && (plane_ext.orientation->count == plane_ext.orientationKeys->count)) {

						frac = gf_divfix(cur_len , spine_len);
						if (frac < plane_ext.orientationKeys->vals[0]) ori.vals[j] = plane_ext.orientation->vals[0];
						else if (frac >= plane_ext.orientationKeys->vals[plane_ext.orientationKeys->count-1]) ori.vals[j] = plane_ext.orientation->vals[plane_ext.orientationKeys->count-1];
						else {
							for (k=1; k<plane_ext.orientationKeys->count; k++) {
								Fixed kDiff = plane_ext.orientationKeys->vals[k] - plane_ext.orientationKeys->vals[k-1];
								if (!kDiff) continue;
								if (frac < plane_ext.orientationKeys->vals[k-1]) continue;
								if (frac > plane_ext.orientationKeys->vals[k]) continue;
								frac = gf_divfix(frac - plane_ext.orientationKeys->vals[k-1], kDiff);
								break;
							}
							ori.vals[j] = gf_sg_sfrotation_interpolate(plane_ext.orientation->vals[k-1], plane_ext.orientation->vals[k], frac);
						}
					}

					if (plane_ext.scale->count == plane_ext.scaleKeys->count) {
						frac = gf_divfix(cur_len , spine_len);
						if (frac <= plane_ext.scaleKeys->vals[0]) scale.vals[j] = plane_ext.scale->vals[0];
						else if (frac >= plane_ext.scaleKeys->vals[plane_ext.scaleKeys->count-1]) scale.vals[j] = plane_ext.scale->vals[plane_ext.scale->count-1];
						else {
							for (k=1; k<plane_ext.scaleKeys->count; k++) {
								Fixed kDiff = plane_ext.scaleKeys->vals[k] - plane_ext.scaleKeys->vals[k-1];
								if (!kDiff) continue;
								if (frac < plane_ext.scaleKeys->vals[k-1]) continue;
								if (frac > plane_ext.scaleKeys->vals[k]) continue;
								frac = gf_divfix(frac - plane_ext.scaleKeys->vals[k-1], kDiff);
								break;
							}
							scale.vals[j].x = gf_mulfix(plane_ext.scale->vals[k].x - plane_ext.scale->vals[k-1].x, frac) + plane_ext.scale->vals[k-1].x;
							scale.vals[j].y = gf_mulfix(plane_ext.scale->vals[k].y - plane_ext.scale->vals[k-1].y, frac) + plane_ext.scale->vals[k-1].y;
						}
					}
				}

				mesh_extrude_path_ext(stack->mesh, geo, &spine_vec, plane_ext.creaseAngle,
				                      bounds.x, bounds.y-bounds.height, bounds.width, bounds.height,
				                      plane_ext.beginCap, plane_ext.endCap, &ori, &scale, plane_ext.txAlongSpine);

				gf_sg_vrml_mf_reset(&ori, GF_SG_VRML_MFROTATION);
				gf_sg_vrml_mf_reset(&scale, GF_SG_VRML_MFVEC2F);
			}

			gf_sg_vrml_mf_reset(&spine_vec, GF_SG_VRML_MFVEC3F);
		}
		mesh_update_bounds(stack->mesh);
		gf_mesh_build_aabbtree(stack->mesh);
	}

	if (tr_state->traversing_mode==TRAVERSE_DRAW_3D) {
		visual_3d_draw(tr_state, stack->mesh);
	} else if (tr_state->traversing_mode==TRAVERSE_GET_BOUNDS) {
		tr_state->bbox = stack->mesh->bounds;
	}
}

void compositor_init_planar_extrusion(GF_Compositor *compositor, GF_Node *node)
{
	drawable_3d_new(node);
	gf_node_set_callback_function(node, TraversePlanarExtrusion);
}

/*PlaneClipper hardcoded proto*/
typedef struct
{
	BASE_NODE
	CHILDREN

	GF_Plane plane;
} PlaneClipper;

typedef struct
{
	GROUPING_NODE_STACK_3D
	PlaneClipper pc;
} PlaneClipperStack;

static Bool PlaneClipper_GetNode(GF_Node *node, PlaneClipper *pc)
{
	GF_FieldInfo field;
	memset(pc, 0, sizeof(PlaneClipper));
	pc->sgprivate = node->sgprivate;

	if (gf_node_get_field(node, 0, &field) != GF_OK) return 0;
	if (field.fieldType != GF_SG_VRML_SFVEC3F) return 0;
	pc->plane.normal = * (SFVec3f *) field.far_ptr;
	if (gf_node_get_field(node, 1, &field) != GF_OK) return 0;
	if (field.fieldType != GF_SG_VRML_SFFLOAT) return 0;
	pc->plane.d = * (SFFloat *) field.far_ptr;
	if (gf_node_get_field(node, 2, &field) != GF_OK) return 0;
	if (field.fieldType != GF_SG_VRML_MFNODE) return 0;
	pc->children = *(GF_ChildNodeItem **) field.far_ptr;
	return 1;
}


static void TraversePlaneClipper(GF_Node *node, void *rs, Bool is_destroy)
{
	PlaneClipperStack *stack = (PlaneClipperStack *)gf_node_get_private(node);
	GF_TraverseState *tr_state = (GF_TraverseState *) rs;

	if (is_destroy) {
		group_3d_delete(node);
		return;
	}

	if (gf_node_dirty_get(node)) {
		PlaneClipper_GetNode(node, &stack->pc);
	}

	if (tr_state->num_clip_planes==MAX_USER_CLIP_PLANES) {
		group_3d_traverse((GF_Node*)&stack->pc, (GroupingNode*)stack, tr_state);
		return;
	}

	if (tr_state->traversing_mode == TRAVERSE_SORT) {
		GF_Matrix mx;
		gf_mx_copy(mx, tr_state->model_matrix);
		visual_3d_set_clip_plane(tr_state->visual, stack->pc.plane, &mx, 0);
		tr_state->num_clip_planes++;

		group_3d_traverse((GF_Node*)&stack->pc, (GroupingNode*)stack, tr_state);
		visual_3d_reset_clip_plane(tr_state->visual);
		tr_state->num_clip_planes--;
	} else {
		tr_state->clip_planes[tr_state->num_clip_planes] = stack->pc.plane;
		gf_mx_apply_plane(&tr_state->model_matrix, &tr_state->clip_planes[tr_state->num_clip_planes]);
		tr_state->num_clip_planes++;

		group_3d_traverse((GF_Node*)&stack->pc, (GroupingNode*)stack, tr_state);

		tr_state->num_clip_planes--;
	}

}

void compositor_init_plane_clipper(GF_Compositor *compositor, GF_Node *node)
{
	PlaneClipper pc;
	if (PlaneClipper_GetNode(node, &pc)) {
		PlaneClipperStack *stack;
		GF_SAFEALLOC(stack, PlaneClipperStack);
		//SetupGroupingNode(stack, compositor->compositor, node, & pc.children);
		gf_node_set_private(node, stack);
		gf_node_set_callback_function(node, TraversePlaneClipper);
		/*we're a grouping node, force bounds rebuild as soon as loaded*/
		gf_node_dirty_set(node, GF_SG_CHILD_DIRTY, 0);

		stack->pc = pc;
		gf_node_proto_set_grouping(node);
	}
}

#endif


/*OffscreenGroup hardcoded proto*/
typedef struct
{
	BASE_NODE
	CHILDREN

	s32 offscreen;
	Fixed opacity;
} OffscreenGroup;

typedef struct
{
	GROUPING_MPEG4_STACK_2D

#ifndef GF_SR_USE_VIDEO_CACHE
	struct _group_cache *cache;
#endif

	OffscreenGroup og;
	Bool detached;
} OffscreenGroupStack;

static Bool OffscreenGroup_GetNode(GF_Node *node, OffscreenGroup *og)
{
	GF_FieldInfo field;
	memset(og, 0, sizeof(OffscreenGroup));
	og->sgprivate = node->sgprivate;

	if (gf_node_get_field(node, 0, &field) != GF_OK) return 0;
	if (field.fieldType != GF_SG_VRML_MFNODE) return 0;
	og->children = *(GF_ChildNodeItem **) field.far_ptr;

	if (gf_node_get_field(node, 1, &field) != GF_OK) return 0;
	if (field.fieldType != GF_SG_VRML_SFINT32) return 0;
	og->offscreen = * (SFInt32 *) field.far_ptr;

	if (gf_node_get_field(node, 2, &field) != GF_OK) return 0;
	if (field.fieldType != GF_SG_VRML_SFFLOAT) return 0;
	og->opacity = * (SFFloat *) field.far_ptr;
	return 1;
}


static void TraverseOffscreenGroup(GF_Node *node, void *rs, Bool is_destroy)
{
	OffscreenGroupStack *stack = (OffscreenGroupStack *)gf_node_get_private(node);
	GF_TraverseState *tr_state = (GF_TraverseState *) rs;

	if (is_destroy) {
		if (stack->cache) group_cache_del(stack->cache);
		gf_free(stack);
		return;
	}

	if (tr_state->traversing_mode==TRAVERSE_SORT) {
		if (!stack->detached && (gf_node_dirty_get(node) & GF_SG_NODE_DIRTY)) {
			OffscreenGroup_GetNode(node, &stack->og);

			if (stack->og.offscreen) {
				stack->flags |= GROUP_IS_CACHED | GROUP_PERMANENT_CACHE;
				if (!stack->cache) {
					stack->cache = group_cache_new(tr_state->visual->compositor, (GF_Node*)&stack->og);
				}
				stack->cache->opacity = stack->og.opacity;
				stack->cache->drawable->flags |= DRAWABLE_HAS_CHANGED;
			} else {
				if (stack->cache) group_cache_del(stack->cache);
				stack->cache = NULL;
				stack->flags &= ~(GROUP_IS_CACHED|GROUP_PERMANENT_CACHE);
			}
			gf_node_dirty_clear(node, GF_SG_NODE_DIRTY);
			/*flag is not set for PROTO*/
			gf_node_dirty_set(node, GF_SG_CHILD_DIRTY, 0);
		}
		if (stack->cache) {
			if (stack->detached)
				gf_node_dirty_clear(node, GF_SG_CHILD_DIRTY);

			tr_state->subscene_not_over = 0;
			group_cache_traverse((GF_Node *)&stack->og, stack->cache, tr_state, stack->cache->force_recompute, 1, stack->detached ? 1 : 0);

			if (gf_node_dirty_get(node)) {
				gf_node_dirty_clear(node, GF_SG_CHILD_DIRTY);
			} else if ((stack->og.offscreen==2) && !stack->detached && !tr_state->subscene_not_over && stack->cache->txh.width && stack->cache->txh.height) {
				GF_FieldInfo field;
				if (gf_node_get_field(node, 0, &field) == GF_OK) {
					gf_node_unregister_children(node, *(GF_ChildNodeItem **) field.far_ptr);
					*(GF_ChildNodeItem **) field.far_ptr = NULL;
					stack->detached = 1;
				}
				if (gf_node_get_field(node, 3, &field) == GF_OK) {
					*(SFBool *) field.far_ptr = 1;
					//gf_node_event_out(node, 3);
				}
			}
		} else {
			group_2d_traverse((GF_Node *)&stack->og, (GroupingNode2D*)stack, tr_state);
		}
	}
	/*draw mode*/
	else if (stack->cache && (tr_state->traversing_mode == TRAVERSE_DRAW_2D)) {
		/*draw it*/
		group_cache_draw(stack->cache, tr_state);
		gf_node_dirty_clear(node, GF_SG_CHILD_DIRTY);
	} else if (!stack->detached) {
		group_2d_traverse((GF_Node *)&stack->og, (GroupingNode2D*)stack, tr_state);
	} else {
		if (tr_state->traversing_mode == TRAVERSE_GET_BOUNDS) {
			tr_state->bounds = stack->bounds;
		}
		else if (tr_state->traversing_mode == TRAVERSE_PICK) {
			vrml_drawable_pick(stack->cache->drawable, tr_state);
		}
	}
}

void compositor_init_offscreen_group(GF_Compositor *compositor, GF_Node *node)
{
	OffscreenGroup og;
	if (OffscreenGroup_GetNode(node, &og)) {
		OffscreenGroupStack *stack;
		GF_SAFEALLOC(stack, OffscreenGroupStack);
		gf_node_set_private(node, stack);
		gf_node_set_callback_function(node, TraverseOffscreenGroup);
		stack->og = og;
		if (og.offscreen) stack->flags |= GROUP_IS_CACHED;
		gf_node_proto_set_grouping(node);
	}
}


/*DepthGroup hardcoded proto*/
typedef struct
{
	BASE_NODE
	CHILDREN

	Fixed depth_gain, depth_offset;

} DepthGroup;

typedef struct
{
	GROUPING_MPEG4_STACK_2D
	DepthGroup dg;
} DepthGroupStack;

static Bool DepthGroup_GetNode(GF_Node *node, DepthGroup *dg)
{
	GF_FieldInfo field;
	memset(dg, 0, sizeof(DepthGroup));
	dg->sgprivate = node->sgprivate;

	if (gf_node_get_field(node, 0, &field) != GF_OK) return 0;
	if (field.fieldType != GF_SG_VRML_MFNODE) return 0;
	dg->children = *(GF_ChildNodeItem **) field.far_ptr;

	if (gf_node_get_field(node, 1, &field) != GF_OK) return 0;
	if (field.fieldType != GF_SG_VRML_SFINT32) return 0;

	if (gf_node_get_field(node, 2, &field) != GF_OK) return 0;
	if (field.fieldType != GF_SG_VRML_SFFLOAT) return 0;
	dg->depth_gain = * (SFFloat *) field.far_ptr;

	if (gf_node_get_field(node, 3, &field) != GF_OK) return 0;
	if (field.fieldType != GF_SG_VRML_SFFLOAT) return 0;
	dg->depth_offset = * (SFFloat *) field.far_ptr;

	return 1;
}


static void TraverseDepthGroup(GF_Node *node, void *rs, Bool is_destroy)
{
#ifdef GF_SR_USE_DEPTH
	Fixed depth_gain, depth_offset;
#endif

	DepthGroupStack *stack = (DepthGroupStack *)gf_node_get_private(node);
	GF_TraverseState *tr_state = (GF_TraverseState *) rs;

	if (is_destroy) {
		gf_free(stack);
		return;
	}

	if (tr_state->traversing_mode==TRAVERSE_SORT) {
		if (gf_node_dirty_get(node) & GF_SG_NODE_DIRTY) {

			gf_node_dirty_clear(node, GF_SG_NODE_DIRTY);
			/*flag is not set for PROTO*/
			gf_node_dirty_set(node, GF_SG_CHILD_DIRTY, 0);
		}
	}
	DepthGroup_GetNode(node, &stack->dg);


#ifdef GF_SR_USE_DEPTH
	depth_gain = tr_state->depth_gain;
	depth_offset = tr_state->depth_offset;

	// new offset is multiplied by parent gain and added to parent offset
	tr_state->depth_offset = gf_mulfix(stack->dg.depth_offset, tr_state->depth_gain) + tr_state->depth_offset;

	// gain is multiplied by parent gain
	tr_state->depth_gain = gf_mulfix(tr_state->depth_gain, stack->dg.depth_gain);
#endif

#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d) {
		GF_Matrix mx_bckup, mx;

		gf_mx_copy(mx_bckup, tr_state->model_matrix);
		gf_mx_init(mx);
		mx.m[14] = gf_mulfix(stack->dg.depth_offset, tr_state->visual->compositor->depth_gl_scale);
		gf_mx_add_matrix(&tr_state->model_matrix, &mx);
		group_2d_traverse((GF_Node *)&stack->dg, (GroupingNode2D*)stack, tr_state);
		gf_mx_copy(tr_state->model_matrix, mx_bckup);

	} else
#endif
	{

		group_2d_traverse((GF_Node *)&stack->dg, (GroupingNode2D*)stack, tr_state);
	}

#ifdef GF_SR_USE_DEPTH
	tr_state->depth_gain = depth_gain;
	tr_state->depth_offset = depth_offset;
#endif
}

void compositor_init_depth_group(GF_Compositor *compositor, GF_Node *node)
{
	DepthGroup dg;
	if (DepthGroup_GetNode(node, &dg)) {
		DepthGroupStack *stack;
		GF_SAFEALLOC(stack, DepthGroupStack);
		gf_node_set_private(node, stack);
		gf_node_set_callback_function(node, TraverseDepthGroup);
		stack->dg = dg;
		gf_node_proto_set_grouping(node);
	} else GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor2D] Unable to initialize depth group  \n"));

}

#ifdef GF_SR_USE_DEPTH
static void TraverseDepthViewPoint(GF_Node *node, void *rs, Bool is_destroy)
{
	if (!is_destroy && gf_node_dirty_get(node)) {
		GF_TraverseState *tr_state = (GF_TraverseState *) rs;
		GF_FieldInfo field;
		gf_node_dirty_clear(node, 0);

		tr_state->visual->depth_vp_position = 0;
		tr_state->visual->depth_vp_range = 0;
#ifndef GPAC_DISABLE_3D
		if (!tr_state->camera) return;
		tr_state->camera->flags |= CAM_IS_DIRTY;
#endif

		if (gf_node_get_field(node, 0, &field) != GF_OK) return;
		if (field.fieldType != GF_SG_VRML_SFBOOL) return;

		if ( *(SFBool *) field.far_ptr) {
			if (gf_node_get_field(node, 1, &field) != GF_OK) return;
			if (field.fieldType != GF_SG_VRML_SFFLOAT) return;
			tr_state->visual->depth_vp_position = *(SFFloat *) field.far_ptr;
			if (gf_node_get_field(node, 2, &field) != GF_OK) return;
			if (field.fieldType != GF_SG_VRML_SFFLOAT) return;
			tr_state->visual->depth_vp_range = *(SFFloat *) field.far_ptr;
		}
#ifndef GPAC_DISABLE_3D
		if (tr_state->layer3d) gf_node_dirty_set(tr_state->layer3d, GF_SG_NODE_DIRTY, 0);
#endif
		gf_sc_invalidate(tr_state->visual->compositor, NULL);
	}
}
#endif

static void compositor_init_depth_viewpoint(GF_Compositor *compositor, GF_Node *node)
{
#ifdef GF_SR_USE_DEPTH
	gf_node_set_callback_function(node, TraverseDepthViewPoint);
#endif
}

/*IndexedCurve2D hardcoded proto*/

typedef struct
{
	BASE_NODE

	GF_Node *point;
	Fixed fineness;
	MFInt32 type;
	MFInt32 index;
} IndexedCurve2D;

static Bool IndexedCurve2D_GetNode(GF_Node *node, IndexedCurve2D *ic2d)
{
	GF_FieldInfo field;
	memset(ic2d, 0, sizeof(IndexedCurve2D));

	ic2d->sgprivate = node->sgprivate;

	if (gf_node_get_field(node, 0, &field) != GF_OK) return 0;
	if (field.fieldType != GF_SG_VRML_SFNODE) return 0;
	ic2d->point = * (GF_Node **) field.far_ptr;

	if (gf_node_get_field(node, 1, &field) != GF_OK) return 0;
	if (field.fieldType != GF_SG_VRML_SFFLOAT) return 0;
	ic2d->fineness = *(SFFloat *) field.far_ptr;

	if (gf_node_get_field(node, 2, &field) != GF_OK) return 0;
	if (field.fieldType != GF_SG_VRML_MFINT32) return 0;
	ic2d->type = *(MFInt32 *) field.far_ptr;

	if (gf_node_get_field(node, 3, &field) != GF_OK) return 0;
	if (field.fieldType != GF_SG_VRML_MFINT32) return 0;
	ic2d->index = *(MFInt32 *) field.far_ptr;

	return 1;
}

void curve2d_check_changes(GF_Node *node, Drawable *stack, GF_TraverseState *tr_state, MFInt32 *idx);

static void TraverseIndexedCurve2D(GF_Node *node, void *rs, Bool is_destroy)
{
	DrawableContext *ctx;
	IndexedCurve2D ic2d;
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;
	Drawable *stack = (Drawable *)gf_node_get_private(node);

	if (is_destroy) {
		drawable_node_del(node);
		return;
	}

	if (gf_node_dirty_get(node)) {
		if (!IndexedCurve2D_GetNode(node, &ic2d)) return;
		curve2d_check_changes((GF_Node*) &ic2d, stack, tr_state, &ic2d.index);
	}

	switch (tr_state->traversing_mode) {
#ifndef GPAC_DISABLE_3D
	case TRAVERSE_DRAW_3D:
		if (!stack->mesh) {
			stack->mesh = new_mesh();
			mesh_from_path(stack->mesh, stack->path);
		}
		visual_3d_draw_2d(stack, tr_state);
		return;
#endif
	case TRAVERSE_PICK:
		vrml_drawable_pick(stack, tr_state);
		return;
	case TRAVERSE_GET_BOUNDS:
		gf_path_get_bounds(stack->path, &tr_state->bounds);
		return;
	case TRAVERSE_SORT:
#ifndef GPAC_DISABLE_3D
		if (tr_state->visual->type_3d) return;
#endif
		ctx = drawable_init_context_mpeg4(stack, tr_state);
		if (!ctx) return;
		drawable_finalize_sort(ctx, tr_state, NULL);
		return;
	}
}

static void compositor_init_idx_curve2d(GF_Compositor *compositor, GF_Node *node)
{
	drawable_stack_new(compositor, node);
	gf_node_set_callback_function(node, TraverseIndexedCurve2D);
}





/*TransformRef hardcoded proto*/
typedef struct
{
	BASE_NODE
	CHILDREN
} Untransform;

typedef struct
{
	GROUPING_MPEG4_STACK_2D
	Untransform untr;
} UntransformStack;

static Bool Untransform_GetNode(GF_Node *node, Untransform *tr)
{
	GF_FieldInfo field;
	memset(tr, 0, sizeof(Untransform));
	tr->sgprivate = node->sgprivate;

	if (gf_node_get_field(node, 0, &field) != GF_OK) return 0;
	if (field.fieldType != GF_SG_VRML_MFNODE) return 0;
	tr->children = *(GF_ChildNodeItem **) field.far_ptr;

	return 1;
}


static void TraverseUntransform(GF_Node *node, void *rs, Bool is_destroy)
{
	UntransformStack *stack = (UntransformStack *)gf_node_get_private(node);
	GF_TraverseState *tr_state = (GF_TraverseState *) rs;

	if (is_destroy) {
		gf_free(stack);
		return;
	}

	if (tr_state->traversing_mode==TRAVERSE_SORT) {
		if (gf_node_dirty_get(node)) {
			Untransform_GetNode(node, &stack->untr); /*lets place it below*/
			gf_node_dirty_clear(node, GF_SG_NODE_DIRTY);
		}
	}

#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d) {
		GF_Matrix mx_model;
		GF_Camera backup_cam;

		if (!tr_state->camera) return;

		gf_mx_copy(mx_model, tr_state->model_matrix);
		gf_mx_init(tr_state->model_matrix);

		memcpy(&backup_cam, tr_state->camera, sizeof(GF_Camera));


		camera_invalidate(tr_state->camera);
		tr_state->camera->is_3D=0;
		tr_state->camera->flags |= CAM_NO_LOOKAT;
		tr_state->camera->end_zoom = FIX_ONE;
		camera_update(tr_state->camera, NULL, 1);


		if (tr_state->traversing_mode == TRAVERSE_SORT) {
			visual_3d_set_viewport(tr_state->visual, tr_state->camera->vp);
			visual_3d_projection_matrix_modified(tr_state->visual);

			gf_node_traverse_children((GF_Node *)&stack->untr, tr_state);

			gf_mx_copy(tr_state->model_matrix, mx_model);
			memcpy(tr_state->camera, &backup_cam, sizeof(GF_Camera));

			visual_3d_projection_matrix_modified(tr_state->visual);

			visual_3d_set_viewport(tr_state->visual, tr_state->camera->vp);
		} else if (tr_state->traversing_mode == TRAVERSE_PICK) {
			Fixed prev_dist = tr_state->visual->compositor->hit_square_dist;
			GF_Ray r = tr_state->ray;
			tr_state->ray.orig.x = INT2FIX(tr_state->pick_x);
			tr_state->ray.orig.y = INT2FIX(tr_state->pick_y);
			tr_state->ray.orig.z = 0;
			tr_state->ray.dir.x = 0;
			tr_state->ray.dir.y = 0;
			tr_state->ray.dir.z = -FIX_ONE;
			tr_state->visual->compositor->hit_square_dist=0;

			gf_node_traverse_children((GF_Node *)&stack->untr, tr_state);

			gf_mx_copy(tr_state->model_matrix, mx_model);
			memcpy(tr_state->camera, &backup_cam, sizeof(GF_Camera));
			tr_state->ray = r;

			/*nothing picked, restore previous pick*/
			if (!tr_state->visual->compositor->hit_square_dist)
				tr_state->visual->compositor->hit_square_dist = prev_dist;

		} else {
			gf_node_traverse_children((GF_Node *)&stack->untr, tr_state);

			gf_mx_copy(tr_state->model_matrix, mx_model);
			memcpy(tr_state->camera, &backup_cam, sizeof(GF_Camera));
		}

	} else
#endif
	{
		GF_Matrix2D mx2d_backup;
		gf_mx2d_copy(mx2d_backup, tr_state->transform);
		gf_mx2d_init(tr_state->transform);

		group_2d_traverse((GF_Node *)&stack->untr, (GroupingNode2D *)stack, tr_state);

		gf_mx2d_copy(tr_state->transform, mx2d_backup);


	}
}

void compositor_init_untransform(GF_Compositor *compositor, GF_Node *node)
{
	Untransform tr;
	if (Untransform_GetNode(node, &tr)) {
		UntransformStack *stack;
		GF_SAFEALLOC(stack, UntransformStack);
		gf_node_set_private(node, stack);
		gf_node_set_callback_function(node, TraverseUntransform);
		stack->untr = tr;
		gf_node_proto_set_grouping(node);
	}
}

/*hardcoded proto loading - this is mainly used for module development and testing...*/
void compositor_init_hardcoded_proto(GF_Compositor *compositor, GF_Node *node)
{
	MFURL *proto_url;
	GF_Proto *proto;
	u32 i, j;
	GF_HardcodedProto *ifce;

	proto = gf_node_get_proto(node);
	if (!proto) return;
	proto_url = gf_sg_proto_get_extern_url(proto);

	for (i=0; i<proto_url->count; i++) {
		const char *url = proto_url->vals[0].url;
#ifndef GPAC_DISABLE_3D
		if (!strcmp(url, "urn:inet:gpac:builtin:PathExtrusion")) {
			compositor_init_path_extrusion(compositor, node);
			return;
		}
		if (!strcmp(url, "urn:inet:gpac:builtin:PlanarExtrusion")) {
			compositor_init_planar_extrusion(compositor, node);
			return;
		}
		if (!strcmp(url, "urn:inet:gpac:builtin:PlaneClipper")) {
			compositor_init_plane_clipper(compositor, node);
			return;
		}
#endif
		if (!strcmp(url, "urn:inet:gpac:builtin:TextureText")) {
			compositor_init_texture_text(compositor, node);
			return;
		}
		if (!strcmp(url, "urn:inet:gpac:builtin:OffscreenGroup")) {
			compositor_init_offscreen_group(compositor, node);
			return;
		}
		if (!strcmp(url, "urn:inet:gpac:builtin:DepthGroup")) {
			compositor_init_depth_group(compositor, node);
			return;
		}
		if (!strcmp(url, "urn:inet:gpac:builtin:DepthViewPoint")) {
			compositor_init_depth_viewpoint(compositor, node);
			return;
		}
		if (!strcmp(url, "urn:inet:gpac:builtin:IndexedCurve2D")) {
			compositor_init_idx_curve2d(compositor, node);
			return;
		}
		if (!strcmp(url, "urn:inet:gpac:builtin:Untransform")) {
			compositor_init_untransform(compositor, node);
			return;
		}
		if (!strcmp(url, "urn:inet:gpac:builtin:FlashShape")) {
			compositor_init_hc_flashshape(compositor, node);
			return;
		}

		/*check proto modules*/
		if (compositor->proto_modules) {
			j = 0;
			while ( (ifce = (GF_HardcodedProto *)gf_list_enum(compositor->proto_modules, &j) )) {
				if ( ifce->can_load_proto(url) && ifce->init(ifce, compositor, node, url) ) {
					return;
				}
			}
		}
	}

}

Bool gf_sc_uri_is_hardcoded_proto(GF_Compositor *compositor, const char *uri)
{
	/*check proto modules*/
	if (compositor && compositor->proto_modules ) {
		u32 j = 0;
		GF_HardcodedProto *ifce;
		while ( (ifce = (GF_HardcodedProto *)gf_list_enum(compositor->proto_modules, &j) )) {
			if ( ifce->can_load_proto(uri)) {
				return 1;
			}
		}
	}
	return 0;
}

#endif /*GPAC_DISABLE_VRML*/
