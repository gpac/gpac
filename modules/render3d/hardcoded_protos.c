/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / 3D rendering module
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

#include "render3d_nodes.h"
#include "grouping.h"


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

static void RenderPathExtrusion(GF_Node *node, void *rs, Bool is_destroy)
{
	PathExtrusion path_ext;
	stack2D *st2D;
	RenderEffect3D *eff = (RenderEffect3D *)rs;
	DrawableStack *st = (DrawableStack *)gf_node_get_private(node);
	
	if (is_destroy) {
		drawable_node_destroy(node);
		return;
	}
	if (!PathExtrusion_GetNode(node, &path_ext)) return;
	if (!path_ext.geometry) return;


	if (gf_node_dirty_get(node)) {
		gf_node_render(path_ext.geometry, eff);
		gf_node_dirty_clear(node, 0);
		switch (gf_node_get_tag(path_ext.geometry) ) {
		case TAG_MPEG4_Circle:
		case TAG_MPEG4_Ellipse:
		case TAG_MPEG4_Rectangle:
		case TAG_MPEG4_Curve2D:
		case TAG_MPEG4_XCurve2D:
		case TAG_MPEG4_IndexedFaceSet2D:
		case TAG_MPEG4_IndexedLineSet2D:
			st2D = (stack2D*)gf_node_get_private(path_ext.geometry);
			if (!st2D) return;
			mesh_extrude_path(st->mesh, st2D->path, path_ext.spine, path_ext.creaseAngle, path_ext.beginCap, path_ext.endCap, path_ext.orientation, path_ext.scale, path_ext.txAlongSpine);
			break;
		case TAG_MPEG4_Text:
			Text_Extrude(path_ext.geometry, eff, st->mesh, path_ext.spine, path_ext.creaseAngle, path_ext.beginCap, path_ext.endCap, path_ext.orientation, path_ext.scale, path_ext.txAlongSpine);
			break;
		}
	}

	if (!eff->traversing_mode) {
		VS_DrawMesh(eff, st->mesh);
	} else if (eff->traversing_mode==TRAVERSE_GET_BOUNDS) {
		eff->bbox = st->mesh->bounds;
	}
}

void R3D_InitPathExtrusion(Render3D *sr, GF_Node *node)
{
	BaseDrawableStack(sr->compositor, node);
	gf_node_set_callback_function(node, RenderPathExtrusion);
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

static void RenderPlanarExtrusion(GF_Node *node, void *rs, Bool is_destroy)
{
	PlanarExtrusion plane_ext;
	stack2D *st2D;
	u32 i, j, k;
	MFVec3f spine_vec;
	SFVec3f d;
	Fixed spine_len;
	GF_Rect bounds;
	GF_Path *geo, *spine;
	RenderEffect3D *eff = (RenderEffect3D *)rs;
	DrawableStack *st = (DrawableStack *)gf_node_get_private(node);

	if (is_destroy) {
		drawable_node_destroy(node);
		return;
	}

	if (!PlanarExtrusion_GetNode(node, &plane_ext)) return;
	if (!plane_ext.geometry || !plane_ext.spine) return;


	if (gf_node_dirty_get(node)) {
		u32 cur, nb_pts;
		geo = spine = NULL;
		gf_node_render(plane_ext.geometry, eff);
		gf_node_render(plane_ext.spine, eff);
		gf_node_dirty_clear(node, 0);
		switch (gf_node_get_tag(plane_ext.geometry) ) {
		case TAG_MPEG4_Circle:
		case TAG_MPEG4_Ellipse:
		case TAG_MPEG4_Rectangle:
		case TAG_MPEG4_Curve2D:
		case TAG_MPEG4_XCurve2D:
		case TAG_MPEG4_IndexedFaceSet2D:
		case TAG_MPEG4_IndexedLineSet2D:
			st2D = (stack2D*)gf_node_get_private(plane_ext.geometry);
			if (st2D) geo = st2D->path;
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
			st2D = (stack2D*)gf_node_get_private(plane_ext.spine);
			if (st2D) spine = st2D->path;
			break;
		default:
			return;
		}
		if (!geo || !spine) return;

		mesh_reset(st->mesh);
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
				mesh_extrude_path_ext(st->mesh, geo, &spine_vec, plane_ext.creaseAngle, 
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

				mesh_extrude_path_ext(st->mesh, geo, &spine_vec, plane_ext.creaseAngle, 
					bounds.x, bounds.y-bounds.height, bounds.width, bounds.height, 
					plane_ext.beginCap, plane_ext.endCap, &ori, &scale, plane_ext.txAlongSpine);

				gf_sg_vrml_mf_reset(&ori, GF_SG_VRML_MFROTATION);
				gf_sg_vrml_mf_reset(&scale, GF_SG_VRML_MFVEC2F);
			}

			gf_sg_vrml_mf_reset(&spine_vec, GF_SG_VRML_MFVEC3F);
		}
		mesh_update_bounds(st->mesh);
		gf_mesh_build_aabbtree(st->mesh);
	}

	if (!eff->traversing_mode) {
		VS_DrawMesh(eff, st->mesh);
	} else if (eff->traversing_mode==TRAVERSE_GET_BOUNDS) {
		eff->bbox = st->mesh->bounds;
	}
}

