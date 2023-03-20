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

#include "nodes_stacks.h"
#include "visual_manager.h"

#ifndef GPAC_DISABLE_COMPOSITOR

#include "offscreen_cache.h"
#include "mpeg4_grouping.h"

#include <gpac/modules/hardcoded_proto.h>
#include "texturing.h"


#define CHECK_FIELD(__name, __index, __type) \
	if (gf_node_get_field(node, __index, &field) != GF_OK) {\
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[HardcodedProtos] Cannot get field index %d for proto %s\n", __index, __name));\
		return GF_FALSE; \
	}\
	if (field.fieldType != __type) {\
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[HardcodedProtos] %s field idx %d (%s) is not of type %s\n", __name, field.fieldIndex, field.name, gf_sg_vrml_get_field_type_name(__type) ));\
		return GF_FALSE;\
	}


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
	
	CHECK_FIELD("PathExtrusion", 0, GF_SG_VRML_SFNODE);
	path_ext->geometry = * (GF_Node **) field.far_ptr;
	
	CHECK_FIELD("PathExtrusion", 1, GF_SG_VRML_MFVEC3F);
	path_ext->spine = (MFVec3f *) field.far_ptr;

	CHECK_FIELD("PathExtrusion", 2, GF_SG_VRML_SFBOOL);
	path_ext->beginCap = *(SFBool *) field.far_ptr;
	
	CHECK_FIELD("PathExtrusion", 3, GF_SG_VRML_SFBOOL);
	path_ext->endCap = *(SFBool *) field.far_ptr;
	
	CHECK_FIELD("PathExtrusion", 4, GF_SG_VRML_SFFLOAT);
	path_ext->creaseAngle = *(SFFloat *) field.far_ptr;
	
	CHECK_FIELD("PathExtrusion", 5, GF_SG_VRML_MFROTATION);
	path_ext->orientation = (MFRotation *) field.far_ptr;
	
	CHECK_FIELD("PathExtrusion", 6, GF_SG_VRML_MFVEC2F);
	path_ext->scale = (MFVec2f *) field.far_ptr;
	
	CHECK_FIELD("PathExtrusion", 7, GF_SG_VRML_SFBOOL);
	path_ext->txAlongSpine = *(SFBool *) field.far_ptr;
	return GF_TRUE;
}

