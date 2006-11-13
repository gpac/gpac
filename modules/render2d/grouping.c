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


#include "grouping.h"
#include "visualsurface2d.h"
#ifndef GPAC_DISABLE_SVG
#include "svg_stacks.h"
#endif

void child2d_compute_bounds(ChildGroup2D *cg)
{
	u32 i, count;
	Fixed a, d;
	if (cg->bounds_forced) return;
	cg->is_text_group = 1;
	cg->ascent = cg->descent = 0;
	gf_rect_reset(&cg->original);

	count = gf_list_count(cg->contexts);
	for (i=0; i<count; i++) {
		void text2D_get_ascent_descent(DrawableContext *ctx, Fixed *a, Fixed *d);
		DrawableContext *ctx = (DrawableContext *)gf_list_get(cg->contexts, i);
		gf_rect_union(&cg->original, &ctx->unclip);
		if (!cg->is_text_group) continue;
		if (!ctx->is_text) {
			cg->is_text_group = 0;
			continue;
		}
		text2D_get_ascent_descent(ctx, &a, &d);
		if (a>cg->ascent) cg->ascent = a;
		if (d>cg->descent) cg->descent = d;
	}
}

void group2d_force_bounds(GroupingNode2D *group, GF_Rect *clip)
{
	ChildGroup2D *cg;
	if (!group || !clip) return;
	cg = (ChildGroup2D *)gf_list_get(group->groups, gf_list_count(group->groups)-1);
	if (!cg) return;
	cg->ascent = cg->descent = 0;
	cg->is_text_group = 0;
	cg->original = *clip;
	cg->final = cg->original;
	cg->bounds_forced = 1;
}

void group2d_start_child(GroupingNode2D *group)
{
	ChildGroup2D *cg;
	GF_SAFEALLOC(cg, ChildGroup2D);
	cg->contexts = gf_list_new();
	gf_list_add(group->groups, cg);
}
void group2d_end_child(GroupingNode2D *group)
{
	ChildGroup2D *cg = (ChildGroup2D *)gf_list_get(group->groups, gf_list_count(group->groups)-1);
	if (!cg) return;
	child2d_compute_bounds(cg);
	cg->final = cg->original;
}

void group2d_reset_children(GroupingNode2D *group)
{
	while (gf_list_count(group->groups)) {
		ChildGroup2D *cg = (ChildGroup2D *)gf_list_get(group->groups, 0);
		gf_list_rem(group->groups, 0);
		gf_list_del(cg->contexts);
		free(cg);
	}
}

void group2d_add_to_context_list(GroupingNode2D *group, DrawableContext *ctx)
{
	ChildGroup2D *cg = (ChildGroup2D *)gf_list_get(group->groups, gf_list_count(group->groups)-1);
	if (!cg) return;
	gf_list_add(cg->contexts, ctx);
}
/*
	This is the generic routine for child traversing - note we are not duplicating the effect
*/
void group2d_traverse(GroupingNode2D *group, GF_List *children, RenderEffect2D *eff)
{
	u32 i, count;
	Bool split_text_backup;
	GF_Node *child;
	u32 count2;
	SensorHandler *hsens;
	GF_List *sensors_backup = NULL;
	count = gf_list_count(children);

	/*rebuild sensor list */
	if (gf_node_dirty_get(group->owner) & GF_SG_CHILD_DIRTY) {
		/*reset*/
		gf_list_reset(group->sensors);

		/*special case for anchor which is a parent node acting as a sensor*/
		if (gf_node_get_tag(group->owner)==TAG_MPEG4_Anchor) {
			SensorHandler *r2d_anchor_get_handler(GF_Node *n);
			hsens = r2d_anchor_get_handler(group->owner);
			if (hsens) gf_list_add(group->sensors, hsens);
		}
		for (i=0; i<count; i++) {
			child = (GF_Node*)gf_list_get(children, i);
			if (!child || !is_sensor_node(child) ) continue;
			hsens = get_sensor_handler(child);
			/*only keep track of locally enabled sensors*/
			if (hsens) gf_list_add(group->sensors, hsens);
		}
	}

	/*if we have an active sensor at this level discard all sensors in current render context (cf VRML)*/
	count2 = gf_list_count(group->sensors);
	if (count2) {
		sensors_backup = eff->sensors;
		eff->sensors = gf_list_new();
	
		/*add sensor(s) to effects*/	
		for (i=0; i <count2; i++) {
			SensorHandler *hsens = (SensorHandler *)gf_list_get(group->sensors, i);
			effect_add_sensor(eff, hsens, &eff->transform);
		}
	}

	gf_node_dirty_clear(group->owner, 0);

	if (eff->parent == group) {
		for (i=0; i<count; i++) {
			group2d_start_child(group);
			child = (GF_Node*)gf_list_get(children, i);
			gf_node_render(child, eff);
			group2d_end_child(group);
		}
	} else {
		split_text_backup = eff->text_split_mode;
		if (count>1) eff->text_split_mode = 0;
		for (i=0; i<count; i++) {
			child = (GF_Node*)gf_list_get(children, i);
			gf_node_render(child, eff);
		}
		eff->text_split_mode = split_text_backup;
	}
	if (count2) {
		/*destroy current effect list and restore previous*/
		effect_reset_sensors(eff);
		gf_list_del(eff->sensors);
		eff->sensors = sensors_backup;
	}
}