void R3D_InitPlanarExtrusion(Render3D *sr, GF_Node *node)
{
	BaseDrawableStack(sr->compositor, node);
	gf_node_set_callback_function(node, RenderPlanarExtrusion);
}

/*PlaneClipper hardcoded proto*/
typedef struct
{
    SFVec3f normal;
    Fixed dist;
	GF_ChildNodeItem *children;
} PlaneClipper;

static Bool PlaneClipper_GetNode(GF_Node *node, PlaneClipper *pc)
{
	GF_FieldInfo field;
	memset(pc, 0, sizeof(PlaneClipper));
	if (gf_node_get_field(node, 0, &field) != GF_OK) return 0;
	if (field.fieldType != GF_SG_VRML_SFVEC3F) return 0;
	pc->normal = * (SFVec3f *) field.far_ptr;
	if (gf_node_get_field(node, 1, &field) != GF_OK) return 0;
	if (field.fieldType != GF_SG_VRML_SFFLOAT) return 0;
	pc->dist = * (SFFloat *) field.far_ptr;
	if (gf_node_get_field(node, 2, &field) != GF_OK) return 0;
	if (field.fieldType != GF_SG_VRML_MFNODE) return 0;
	pc->children = *(GF_ChildNodeItem **) field.far_ptr;
	return 1;
}

static void RenderPlaneClipper(GF_Node *node, void *rs, Bool is_destroy)
{
	PlaneClipper pc;
	GF_Plane p;
	GroupingNode *st = (GroupingNode *)gf_node_get_private(node);
	RenderEffect3D *eff = (RenderEffect3D *) rs;

	if (is_destroy) {
		DeleteGroupingNode(st);
		return;
	}

	if (!PlaneClipper_GetNode(node, &pc)) return;

	if (eff->num_clip_planes==MAX_USER_CLIP_PLANES) {
		grouping_traverse(st, eff, NULL);
		return;
	}

	p.normal = pc.normal;
	p.d = pc.dist;
	eff->clip_planes[eff->num_clip_planes] = p;
	gf_mx_apply_plane(&eff->model_matrix, &eff->clip_planes[eff->num_clip_planes]);
	eff->num_clip_planes++;

	if (eff->traversing_mode == TRAVERSE_SORT) {
		GF_Plane p;
		p.normal = pc.normal;
		p.d = pc.dist;
		VS3D_SetClipPlane(eff->surface, p);
		grouping_traverse(st, eff, NULL);
		VS3D_ResetClipPlane(eff->surface);
	} else {
		grouping_traverse(st, eff, NULL);
	}

	eff->num_clip_planes--;
}

void R3D_InitPlaneClipper(Render3D *sr, GF_Node *node)
{
	PlaneClipper pc;
	if (PlaneClipper_GetNode(node, &pc)) {
		GroupingNode *stack = (GroupingNode *)malloc(sizeof(GroupingNode));
		SetupGroupingNode(stack, sr->compositor, node, & pc.children);
		gf_node_set_private(node, stack);
		gf_node_set_callback_function(node, RenderPlaneClipper);
		/*we're a grouping node, force bounds rebuild as soon as loaded*/
		gf_node_dirty_set(node, GF_SG_CHILD_DIRTY, 0);
	}
}


/*hardcoded proto loading - this is mainly used for module development and testing...*/
void R3D_InitHardcodedProto(Render3D *sr, GF_Node *node)
{
	MFURL *proto_url;
	GF_Proto *proto;
	u32 i;

	proto = gf_node_get_proto(node);
	if (!proto) return;
	proto_url = gf_sg_proto_get_extern_url(proto);

	for (i=0; i<proto_url->count; i++) {
		const char *url = proto_url->vals[0].url;
		if (!strnicmp(url, "urn:inet:gpac:builtin:PathExtrusion", 22 + 13)) {
			R3D_InitPathExtrusion(sr, node);
			return;
		}
		if (!strnicmp(url, "urn:inet:gpac:builtin:PlanarExtrusion", 22 + 15)) {
			R3D_InitPlanarExtrusion(sr, node);
			return;
		}
		if (!strnicmp(url, "urn:inet:gpac:builtin:PlaneClipper", 22 + 12)) {
			R3D_InitPlaneClipper(sr, node);
			return;
		}
		if (!strnicmp(url, "urn:inet:gpac:builtin:TextureText", 22 + 11)) {
			void R3D_InitTextureText(Render3D *sr, GF_Node *node);
			R3D_InitTextureText(sr, node);
			return;
		}
	}
}