static void TraversePathExtrusion(GF_Node *node, void *rs, Bool is_destroy)
{
	PathExtrusion path_ext;
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;
	Drawable3D *stack = (Drawable3D *)gf_node_get_private(node);

	if (is_destroy) {
		drawable_3d_del(node);
		return;
	}
	if (!PathExtrusion_GetNode(node, &path_ext)) return;
	if (!path_ext.geometry) return;


	if (gf_node_dirty_get(node)) {
		Drawable *stack_2d;
		u32 mode = tr_state->traversing_mode;
		tr_state->traversing_mode = TRAVERSE_GET_BOUNDS;
		gf_node_traverse(path_ext.geometry, tr_state);
		tr_state->traversing_mode = mode;

		gf_node_dirty_clear(node, 0);

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

	CHECK_FIELD("PlanarExtrusion", 0, GF_SG_VRML_SFNODE);
	path_ext->geometry = * (GF_Node **) field.far_ptr;
	
	CHECK_FIELD("PlanarExtrusion", 1, GF_SG_VRML_SFNODE);
	path_ext->spine = * (GF_Node **) field.far_ptr;
	
	CHECK_FIELD("PlanarExtrusion", 2, GF_SG_VRML_SFBOOL);
	path_ext->beginCap = *(SFBool *) field.far_ptr;
	
	CHECK_FIELD("PlanarExtrusion", 3, GF_SG_VRML_SFBOOL);
	path_ext->endCap = *(SFBool *) field.far_ptr;
	
	CHECK_FIELD("PlanarExtrusion", 4, GF_SG_VRML_SFFLOAT);
	path_ext->creaseAngle = *(SFFloat *) field.far_ptr;
	
	CHECK_FIELD("PlanarExtrusion", 5, GF_SG_VRML_MFFLOAT);
	path_ext->orientationKeys = (MFFloat *) field.far_ptr;
	
	CHECK_FIELD("PlanarExtrusion", 6, GF_SG_VRML_MFROTATION);
	path_ext->orientation = (MFRotation *) field.far_ptr;
	
	CHECK_FIELD("PlanarExtrusion", 7, GF_SG_VRML_MFFLOAT);
	path_ext->scaleKeys = (MFFloat *) field.far_ptr;
	
	CHECK_FIELD("PlanarExtrusion", 8, GF_SG_VRML_MFVEC2F);
	path_ext->scale = (MFVec2f *) field.far_ptr;
	
	CHECK_FIELD("PlanarExtrusion", 9, GF_SG_VRML_SFBOOL);
	path_ext->txAlongSpine = *(SFBool *) field.far_ptr;

	return GF_TRUE;
}

static void TraversePlanarExtrusion(GF_Node *node, void *rs, Bool is_destroy)
{
	PlanarExtrusion plane_ext;
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;
	Drawable3D *stack = (Drawable3D *)gf_node_get_private(node);

	if (is_destroy) {
		drawable_3d_del(node);
		return;
	}

	if (!PlanarExtrusion_GetNode(node, &plane_ext)) return;
	if (!plane_ext.geometry || !plane_ext.spine) return;


	if (gf_node_dirty_get(node)) {
		Drawable *stack_2d;
		u32 i, j, k;
		MFVec3f spine_vec;
		SFVec3f d;
		Fixed spine_len;
		GF_Rect bounds;
		u32 cur, nb_pts;
		u32 mode = tr_state->traversing_mode;
		GF_Path *geo, *spine;
		geo = spine = NULL;

		tr_state->traversing_mode = TRAVERSE_GET_BOUNDS;
		gf_node_traverse(plane_ext.geometry, tr_state);
		gf_node_traverse(plane_ext.spine, tr_state);
		tr_state->traversing_mode = mode;
		gf_node_dirty_clear(node, 0);

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

	CHECK_FIELD("PlaneClipper", 0, GF_SG_VRML_SFVEC3F);
	pc->plane.normal = * (SFVec3f *) field.far_ptr;

	CHECK_FIELD("PlaneClipper", 1, GF_SG_VRML_SFFLOAT);
	pc->plane.d = * (SFFloat *) field.far_ptr;
	
	CHECK_FIELD("PlaneClipper", 2, GF_SG_VRML_MFNODE);
	pc->children = *(GF_ChildNodeItem **) field.far_ptr;
	return GF_TRUE;
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
		gf_node_dirty_clear(node, GF_SG_NODE_DIRTY);
	}

	if (tr_state->num_clip_planes==MAX_USER_CLIP_PLANES) {
		group_3d_traverse((GF_Node*)&stack->pc, (GroupingNode*)stack, tr_state);
		return;
	}

	if (tr_state->traversing_mode == TRAVERSE_SORT) {
		GF_Matrix mx;
		gf_mx_copy(mx, tr_state->model_matrix);
		visual_3d_set_clip_plane(tr_state->visual, stack->pc.plane, &mx, GF_FALSE);
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
		if (!stack) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate plane clipper stack\n"));
			return;
		}
		//SetupGroupingNode(stack, compositor->compositor, node, & pc.children);
		gf_node_set_private(node, stack);
		gf_node_set_callback_function(node, TraversePlaneClipper);
		/*we're a grouping node, force bounds rebuild as soon as loaded*/
		gf_node_dirty_set(node, GF_SG_CHILD_DIRTY, GF_FALSE);

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

	CHECK_FIELD("OffscreenGroup", 0, GF_SG_VRML_MFNODE);
	og->children = *(GF_ChildNodeItem **) field.far_ptr;

	CHECK_FIELD("OffscreenGroup", 1, GF_SG_VRML_SFINT32);
	og->offscreen = * (SFInt32 *) field.far_ptr;

	CHECK_FIELD("OffscreenGroup", 2, GF_SG_VRML_SFFLOAT);
	og->opacity = * (SFFloat *) field.far_ptr;
	
	return GF_TRUE;
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
			gf_node_dirty_set(node, GF_SG_CHILD_DIRTY, GF_FALSE);
		}
		if (stack->cache) {
			if (stack->detached)
				gf_node_dirty_clear(node, GF_SG_CHILD_DIRTY);

			tr_state->subscene_not_over = 0;
			group_cache_traverse((GF_Node *)&stack->og, stack->cache, tr_state, stack->cache->force_recompute, GF_TRUE, stack->detached ? GF_TRUE : GF_FALSE);

			if (gf_node_dirty_get(node)) {
				gf_node_dirty_clear(node, GF_SG_CHILD_DIRTY);
			} else if ((stack->og.offscreen==2) && !stack->detached && !tr_state->subscene_not_over && stack->cache->txh.width && stack->cache->txh.height) {
				GF_FieldInfo field;
				if (gf_node_get_field(node, 0, &field) == GF_OK) {
					gf_node_unregister_children(node, *(GF_ChildNodeItem **) field.far_ptr);
					*(GF_ChildNodeItem **) field.far_ptr = NULL;
					stack->detached = GF_TRUE;
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
		else if (stack->cache && (tr_state->traversing_mode == TRAVERSE_PICK)) {
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
		if (!stack) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate offscreen group stack\n"));
			return;
		}
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

	CHECK_FIELD("DepthGroup", 0, GF_SG_VRML_MFNODE);
	dg->children = *(GF_ChildNodeItem **) field.far_ptr;

	CHECK_FIELD("DepthGroup", 1, GF_SG_VRML_SFFLOAT);
	dg->depth_gain = * (SFFloat *) field.far_ptr;

	CHECK_FIELD("DepthGroup", 2, GF_SG_VRML_SFFLOAT);
	dg->depth_offset = * (SFFloat *) field.far_ptr;

	return GF_TRUE;
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
			gf_node_dirty_set(node, GF_SG_CHILD_DIRTY, GF_FALSE);
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
		if (!stack) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate depth group stack\n"));
			return;
		}
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
		if (tr_state->layer3d) gf_node_dirty_set(tr_state->layer3d, GF_SG_NODE_DIRTY, GF_FALSE);
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

	CHECK_FIELD("IndexedCurve2D", 0, GF_SG_VRML_SFNODE);
	ic2d->point = * (GF_Node **) field.far_ptr;

	CHECK_FIELD("IndexedCurve2D", 1, GF_SG_VRML_SFFLOAT);
	ic2d->fineness = *(SFFloat *) field.far_ptr;

	CHECK_FIELD("IndexedCurve2D", 2, GF_SG_VRML_MFINT32);
	ic2d->type = *(MFInt32 *) field.far_ptr;

	CHECK_FIELD("IndexedCurve2D", 3, GF_SG_VRML_MFINT32);
	ic2d->index = *(MFInt32 *) field.far_ptr;

	return GF_TRUE;
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
		//clears dirty flag
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

	CHECK_FIELD("Untransform", 0, GF_SG_VRML_MFNODE);
	tr->children = *(GF_ChildNodeItem **) field.far_ptr;

	return GF_TRUE;
}


void TraverseUntransformEx(GF_Node *node, void *rs, GroupingNode2D *grp)
{
	GF_TraverseState *tr_state = (GF_TraverseState *) rs;

#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d) {
		GF_Matrix mx_model;
		GF_Camera backup_cam;

		if (!tr_state->camera) return;

		gf_mx_copy(mx_model, tr_state->model_matrix);
		gf_mx_init(tr_state->model_matrix);

		memcpy(&backup_cam, tr_state->camera, sizeof(GF_Camera));

		camera_invalidate(tr_state->camera);
		tr_state->camera->is_3D = GF_FALSE;
		tr_state->camera->flags |= CAM_NO_LOOKAT;
		tr_state->camera->end_zoom = FIX_ONE;
		camera_update(tr_state->camera, NULL, GF_TRUE);


		if (tr_state->traversing_mode == TRAVERSE_SORT) {
			visual_3d_set_viewport(tr_state->visual, tr_state->camera->proj_vp);
			visual_3d_projection_matrix_modified(tr_state->visual);

			gf_node_traverse_children(node, tr_state);

			gf_mx_copy(tr_state->model_matrix, mx_model);
			memcpy(tr_state->camera, &backup_cam, sizeof(GF_Camera));

			visual_3d_projection_matrix_modified(tr_state->visual);

			visual_3d_set_viewport(tr_state->visual, tr_state->camera->proj_vp);
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

			gf_node_traverse_children(node, tr_state);

			gf_mx_copy(tr_state->model_matrix, mx_model);
			memcpy(tr_state->camera, &backup_cam, sizeof(GF_Camera));
			tr_state->ray = r;

			/*nothing picked, restore previous pick*/
			if (!tr_state->visual->compositor->hit_square_dist)
				tr_state->visual->compositor->hit_square_dist = prev_dist;

		} else {
			gf_node_traverse_children(node, tr_state);

			gf_mx_copy(tr_state->model_matrix, mx_model);
			memcpy(tr_state->camera, &backup_cam, sizeof(GF_Camera));
		}

	} else
#endif
	{
		GF_Matrix2D mx2d_backup;
		gf_mx2d_copy(mx2d_backup, tr_state->transform);
		gf_mx2d_init(tr_state->transform);

		group_2d_traverse(node, grp, tr_state);

		gf_mx2d_copy(tr_state->transform, mx2d_backup);
	}
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
	TraverseUntransformEx((GF_Node *)&stack->untr, tr_state, (GroupingNode2D *)stack);
}

void compositor_init_untransform(GF_Compositor *compositor, GF_Node *node)
{
	Untransform tr;
	if (Untransform_GetNode(node, &tr)) {
		UntransformStack *stack;
		GF_SAFEALLOC(stack, UntransformStack);
		if (!stack) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate untransform stack\n"));
			return;
		}
		gf_node_set_private(node, stack);
		gf_node_set_callback_function(node, TraverseUntransform);
		stack->untr = tr;
		gf_node_proto_set_grouping(node);
	}
}



