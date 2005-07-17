/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / 2D rendering module
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

#include "stacks2d.h"
#include "visualsurface2d.h"

typedef struct
{
	GF_Node *owner;
	GF_Renderer *compositor;
	GROUPINGNODESTACK2D
	
	GF_Node *last_geom;
	GF_PathIterator *iter;
} PathLayoutStack;

static void DestroyPathLayout(GF_Node *node)
{
	PathLayoutStack *st = (PathLayoutStack *)gf_node_get_private(node);
	DeleteGroupingNode2D((GroupingNode2D *)st);
	if (st->iter) gf_path_iterator_del(st->iter);
	free(st);
}

static void RenderPathLayout(GF_Node *node, void *rs)
{
	u32 i, count, minor, major, int_bck;
	Fixed length, offset, length_after_point;
	Bool res;
	ChildGroup2D *cg;
	GF_Matrix2D mat;
	GroupingNode2D *parent_bck;
	PathLayoutStack *gr = (PathLayoutStack *) gf_node_get_private(node);
	M_PathLayout *pl = (M_PathLayout *)node;
	RenderEffect2D *eff = (RenderEffect2D *) rs;
	
	if (!pl->geometry) return;
	
	/*only low-level primitives allowed*/
	switch (gf_node_get_tag((GF_Node *) pl->geometry)) {
	case TAG_MPEG4_Rectangle: return;
	case TAG_MPEG4_Circle: return;
	case TAG_MPEG4_Ellipse: return;
	}

	/*store effect*/
	gf_mx2d_copy(mat, eff->transform);
	parent_bck = eff->parent;

	gf_mx2d_init(eff->transform);
	eff->parent = NULL;

	/*check geom changes*/
	if ((pl->geometry != gr->last_geom) || gf_node_dirty_get(pl->geometry)) {
		if (gr->iter) gf_path_iterator_del(gr->iter);
		gr->iter = NULL;

		int_bck = eff->trav_flags;
		eff->trav_flags |= GF_SR_TRAV_SWITCHED_OFF;
		gf_node_render(pl->geometry, eff);
		eff->trav_flags = int_bck;

		gr->last_geom = pl->geometry;
	}

	if (!gr->iter) {
		/*get the drawable*/
		Drawable *dr = (Drawable *) gf_node_get_private( (GF_Node *) pl->geometry);
		/*init iteration*/
		if (!dr || !dr->path) return;
		gr->iter = gf_path_iterator_new(dr->path);
		if (!gr->iter) return;
	}
	
	eff->parent = (GroupingNode2D*)gr;
	int_bck = eff->text_split_mode;
	eff->text_split_mode = 2;
	group2d_traverse((GroupingNode2D*)gr, pl->children, eff);
	eff->text_split_mode = int_bck;

	/*restore effect*/
	gf_mx2d_copy(eff->transform, mat);
	eff->parent = parent_bck;

	count = gf_list_count(gr->groups);
	
	length = gf_path_iterator_get_length(gr->iter);
	offset = gf_mulfix(length, pl->pathOffset);

	major = pl->alignment.count ? pl->alignment.vals[0] : 0;
	minor = (pl->alignment.count==2) ? pl->alignment.vals[1] : 0;

	if (pl->wrapMode==1) {
		while (offset<0) offset += length;
	}

	for (i=0; i<count; i++) {
		cg = gf_list_get(gr->groups, i);
		if (cg->original.width>length) break;

		/*first set our center and baseline*/
		gf_mx2d_init(mat);

		/*major align*/
		switch (major) {
		case 2:
			if (cg->is_text_group) gf_mx2d_add_translation(&mat, -1*cg->original.x - cg->original.width, 0);
			else gf_mx2d_add_translation(&mat, -1 * cg->original.width/2, 0);
			length_after_point = 0;
			break;
		case 1:
			length_after_point = cg->original.width/2;
			if (cg->is_text_group) gf_mx2d_add_translation(&mat, -1*cg->original.x - cg->original.width / 2, 0);
			break;
		default:
		case 0:
			if (cg->is_text_group) gf_mx2d_add_translation(&mat, cg->original.x, 0);
			else gf_mx2d_add_translation(&mat, cg->original.width/2, 0);
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
			child2d_render_done_complex(cg, (RenderEffect2D *)rs, NULL);
			goto next;
		}

		/*minor justify*/
		switch (minor) {
		/*top alignment*/
		case 3:
			if (cg->is_text_group) 
				gf_mx2d_add_translation(&mat, 0, -1 * cg->ascent);
			else 
				gf_mx2d_add_translation(&mat, 0, -1 * cg->original.height / 2);
			
			break;
		/*baseline*/
		case 1:
			/*move to bottom align if not text*/
			if (!cg->is_text_group) 
				gf_mx2d_add_translation(&mat, 0, cg->original.height / 2);
			break;
		/*middle*/
		case 2:
			/*if text use (asc+desc) /2 as line height since glyph height differ*/
			if (cg->is_text_group) 
				gf_mx2d_add_translation(&mat, 0, cg->descent - (cg->ascent + cg->descent) / 2);
			break;
		/*bottomline alignment*/
		case 0:
		default:
			if (cg->is_text_group)
				gf_mx2d_add_translation(&mat, 0, cg->descent);
			else
				gf_mx2d_add_translation(&mat, 0, cg->original.height / 2);
			
			break;
		}
		res = gf_path_iterator_get_transform(gr->iter, offset, (Bool) (pl->wrapMode==2), &mat, 1, length_after_point);
		if (!res) break;

		child2d_render_done_complex(cg, (RenderEffect2D *)rs, &mat);

next:
		if (i+1<count) {
			ChildGroup2D *cg_next = gf_list_get(gr->groups, i+1);

			/*update offset according to major alignment */
			switch (major) {
			case 2:
				if (cg_next->is_text_group) offset += gf_mulfix(pl->spacing, cg_next->original.x);
				offset += gf_mulfix(pl->spacing, cg_next->original.width);
				break;
			case 1:
				if (cg->is_text_group) offset += gf_mulfix(pl->spacing, cg->original.x / 2);
				offset += gf_mulfix(pl->spacing, cg->original.width / 2);
				offset += cg_next->original.width / 2;
				break;
			default:
			case 0:
				if (cg->is_text_group) offset += gf_mulfix(pl->spacing, cg->original.x);
				offset += gf_mulfix(pl->spacing, cg->original.width);
				break;
			}
		}
		/*wrap*/
		if ((pl->wrapMode==1) && (offset>=length)) offset-=length;
	}

	/*undrawn nodes*/
	for (;i<count; i++) {
		cg = gf_list_get(gr->groups, i);
		child2d_render_done_complex(cg, (RenderEffect2D *)rs, NULL);
	}
	group2d_reset_children((GroupingNode2D *) gr);
}

void R2D_InitPathLayout(Render2D *sr, GF_Node *node)
{
	PathLayoutStack *stack;
	GF_SAFEALLOC(stack, sizeof(PathLayoutStack));
	SetupGroupingNode2D((GroupingNode2D*)stack, sr, node);

	gf_node_set_private(node, stack);
	gf_node_set_predestroy_function(node, DestroyPathLayout);
	gf_node_set_render_function(node, RenderPathLayout);
}

