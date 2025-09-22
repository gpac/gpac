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
#include "mpeg4_grouping.h"
#include "visual_manager.h"

#if !defined(GPAC_DISABLE_VRML) && !defined(GPAC_DISABLE_COMPOSITOR)

typedef struct
{
	PARENT_MPEG4_STACK_2D

	GF_Node *last_geom;
	GF_PathIterator *iter;
} PathLayoutStack;




static void TraversePathLayout(GF_Node *node, void *rs, Bool is_destroy)
{
	u32 i, count, minor, major, int_bck;
	Fixed length, offset, length_after_point;
	Bool res;
	u32 mode_bckup;
	ChildGroup *cg;
#ifndef GPAC_DISABLE_3D
	GF_Matrix mat;
#endif
	GF_Matrix2D mx2d;
	ParentNode2D *parent_bck;
	PathLayoutStack *gr = (PathLayoutStack*) gf_node_get_private(node);
	M_PathLayout *pl = (M_PathLayout *)node;
	GF_TraverseState *tr_state = (GF_TraverseState *) rs;

	if (is_destroy) {
		parent_node_predestroy((ParentNode2D *)gr);
		if (gr->iter) gf_path_iterator_del(gr->iter);
		gf_free(gr);
		return;
	}
	if (!pl->geometry) return;

	/*only low-level primitives allowed*/
	switch (gf_node_get_tag((GF_Node *) pl->geometry)) {
	case TAG_MPEG4_Rectangle:
		return;
	case TAG_MPEG4_Circle:
		return;
	case TAG_MPEG4_Ellipse:
		return;
	}

	/*store traversing state*/
#ifndef GPAC_DISABLE_3D
	gf_mx_copy(mat, tr_state->model_matrix);
	gf_mx_init(tr_state->model_matrix);
#endif

	gf_mx2d_copy(mx2d, tr_state->transform);
	gf_mx2d_init(tr_state->transform);

	parent_bck = tr_state->parent;
	tr_state->parent = NULL;

	/*check geom changes*/
	if ((pl->geometry != gr->last_geom) || gf_node_dirty_get(pl->geometry)) {
		if (gr->iter) gf_path_iterator_del(gr->iter);
		gr->iter = NULL;

		int_bck = tr_state->switched_off;
		mode_bckup = tr_state->traversing_mode;
		tr_state->traversing_mode = TRAVERSE_GET_BOUNDS;
		tr_state->switched_off = 1;
		gf_node_traverse((GF_Node *)pl->geometry, tr_state);
		tr_state->traversing_mode = mode_bckup;
		tr_state->switched_off = int_bck;
	}

	if (!gr->iter) {
		Drawable *dr = NULL;
		/*get the drawable */
		switch (gf_node_get_tag(pl->geometry) ) {
		case TAG_MPEG4_Circle:
		case TAG_MPEG4_Curve2D:
		case TAG_MPEG4_XCurve2D:
		case TAG_MPEG4_Ellipse:
		case TAG_MPEG4_IndexedLineSet2D:
		case TAG_MPEG4_IndexedFaceSet2D:
		case TAG_MPEG4_Rectangle:
#ifndef GPAC_DISABLE_X3D
		case TAG_X3D_Disk2D:
		case TAG_X3D_Arc2D:
		case TAG_X3D_Polyline2D:
		case TAG_X3D_TriangleSet2D:
#endif
			dr = (Drawable *) gf_node_get_private( (GF_Node *) pl->geometry);
			break;
		default:
			break;
		}
		/*init iteration*/
		if (!dr || !dr->path) return;
		gr->iter = gf_path_iterator_new(dr->path);
		if (!gr->iter) return;
	}

	tr_state->parent = (ParentNode2D *) gr;
	int_bck = tr_state->text_split_mode;
	tr_state->text_split_mode = 2;
	mode_bckup = tr_state->traversing_mode;
	tr_state->traversing_mode = TRAVERSE_GET_BOUNDS;
	parent_node_traverse(node, (ParentNode2D *) gr, tr_state);
	tr_state->text_split_mode = int_bck;
	tr_state->traversing_mode = mode_bckup;

	/*restore traversing state*/
#ifndef GPAC_DISABLE_3D
	gf_mx_copy(tr_state->model_matrix, mat);
#endif
	tr_state->parent = parent_bck;
	gf_mx2d_copy(tr_state->transform, mx2d);

	count = gf_list_count(gr->groups);

	length = gf_path_iterator_get_length(gr->iter);
	/*place all children*/
	offset = gf_mulfix(length, pl->pathOffset);

	major = pl->alignment.count ? pl->alignment.vals[0] : 0;
	minor = (pl->alignment.count==2) ? pl->alignment.vals[1] : 0;

	if (pl->wrapMode==1) {
		while (offset<0) offset += length;
	}

	for (i=0; i<count; i++) {
		cg = (ChildGroup*)gf_list_get(gr->groups, i);
		if (cg->original.width>length) break;

		/*first set our center and baseline*/
		gf_mx2d_init(mx2d);

		/*major align*/
		switch (major) {
		case 2:
			if (cg->ascent) gf_mx2d_add_translation(&mx2d, -cg->original.x - cg->original.width, 0);
			else gf_mx2d_add_translation(&mx2d, -cg->original.width/2, 0);
			length_after_point = 0;
			break;
		case 1:
			length_after_point = cg->original.width/2;
			if (cg->ascent) gf_mx2d_add_translation(&mx2d, -cg->original.x - cg->original.width / 2, 0);
			break;
		default:
		case 0:
			if (cg->ascent) gf_mx2d_add_translation(&mx2d, cg->original.x, 0);
			else gf_mx2d_add_translation(&mx2d, cg->original.width/2, 0);
			length_after_point = cg->original.width;
			break;
		}

		/*if wrapping and out of path, restart*/
		if ((pl->wrapMode==1) && (offset+length_after_point>=length)) {
			offset += length_after_point;
			offset -= length;
			i--;
			continue;
		}
		/*if not wrapping and not yet in path skip */
		if (!pl->wrapMode && (offset+length_after_point < 0)) {
			parent_node_child_traverse_matrix(cg, (GF_TraverseState *)rs, NULL);
			goto next;
		}

		/*minor justify*/
		switch (minor) {
		/*top alignment*/
		case 3:
			if (cg->ascent)
				gf_mx2d_add_translation(&mx2d, 0, -1 * cg->ascent);
			else
				gf_mx2d_add_translation(&mx2d, 0, -1 * cg->original.height / 2);

			break;
		/*baseline*/
		case 1:
			/*move to bottom align if not text*/
			if (!cg->ascent)
				gf_mx2d_add_translation(&mx2d, 0, cg->original.height / 2);
			break;
		/*middle*/
		case 2:
			/*if text use (asc+desc) /2 as line height since glyph height differ*/
			if (cg->ascent)
				gf_mx2d_add_translation(&mx2d, 0, cg->descent - (cg->ascent + cg->descent) / 2);
			break;
		/*bottomline alignment*/
		case 0:
		default:
			if (cg->ascent)
				gf_mx2d_add_translation(&mx2d, 0, cg->descent);
			else
				gf_mx2d_add_translation(&mx2d, 0, cg->original.height / 2);

			break;
		}
		res = gf_path_iterator_get_transform(gr->iter, offset, (Bool) (pl->wrapMode==2), &mx2d, 1, length_after_point);
		if (!res) break;

		parent_node_child_traverse_matrix(cg, (GF_TraverseState *)rs, &mx2d);

next:
		if (i+1<count) {
			ChildGroup *cg_next = (ChildGroup*)gf_list_get(gr->groups, i+1);

			/*update offset according to major alignment */
			switch (major) {
			case 2:
				if (cg_next->ascent) offset += gf_mulfix(pl->spacing , cg_next->original.x);
				offset += gf_mulfix(pl->spacing , cg_next->original.width);
				break;
			case 1:
				if (cg->ascent) offset += gf_mulfix(pl->spacing, cg->original.x) / 2;
				offset += gf_mulfix(pl->spacing, cg->original.width) / 2;
				offset += cg_next->original.width / 2;
				break;
			default:
			case 0:
				if (cg->ascent) offset += gf_mulfix(pl->spacing, cg->original.x);
				offset += gf_mulfix(pl->spacing , cg->original.width);
				break;
			}
		}
		/*wrap*/
		if ((pl->wrapMode==1) && (offset>=length)) offset-=length;
	}

	/*undrawn nodes*/
	for (; i<count; i++) {
		cg = (ChildGroup*)gf_list_get(gr->groups, i);
		parent_node_child_traverse_matrix(cg, (GF_TraverseState *)rs, NULL);
	}

	parent_node_reset((ParentNode2D *) gr);
}

void compositor_init_path_layout(GF_Compositor *compositor, GF_Node *node)
{
	PathLayoutStack *stack;
	GF_SAFEALLOC(stack, PathLayoutStack);
	if (!stack) return;
	
	parent_node_setup((ParentNode2D*)stack);
	gf_node_set_private(node, stack);
	gf_node_set_callback_function(node, TraversePathLayout);
}

#endif //!defined(GPAC_DISABLE_VRML) && !defined(GPAC_DISABLE_COMPOSITOR)