/*StyleGroup: overrides appearance of all children*/
typedef struct
{
	BASE_NODE
	CHILDREN

	GF_Node *appearance;
} StyleGroup;

typedef struct
{
	GROUPING_MPEG4_STACK_2D
	StyleGroup sg;
} StyleGroupStack;

static Bool StyleGroup_GetNode(GF_Node *node, StyleGroup *sg)
{
	GF_FieldInfo field;
	memset(sg, 0, sizeof(StyleGroup));
	sg->sgprivate = node->sgprivate;

	CHECK_FIELD("StyleGroup", 0, GF_SG_VRML_MFNODE);
	sg->children = *(GF_ChildNodeItem **) field.far_ptr;

	CHECK_FIELD("StyleGroup", 1, GF_SG_VRML_SFNODE);
	sg->appearance = *(GF_Node **)field.far_ptr;

	return GF_TRUE;
}


static void TraverseStyleGroup(GF_Node *node, void *rs, Bool is_destroy)
{
	Bool set = GF_FALSE;
	StyleGroupStack *stack = (StyleGroupStack *)gf_node_get_private(node);
	GF_TraverseState *tr_state = (GF_TraverseState *) rs;

	if (is_destroy) {
		gf_free(stack);
		return;
	}

	if (tr_state->traversing_mode==TRAVERSE_SORT) {
		if (gf_node_dirty_get(node) & GF_SG_NODE_DIRTY) {

			gf_node_dirty_clear(node, GF_SG_NODE_DIRTY);
			/*flag is not set for PROTO*/
			gf_node_dirty_set(node, GF_SG_CHILD_DIRTY, GF_FALSE);
		}
	}
	StyleGroup_GetNode(node, &stack->sg);

	if (!tr_state->override_appearance) {
		set = GF_TRUE;
		tr_state->override_appearance = stack->sg.appearance;
	}
	group_2d_traverse((GF_Node *)&stack->sg, (GroupingNode2D*)stack, tr_state);

	if (set) {
		tr_state->override_appearance = NULL;
	}
}