void gf_mx2d_apply_rect_int(GF_Matrix2D *mat, GF_IRect *rc)
{
	GF_Rect rcft = gf_rect_ft(rc);
	gf_mx2d_apply_rect(mat, &rcft);
	*rc = gf_rect_pixelize(&rcft);
}

void child2d_render_done(ChildGroup2D *cg, RenderEffect2D *eff, GF_Rect *par_clipper)
{
	GF_Matrix2D mat, loc_mx;
	Fixed x, y, inv_min_hsize;
	u32 i, count;
	u32 j, scount;
	GF_Rect _clip;
	GF_IRect clipper;

	_clip = *par_clipper;
	gf_mx2d_apply_rect(&eff->transform, &_clip);
	clipper = gf_rect_pixelize(&_clip);

	gf_mx2d_init(mat);
	gf_mx2d_add_translation(&mat, cg->final.x - cg->original.x, cg->final.y - cg->original.y);

	inv_min_hsize = gf_invfix(eff->min_hsize);

	count = gf_list_count(cg->contexts);
	for (i=0; i<count; i++) {
		DrawableContext *ctx = (DrawableContext *)gf_list_get(cg->contexts, i);

		gf_mx2d_apply_coords(&mat, &ctx->unclip.x, &ctx->unclip.y);
		x = INT2FIX(ctx->clip.x); y = INT2FIX(ctx->clip.y);
		gf_mx2d_apply_coords(&mat, &x, &y);
		ctx->clip.x = FIX2INT(gf_floor(x));
		ctx->clip.y = FIX2INT(gf_ceil(y));

		gf_mx2d_add_matrix(&ctx->transform, &mat);
		if (!eff->is_pixel_metrics) gf_mx2d_add_scale(&ctx->transform, inv_min_hsize, inv_min_hsize);
		gf_mx2d_add_matrix(&ctx->transform, &eff->transform);

		scount = gf_list_count(ctx->sensors);
		for (j=0; j<scount; j++) {
			SensorContext *sc = (SensorContext *)gf_list_get(ctx->sensors, j);

			if (!eff->is_pixel_metrics) gf_mx2d_add_scale(&sc->matrix, inv_min_hsize, inv_min_hsize);
			gf_mx2d_add_matrix(&sc->matrix, &eff->transform);
		}
		gf_mx2d_init(loc_mx);
		if (!eff->is_pixel_metrics) gf_mx2d_add_scale(&loc_mx, inv_min_hsize, inv_min_hsize);
		gf_mx2d_add_matrix(&loc_mx, &eff->transform);

		gf_mx2d_apply_rect(&loc_mx, &ctx->unclip);
		ctx->unclip_pix = gf_rect_pixelize(&ctx->unclip);

		gf_mx2d_apply_rect_int(&loc_mx, &ctx->clip);
		gf_irect_intersect(&ctx->clip, &clipper);

		drawable_finalize_end(ctx, eff);
	}
}
void child2d_render_done_complex(ChildGroup2D *cg, RenderEffect2D *eff, GF_Matrix2D *mat)
{
	u32 i, count;
	u32 j, scount;
	
	count = gf_list_count(cg->contexts);
	for (i=0; i<count; i++) {
		DrawableContext *ctx = (DrawableContext *)gf_list_get(cg->contexts, i);
		if (!mat) {
			gf_rect_reset(&ctx->clip);
			gf_rect_reset(&ctx->unclip);
			continue;
		}
		gf_mx2d_add_matrix(&ctx->transform, mat);
		gf_mx2d_add_matrix(&ctx->transform, &eff->transform);

		scount = gf_list_count(ctx->sensors);
		for (j=0; j<scount; j++) {
			SensorContext *sc = (SensorContext *)gf_list_get(ctx->sensors, j);
			gf_mx2d_add_matrix(&sc->matrix, &eff->transform);
		}
		gf_mx2d_apply_rect(&ctx->transform, &ctx->unclip);
		ctx->unclip_pix = gf_rect_pixelize(&ctx->unclip);
		gf_mx2d_apply_rect_int(&ctx->transform, &ctx->clip);

		drawable_finalize_end(ctx, eff);
	}
}


void SetupGroupingNode2D(GroupingNode2D *group, Render2D *sr, GF_Node *node)
{
	memset(group, 0, sizeof(GroupingNode2D));
	gf_sr_traversable_setup(group, node, sr->compositor);
	group->sensors = gf_list_new();
	group->groups = gf_list_new();
}

void DeleteGroupingNode2D(GroupingNode2D *group)
{
	/*just in case*/
	group2d_reset_children(group);
	gf_list_del(group->sensors);
	group2d_reset_children(group);
	gf_list_del(group->groups);
}

void DestroyBaseGrouping2D(GF_Node *node)
{
	GroupingNode2D *group = (GroupingNode2D *)gf_node_get_private(node);
	DeleteGroupingNode2D(group);
	free(group);
}