void compositor_init_style_group(GF_Compositor *compositor, GF_Node *node)
{
	StyleGroup sg;
	if (StyleGroup_GetNode(node, &sg)) {
		StyleGroupStack *stack;
		GF_SAFEALLOC(stack, StyleGroupStack);
		if (!stack) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate style group stack\n"));
			return;
		}
		gf_node_set_private(node, stack);
		gf_node_set_callback_function(node, TraverseStyleGroup);
		stack->sg = sg;
		gf_node_proto_set_grouping(node);
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor2D] Unable to initialize style group\n"));
	}
}



/*TestSensor: tests eventIn/eventOuts for hardcoded proto*/
typedef struct
{
	BASE_NODE

	Bool onTrigger;
	Fixed value;
} TestSensor;

typedef struct
{
	TestSensor ts;
} TestSensorStack;

static Bool TestSensor_GetNode(GF_Node *node, TestSensor *ts)
{
	GF_FieldInfo field;
	memset(ts, 0, sizeof(TestSensor));
	ts->sgprivate = node->sgprivate;

	CHECK_FIELD("TestSensor", 0, GF_SG_VRML_SFBOOL);
	if (field.eventType != GF_SG_EVENT_IN) return GF_FALSE;
	ts->onTrigger = *(SFBool *)field.far_ptr;

	CHECK_FIELD("TestSensor", 1, GF_SG_VRML_SFFLOAT);
	if (field.eventType != GF_SG_EVENT_EXPOSED_FIELD) return GF_FALSE;
	ts->value = *(SFFloat *)field.far_ptr;

	CHECK_FIELD("TestSensor", 2, GF_SG_VRML_SFFLOAT);
	if (field.eventType != GF_SG_EVENT_OUT) return GF_FALSE;

	return GF_TRUE;
}


static void TraverseTestSensor(GF_Node *node, void *rs, Bool is_destroy)
{
	TestSensorStack *stack = (TestSensorStack *)gf_node_get_private(node);

	if (is_destroy) {
		gf_free(stack);
		return;
	}
}

void TestSensor_OnTrigger(GF_Node *node, struct _route *route)
{
	GF_FieldInfo field;
	Fixed value;
	TestSensorStack *stack = (TestSensorStack *)gf_node_get_private(node);
	TestSensor_GetNode(node, &stack->ts);

	if (stack->ts.onTrigger) {
		value = stack->ts.value;
	} else {
		value = 1-stack->ts.value;
	}

	gf_node_get_field(node, 2, &field);
	*(SFFloat*)field.far_ptr = value;
	gf_node_event_out(node, 2);
}

void compositor_init_test_sensor(GF_Compositor *compositor, GF_Node *node)
{
	TestSensor ts;
	if (TestSensor_GetNode(node, &ts)) {
		GF_Err e;
		TestSensorStack *stack;
		GF_SAFEALLOC(stack, TestSensorStack);
		if (!stack) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate test sensor stack\n"));
			return;
		}
		gf_node_set_private(node, stack);
		gf_node_set_callback_function(node, TraverseTestSensor);
		stack->ts = ts;

		e = gf_node_set_proto_eventin_handler(node, 0, TestSensor_OnTrigger);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to initialize Proto TestSensor callback: %s\n", gf_error_to_string(e) ));
		}
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] Unable to initialize test sensor\n"));
	}
}


/*CustomTexture: tests defining new (OpenGL) textures*/
typedef struct
{
    BASE_NODE
    
    Fixed intensity;
} CustomTexture;

typedef struct
{
    CustomTexture tx;
    GF_TextureHandler txh;
    u32 gl_id;
    Bool disabled;
} CustomTextureStack;

static Bool CustomTexture_GetNode(GF_Node *node, CustomTexture *tx)
{
    GF_FieldInfo field;
    memset(tx, 0, sizeof(CustomTexture));
    tx->sgprivate = node->sgprivate;
    
    CHECK_FIELD("CustomTexture", 0, GF_SG_VRML_SFFLOAT);
	if (field.eventType != GF_SG_EVENT_EXPOSED_FIELD) return GF_FALSE;
    tx->intensity = *(SFFloat *)field.far_ptr;
    
    return GF_TRUE;
}

static void TraverseCustomTexture(GF_Node *node, void *rs, Bool is_destroy)
{
    CustomTextureStack *stack = (CustomTextureStack *)gf_node_get_private(node);
    
    if (is_destroy) {
        //release texture object
        gf_sc_texture_destroy(&stack->txh);
        gf_free(stack);
        return;
    }
}

#ifndef GPAC_DISABLE_3D
#include "gl_inc.h"
#endif

static void CustomTexture_update(GF_TextureHandler *txh)
{
#ifndef GPAC_DISABLE_3D
    u8 data[16];
#endif
    CustomTextureStack *stack = gf_node_get_private(txh->owner);
    //alloc texture
    if (!txh->tx_io) {
        //allocate texture
        gf_sc_texture_allocate(txh);
        if (!txh->tx_io) return;
    }
    if (stack->disabled) return;
    
#ifndef GPAC_DISABLE_3D
    //texture not setup, do it
    if (! gf_sc_texture_get_gl_id(txh)) {
        
        //setup some defaults (these two vars are used to setup internal texture format)
        //in our case we only want to test OpenGL so no need to fill in the texture width/height stride
        //since we will upload ourselves the texture
        txh->transparent = 0;
        txh->pixelformat = GF_PIXEL_RGB;

        //signaling we modified associated data (even if no data in our case) to mark texture as dirty
        gf_sc_texture_set_data(txh);
        
        //trigger HW setup of the texture
        gf_sc_texture_push_image(txh, GF_FALSE, GF_FALSE);
        
        //OK we have a valid textureID
        stack->gl_id = gf_sc_texture_get_gl_id(txh);
    }
#endif


    //get current value of node->value
    CustomTexture_GetNode(txh->owner, &stack->tx);

#ifndef GPAC_DISABLE_3D
    //setup our texture data
    memset(data, 0, sizeof(char)*12);
    data[0] = (u8) (0xFF * FIX2FLT(stack->tx.intensity)); //first pixel red modulated by intensity
    data[4] = (u8) (0xFF * FIX2FLT(stack->tx.intensity)); //second pixel green
    data[8] = (u8) (0xFF * FIX2FLT(stack->tx.intensity)); //third pixel blue
    //last pixel black
    
    glBindTexture( GL_TEXTURE_2D, stack->gl_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    
#endif
 
}

void compositor_init_custom_texture(GF_Compositor *compositor, GF_Node *node)
{
    CustomTexture tx;
    if (CustomTexture_GetNode(node, &tx)) {
        CustomTextureStack *stack;
        GF_SAFEALLOC(stack, CustomTextureStack);
		if (!stack) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate custom texture group stack\n"));
			return;
		}
        gf_node_set_private(node, stack);
        gf_node_set_callback_function(node, TraverseCustomTexture);
        stack->tx = tx;

		if (!gf_sc_check_gl_support(compositor)) {
			stack->disabled = GF_TRUE;
			GF_LOG(GF_LOG_WARNING, GF_LOG_COMPOSE, ("[Compositor] Driver disabled, cannot render custom texture test\n"));
			return;
		}
        //register texture object
        gf_sc_texture_setup(&stack->txh, compositor, node);
        stack->txh.update_texture_fcnt = CustomTexture_update;
    
    } else {
        GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] Unable to initialize custom texture\n"));
    }
}

#ifndef GPAC_DISABLE_3D

static void TraverseVRGeometry(GF_Node *node, void *rs, Bool is_destroy)
{
	GF_TextureHandler *txh;
	GF_MediaObjectVRInfo vrinfo;
	GF_MeshSphereAngles sphere_angles;
	Bool mesh_was_reset = GF_FALSE;
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;
	Drawable3D *stack = (Drawable3D *)gf_node_get_private(node);

	if (is_destroy) {
		drawable_3d_del(node);
		return;
	}

	if (!tr_state->appear || ! ((M_Appearance *)tr_state->appear)->texture)
		return;
	
	txh = gf_sc_texture_get_handler( ((M_Appearance *) tr_state->appear)->texture );
	if (!txh->stream) return;

	if (gf_node_dirty_get(node) || (tr_state->traversing_mode==TRAVERSE_DRAW_3D)) {
		if (! gf_mo_get_srd_info(txh->stream, &vrinfo))
			return;

		if (vrinfo.has_full_coverage && tr_state->disable_partial_sphere) {
			if ((vrinfo.srd_w!=vrinfo.srd_max_x) || (vrinfo.srd_h!=vrinfo.srd_max_y))
				return;
		}

		sphere_angles.min_phi = -GF_PI2 + GF_PI * vrinfo.srd_y / vrinfo.srd_max_y;
		sphere_angles.max_phi = -GF_PI2 + GF_PI * (vrinfo.srd_h +  vrinfo.srd_y) / vrinfo.srd_max_y;

		sphere_angles.min_theta = GF_2PI * vrinfo.srd_x / vrinfo.srd_max_x;
		sphere_angles.max_theta = GF_2PI * ( vrinfo.srd_w + vrinfo.srd_x ) / vrinfo.srd_max_x;

		if (gf_node_dirty_get(node)) {
			u32 radius;
			mesh_reset(stack->mesh);

			radius = MAX(vrinfo.scene_width, vrinfo.scene_height) / 4;
			//may happen that we don't have a scene width/height, use hardcoded 100 units radius (size actually doesn't matter
			//since our VP/camera is at the center of the sphere
			if (!radius) {
				radius = 100;
			}

			if (radius) {
				mesh_new_sphere(stack->mesh, -1 * INT2FIX(radius), GF_FALSE, &sphere_angles);

				txh->flags &= ~GF_SR_TEXTURE_REPEAT_S;
				txh->flags &= ~GF_SR_TEXTURE_REPEAT_T;
			}
			mesh_was_reset = GF_TRUE;
			gf_node_dirty_clear(node, GF_SG_NODE_DIRTY);
		}
		
		
		if (tr_state->traversing_mode==TRAVERSE_DRAW_3D) {
			Bool visible = GF_FALSE;
#ifndef GPAC_DISABLE_LOG
			const char *pid_name = gf_filter_pid_get_name(txh->stream->odm->pid);
#endif
			if (! tr_state->camera_was_dirty && !mesh_was_reset) {
				visible = (stack->mesh->flags & MESH_WAS_VISIBLE) ? GF_TRUE : GF_FALSE;
			} else if ((vrinfo.srd_w==vrinfo.srd_max_x) && (vrinfo.srd_h==vrinfo.srd_max_y)) {
				visible = GF_TRUE;
			}
			else if (txh->compositor->tvtf) {
				visible = GF_TRUE;
			//estimate visibility asap, even if texture not yet ready (we have SRD info):
			//this allows sending stop commands which will free inactive decoder HW context
			} else {
				u32 i, j;
				u32 nb_visible=0;
				u32 nb_tests = tr_state->visual->compositor->tvtn;
				u32 min_visible_threshold = tr_state->visual->compositor->tvtt;
				u32 stride;

				//pick nb_tests vertices spaced every stride in the mesh
				stride = stack->mesh->v_count;
				stride /= nb_tests;
				for (i=0; i<nb_tests; i++) {
					Bool vis = GF_TRUE;
					GF_Vec pt = stack->mesh->vertices[i*stride].pos;
					//check the point is in our frustum - don't test far plane
					for (j=1; j<6; j++) {
						Fixed d = gf_plane_get_distance(&tr_state->camera->planes[j], &pt);
						if (d<0) {
							vis = GF_FALSE;
							break;
						}
					}
					if (vis) {
						nb_visible++;
						//abort test if more visible points than our threshold
						if (nb_visible > min_visible_threshold)
							break;
					}
				}
				if (nb_visible > min_visible_threshold) 
					visible = GF_TRUE;
				GF_LOG(GF_LOG_INFO, GF_LOG_COMPOSE, ("[Compositor] Texture %s Partial sphere is %s - %d sample points visible out of %d\n", pid_name, visible ? "visible" : "hidden",  nb_visible, i));
			}

			if (visible) {
				stack->mesh->flags |= MESH_WAS_VISIBLE;
			} else {
				stack->mesh->flags &= ~MESH_WAS_VISIBLE;
			}

			if (visible && (vrinfo.srd_w != vrinfo.srd_max_x) && tr_state->visual->compositor->gazer_enabled) {
				s32 gx, gy;
				tr_state->visual->compositor->hit_node = NULL;
				tr_state->visual->compositor->hit_square_dist = 0;

				//gaze coords are 0,0 in top-left
				gx = (s32)( tr_state->visual->compositor->gaze_x - tr_state->camera->width/2 );
				gy = (s32)( tr_state->camera->height/2 - tr_state->visual->compositor->gaze_y );

				visual_3d_setup_ray(tr_state->visual, tr_state, gx, gy);
				visual_3d_vrml_drawable_pick(node, tr_state, stack->mesh, NULL);
				if (tr_state->visual->compositor->hit_node) {
					GF_LOG(GF_LOG_INFO, GF_LOG_COMPOSE, ("[Compositor] Texture %s Partial sphere is under gaze coord %d %d\n", pid_name, tr_state->visual->compositor->gaze_x, tr_state->visual->compositor->gaze_y));

					tr_state->visual->compositor->hit_node = NULL;
				} else {
					visible = GF_FALSE;
				}

			}

			if (vrinfo.has_full_coverage) {
				if (visible) {
					if (!txh->is_open) {
						GF_LOG(GF_LOG_INFO, GF_LOG_COMPOSE, ("[Compositor] Texture %s stopped on visible partial sphere - starting it\n", pid_name));
						assert(txh->stream && txh->stream->odm);
						txh->stream->odm->disable_buffer_at_next_play = GF_TRUE;
						txh->stream->odm->flags |= GF_ODM_TILED_SHARED_CLOCK;
						gf_sc_texture_play_from_to(txh, NULL, -1, -1, 1, 0);
					}

					if (txh->data) {
						Bool do_show = GF_TRUE;
						Bool is_full_cover =  (vrinfo.srd_w == vrinfo.srd_max_x) ? GF_TRUE : GF_FALSE;
						visual_3d_enable_depth_buffer(tr_state->visual, GF_FALSE);
						visual_3d_enable_antialias(tr_state->visual, GF_FALSE);
						if ((tr_state->visual->compositor->tvtd == TILE_DEBUG_FULL) && !is_full_cover) do_show = GF_FALSE;
						else if ((tr_state->visual->compositor->tvtd == TILE_DEBUG_PARTIAL) && is_full_cover) do_show = GF_FALSE;

						if (do_show) {
							visual_3d_draw(tr_state, stack->mesh);
						}
						visual_3d_enable_depth_buffer(tr_state->visual, GF_TRUE);
					}
				} else {
					if (txh->is_open) {
						GF_LOG(GF_LOG_INFO, GF_LOG_COMPOSE, ("[Compositor] Texture %s playing on hidden partial sphere - stopping it\n", pid_name));
						gf_sc_texture_stop_no_unregister(txh);
					}
				}
			} else {
				if (txh->data) {
					visual_3d_enable_depth_buffer(tr_state->visual, GF_FALSE);
					visual_3d_enable_antialias(tr_state->visual, GF_FALSE);
					visual_3d_draw(tr_state, stack->mesh);
					visual_3d_enable_depth_buffer(tr_state->visual, GF_TRUE);
				}
				if (!tr_state->disable_partial_sphere) {
					if (visible) {
						gf_mo_hint_quality_degradation(txh->stream, 0);
					} else {
						gf_mo_hint_quality_degradation(txh->stream, 100);
					}
				}
			}
		}
	}
	if (tr_state->traversing_mode==TRAVERSE_GET_BOUNDS) {
		tr_state->bbox = stack->mesh->bounds;
	}
}


static void compositor_init_vr_geometry(GF_Compositor *compositor, GF_Node *node)
{
	drawable_3d_new(node);
	gf_node_set_callback_function(node, TraverseVRGeometry);
}

#define VRHUD_SCALE	6
static void TraverseVRHUD(GF_Node *node, void *rs, Bool is_destroy)
{
	GF_TraverseState *tr_state = (GF_TraverseState *) rs;
	GF_Matrix mv_bck, proj_bck, cam_bck;
	/*SFVec3f target;*/
	GF_Rect vp, orig_vp;
	u32 mode, i, cull_bck;
	Fixed angle_yaw/*, angle_pitch*/;
	SFVec3f axis;
	GF_Node *subtree = gf_node_get_private(node);
	if (is_destroy) return;

	if (!tr_state->camera) return;
	mode = tr_state->visual->compositor->vrhud_mode;
	if (!mode) return;

	gf_mx_copy(mv_bck, tr_state->model_matrix);
	gf_mx_copy(cam_bck, tr_state->camera->modelview);

	tr_state->disable_partial_sphere = GF_TRUE;
	/*target = tr_state->camera->target;*/
	orig_vp = tr_state->camera->proj_vp;

/*
	//compute pitch (elevation)
	dlen = tr_state->camera->target.x*tr_state->camera->target.x + tr_state->camera->target.z*tr_state->camera->target.z;
	dlen = gf_sqrt(dlen);
*/

	/*angle_pitch = gf_atan2(tr_state->camera->target.y, dlen);*/

	//compute yaw (rotation Y)
	angle_yaw = gf_atan2(tr_state->camera->target.z, tr_state->camera->target.x);

	//compute axis for the pitch
	axis = tr_state->camera->target;
	axis.y=0;
	gf_vec_norm(&axis);

	visual_3d_enable_depth_buffer(tr_state->visual, GF_FALSE);

	if (mode==2) {
		//rear mirror, reverse x-axis on projection
		tr_state->camera->projection.m[0] *= -1;
		visual_3d_projection_matrix_modified(tr_state->visual);
		//inverse backface culling
		tr_state->reverse_backface = GF_TRUE;
		vp = orig_vp;
		vp.width/=VRHUD_SCALE;
		vp.height/=VRHUD_SCALE;
		vp.x = orig_vp.x + (orig_vp.width-vp.width)/2;
		vp.y = orig_vp.y + orig_vp.height-vp.height;

		visual_3d_set_viewport(tr_state->visual, vp);

		gf_mx_add_rotation(&tr_state->model_matrix, GF_PI, tr_state->camera->up.x, tr_state->camera->up.y, tr_state->camera->up.z);
		gf_node_traverse(subtree, rs);
		tr_state->camera->projection.m[0] *= -1;
		visual_3d_projection_matrix_modified(tr_state->visual);
	} else if (mode==1) {
		gf_mx_copy(proj_bck, tr_state->camera->projection);
		gf_mx_init(tr_state->camera->modelview);

		//force projection with PI/2 fov and AR 1:1
		tr_state->camera->projection.m[0] = -1;
		tr_state->camera->projection.m[5] = 1;
		visual_3d_projection_matrix_modified(tr_state->visual);
		//force cull inside
		cull_bck = tr_state->cull_flag;
		tr_state->cull_flag = CULL_INSIDE;
		//inverse backface culling
		tr_state->reverse_backface = GF_TRUE;

		//draw 3 viewports, each separated by PI/2 rotation
		for (i=0; i<3; i++) {
			vp = orig_vp;
			vp.height/=VRHUD_SCALE;
			vp.width=vp.height;
			//we reverse X in the projection, so reverse the viewports
			vp.x = orig_vp.x + orig_vp.width/2 - 3*vp.width/2 + (3-i-1)*vp.width;
			vp.y = orig_vp.y + orig_vp.height - vp.height;
			visual_3d_set_viewport(tr_state->visual, vp);
			tr_state->disable_cull = GF_TRUE;

			gf_mx_init(tr_state->model_matrix);
			gf_mx_add_rotation(&tr_state->model_matrix, angle_yaw-GF_PI, 0, 1, 0);
			gf_mx_add_rotation(&tr_state->model_matrix, i*GF_PI2, 0, 1, 0);

			gf_node_traverse(subtree, rs);
		}
		gf_mx_copy(tr_state->camera->projection, proj_bck);
		visual_3d_projection_matrix_modified(tr_state->visual);
		tr_state->cull_flag = cull_bck;
		gf_mx_copy(tr_state->camera->modelview, cam_bck);
	}
	else if ((mode==4) || (mode==3)) {
		// mirror, reverse x-axis on projection
		tr_state->camera->projection.m[0] *= -1;
		visual_3d_projection_matrix_modified(tr_state->visual);
		//inverse backface culling
		tr_state->reverse_backface = GF_TRUE;

		//side left view
		vp = orig_vp;
		vp.width/=VRHUD_SCALE;
		vp.height/=VRHUD_SCALE;
		if (mode==3) {
			vp.x = orig_vp.x;
		} else {
			vp.x = orig_vp.x + orig_vp.width/2 - 2*vp.width;
		}
		vp.y = orig_vp.y + orig_vp.height - vp.height;
		visual_3d_set_viewport(tr_state->visual, vp);

		gf_mx_add_rotation(&tr_state->model_matrix, -2*GF_PI/3, 0, 1, 0);

		gf_node_traverse(subtree, rs);

		//side right view
		if (mode==3) {
			vp.x = orig_vp.x + orig_vp.width - vp.width;
		} else {
			vp.x = orig_vp.x + orig_vp.width/2+vp.width;
		}
		visual_3d_set_viewport(tr_state->visual, vp);

		gf_mx_copy(tr_state->model_matrix, mv_bck);
		gf_mx_add_rotation(&tr_state->model_matrix, 2*GF_PI/3, 0, 1, 0);

		gf_node_traverse(subtree, rs);

		if (mode==4) {
			//upper view
			vp.x = orig_vp.x + orig_vp.width/2 - vp.width;
			visual_3d_set_viewport(tr_state->visual, vp);

			gf_mx_copy(tr_state->model_matrix, mv_bck);
			gf_mx_add_rotation(&tr_state->model_matrix, - GF_PI2, -axis.z, 0, axis.x);
			gf_node_traverse(subtree, rs);

			//down view
			vp.x = orig_vp.x + orig_vp.width/2;
			visual_3d_set_viewport(tr_state->visual, vp);

			gf_mx_copy(tr_state->model_matrix, mv_bck);
			gf_mx_add_rotation(&tr_state->model_matrix, GF_PI2, -axis.z, 0, axis.x);

			gf_node_traverse(subtree, rs);
		}

		tr_state->camera->projection.m[0] *= -1;
		visual_3d_projection_matrix_modified(tr_state->visual);
	}

	//restore camera and VP
	gf_mx_copy(tr_state->model_matrix, mv_bck);
	visual_3d_set_viewport(tr_state->visual, orig_vp);
	visual_3d_enable_depth_buffer(tr_state->visual, GF_TRUE);
	tr_state->disable_partial_sphere = GF_FALSE;
	tr_state->reverse_backface = GF_FALSE;
}

void compositor_init_vrhud(GF_Compositor *compositor, GF_Node *node)
{
	GF_Node *n;
	GF_SceneGraph *sg = gf_node_get_graph(node);
	sg = gf_sg_get_parent(sg);

	n = gf_sg_find_node_by_name(sg, "DYN_TRANS");
	if (!n) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Unable to initialize VRHUD group, no main scene\n"));
	} else {
		gf_node_set_callback_function(node, TraverseVRHUD);
		gf_node_proto_set_grouping(node);
		gf_node_set_private(node, n);
	}
}


#endif //GPAC_DISABLE_3D

/*hardcoded proto loading - this is mainly used for module development and testing...*/
void gf_sc_init_hardcoded_proto(GF_Compositor *compositor, GF_Node *node)
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
        if (!url) continue;
        
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
        if (!strcmp(url, "urn:inet:gpac:builtin:VRGeometry")) {
            compositor_init_vr_geometry(compositor, node);
            return;
        }
        if (!strcmp(url, "urn:inet:gpac:builtin:VRHUD")) {
            compositor_init_vrhud(compositor, node);
            return;
        }
#endif
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
#ifdef GPAC_ENABLE_FLASHSHAPE
			compositor_init_hc_flashshape(compositor, node);
#endif
			return;
		}
		if (!strcmp(url, "urn:inet:gpac:builtin:StyleGroup")) {
			compositor_init_style_group(compositor, node);
			return;
		}
		if (!strcmp(url, "urn:inet:gpac:builtin:TestSensor")) {
			compositor_init_test_sensor(compositor, node);
			return;
		}
        if (!strcmp(url, "urn:inet:gpac:builtin:CustomTexture")) {
            compositor_init_custom_texture(compositor, node);
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
				return GF_TRUE;
			}
		}
	}
	return GF_FALSE;
}

GF_TextureHandler *gf_sc_hardcoded_proto_get_texture_handler(GF_Node *n)
{
    
    MFURL *proto_url;
    GF_Proto *proto;
    u32 i;
    
    proto = gf_node_get_proto(n);
    if (!proto) return NULL;
    proto_url = gf_sg_proto_get_extern_url(proto);
    
    for (i=0; i<proto_url->count; i++) {
        const char *url = proto_url->vals[0].url;
        if (!strcmp(url, "urn:inet:gpac:builtin:CustomTexture")) {
            CustomTextureStack *stack = gf_node_get_private(n);
            if (stack) return &stack->txh;
        }
    }
    return NULL;
}

#endif /*GPAC_DISABLE_VRML*/

#endif // GPAC_DISABLE_COMPOSITOR
