/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Cyril Concolato - Jean le Feuvre
 *				Copyright (c) 2005-200X ENST
 *					All rights reserved
 *
 *  This file is part of GPAC / SVG Rendering sub-project
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

#include "visualsurface2d.h"

#ifndef GPAC_DISABLE_SVG
#include "svg_stacks.h"

#include <gpac/internal/scenegraph_dev.h>

static void SVG_Draw_bitmap(RenderEffect2D *eff)
{
	u8 alpha;
	Render2D *sr;
	Bool use_blit;
	DrawableContext *ctx = eff->ctx;
	sr = eff->surface->render;

	use_blit = 1;
	alpha = GF_COL_A(ctx->aspect.fill_color);

	/*this is not a native texture, use graphics*/
	if (!ctx->h_texture->data || ctx->transform.m[1] || ctx->transform.m[3]) {
		use_blit = 0;
	} else {
		if (!eff->surface->SupportsFormat || !eff->surface->DrawBitmap ) use_blit = 0;
		/*format not supported directly, try with brush*/
		else if (!eff->surface->SupportsFormat(eff->surface, ctx->h_texture->pixelformat) ) use_blit = 0;
	}

	/*no HW, fall back to the graphics driver*/
	if (!use_blit) {
		VS2D_TexturePath(eff->surface, ctx->drawable->path, ctx);
		return;
	}

	/*direct rendering, render without clippers */
	if (eff->surface->render->top_effect->trav_flags & TF_RENDER_DIRECT) {
		eff->surface->DrawBitmap(eff->surface, ctx->h_texture, &ctx->bi->clip, &ctx->bi->unclip, alpha, NULL, ctx->col_mat);
	}
	/*render bitmap for all dirty rects*/
	else {
		u32 i;
		GF_IRect clip;
		for (i=0; i<eff->surface->to_redraw.count; i++) {
			/*there's an opaque region above, don't draw*/
#ifdef TRACK_OPAQUE_REGIONS
			if (eff->surface->draw_node_index < eff->surface->to_redraw.opaque_node_index[i]) continue;
#endif
			clip = ctx->bi->clip;
			gf_irect_intersect(&clip, &eff->surface->to_redraw.list[i]);
			if (clip.width && clip.height) {
				eff->surface->DrawBitmap(eff->surface, ctx->h_texture, &clip, &ctx->bi->unclip, alpha, NULL, ctx->col_mat);
			}
		}
	}
}

static void SVG_Build_Bitmap_Graph(SVG_image_stack *st)
{
	GF_Rect rc, new_rc;
	Fixed x, y, width, height;
	u32 tag = gf_node_get_tag(st->graph->node);
	switch (tag) {
#ifdef GPAC_ENABLE_SVG_SA
	case TAG_SVG_SA_image:
		{
			SVG_SA_imageElement *img = (SVG_SA_imageElement *)st->graph->node;
			x = img->x.value;
			y = img->y.value;
			width = img->width.value;
			height = img->height.value;
		}
		break;
	case TAG_SVG_SA_video:
		{
			SVG_SA_videoElement *video = (SVG_SA_videoElement *)st->graph->node;
			x = video->x.value;
			y = video->y.value;
			width = video->width.value;
			height = video->height.value;
		}
		break;
#endif
#ifdef GPAC_ENABLE_SVG_SANI
	case TAG_SVG_SANI_image:
		{
			SVG_SANI_imageElement *img = (SVG_SANI_imageElement *)st->graph->node;
			x = img->x.value;
			y = img->y.value;
			width = img->width.value;
			height = img->height.value;
		}
		break;
	case TAG_SVG_SANI_video:
		{
			SVG_SANI_videoElement *video = (SVG_SANI_videoElement *)st->graph->node;
			x = video->x.value;
			y = video->y.value;
			width = video->width.value;
			height = video->height.value;
		}
		break;
#endif
	case TAG_SVG_image:
	case TAG_SVG_video:
		{
			SVG_Element *e = (SVG_Element *)st->graph->node;
			SVGAllAttributes atts;
			gf_svg_flatten_attributes(e, &atts);
			x = (atts.x ? atts.x->value : 0);
			y = (atts.y ? atts.y->value : 0);
			width = (atts.width ? atts.width->value : 0);
			height = (atts.height ? atts.height->value : 0);
		}
		break;
	default:
		return;
	}

	if (!width) width = INT2FIX(st->txh.width);
	if (!height) height = INT2FIX(st->txh.height);

	gf_path_get_bounds(st->graph->path, &rc);
	drawable_reset_path(st->graph);
	gf_path_add_rect_center(st->graph->path, x+width/2, y+height/2, width, height);
	gf_path_get_bounds(st->graph->path, &new_rc);
	if (!gf_rect_equal(rc, new_rc)) st->graph->flags |= DRAWABLE_HAS_CHANGED;
	gf_node_dirty_clear(st->graph->node, GF_SG_SVG_GEOMETRY_DIRTY);
}

#ifdef GPAC_ENABLE_SVG_SA

static void svg_sa_render_bitmap(GF_Node *node, void *rs)
{
	/*video stack is just an extension of image stack, type-casting is OK*/
	SVG_image_stack *st = (SVG_image_stack*)gf_node_get_private(node);
	RenderEffect2D *eff = (RenderEffect2D *)rs;
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	GF_Matrix2D backup_matrix;
	GF_Matrix2D *m;
	DrawableContext *ctx;

	if (eff->traversing_mode==TRAVERSE_DRAW) {
		SVG_Draw_bitmap(eff);
		return;
	}
	else if (eff->traversing_mode==TRAVERSE_PICK) {
		eff->is_over = 1;
		return;
	}

	svg_sa_render_base(node, eff, &backup_props, &backup_flags);

	if (gf_node_dirty_get(node)) {
		SVG_Build_Bitmap_Graph((SVG_image_stack*)gf_node_get_private(node));
		/*if open and changed, stop and play*/
		if (gf_term_check_iri_change(st->txh.compositor->term, &st->txurl, & ((SVG_SA_Element *)node)->xlink->href)) {
			const char *cache_dir = gf_cfg_get_key(st->txh.compositor->user->config, "General", "CacheDirectory");
			gf_svg_store_embedded_data(& ((SVG_SA_Element *)node)->xlink->href, cache_dir, "embedded_");

			if (gf_term_check_iri_change(st->txh.compositor->term, &st->txurl, & ((SVG_SA_Element *)node)->xlink->href)) {
				gf_term_set_mfurl_from_uri(st->txh.compositor->term, &(st->txurl), & ((SVG_SA_Element*)node)->xlink->href);
				if (st->txh.is_open) gf_sr_texture_stop(&st->txh);
				gf_sr_texture_play_from(&st->txh, &st->txurl, 0, 0, 0, NULL);
			}
		} 
		gf_node_dirty_clear(node, GF_SG_NODE_DIRTY);
	} 
	
	switch (gf_node_get_tag(node)) {
#ifdef GPAC_ENABLE_SVG_SA
	case TAG_SVG_SA_image:
		m = &((SVG_SA_imageElement *)node)->transform.mat;
		break;
	case TAG_SVG_SA_video:
		m = &((SVG_SA_videoElement *)node)->transform.mat;
		break;
#endif
#ifdef GPAC_ENABLE_SVG_SANI
	case TAG_SVG_SANI_image:
		m = &((SVG_SA_imageElement *)node)->transform.mat;
		break;
	case TAG_SVG_SANI_video:
		m = &((SVG_SA_videoElement *)node)->transform.mat;
		break;
#endif
	default:
		/*SVG FIXME*/
		m = NULL;
	}

	/*FIXME: setup aspect ratio*/

	if (eff->traversing_mode == TRAVERSE_GET_BOUNDS) {
		svg_sa_apply_local_transformation(eff, node, &backup_matrix);
		if (!svg_is_display_off(eff->svg_props)) {
			gf_path_get_bounds(st->graph->path, &eff->bounds);
		}
		memcpy(eff->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
		svg_sa_restore_parent_transformation(eff, &backup_matrix);  
		eff->svg_flags = backup_flags;
		return;
	}

	if (svg_is_display_off(eff->svg_props) ||
		*(eff->svg_props->visibility) == SVG_VISIBILITY_HIDDEN) {
		memcpy(eff->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
		eff->svg_flags = backup_flags;
		return;
	}

	svg_sa_apply_local_transformation(eff, node, &backup_matrix);

	ctx = SVG_drawable_init_context(st->graph, eff);
	if (!ctx || !ctx->h_texture ) return;

	/*even if set this is not true*/
	ctx->aspect.pen_props.width = 0;
	ctx->flags |= CTX_NO_ANTIALIAS;

	/*if rotation, transparent*/
	ctx->flags &= ~CTX_IS_TRANSPARENT;
	if (ctx->transform.m[1] || ctx->transform.m[3]) {
		ctx->flags |= CTX_IS_TRANSPARENT;
		ctx->flags &= ~CTX_NO_ANTIALIAS;
	}
	else if (ctx->h_texture->transparent) 
		ctx->flags |= CTX_IS_TRANSPARENT;
	else if (eff->svg_props->opacity && (eff->svg_props->opacity->type==SVG_NUMBER_VALUE) && (eff->svg_props->opacity->value!=FIX_ONE)) {
		ctx->flags = CTX_IS_TRANSPARENT;
		ctx->aspect.fill_color = GF_COL_ARGB(FIX2INT(0xFF * eff->svg_props->opacity->value), 0, 0, 0);
	}

	/*bounds are stored when building graph*/	
	drawable_finalize_render(ctx, eff, NULL);
	svg_sa_restore_parent_transformation(eff, &backup_matrix);  
	memcpy(eff->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
	eff->svg_flags = backup_flags;
}
#endif

static void svg_play_texture(SVG_image_stack *st, SVGAllAttributes *atts)
{
	SVGAllAttributes all_atts;
	Bool lock_scene = 0;
	if (st->txh.is_open) gf_sr_texture_stop(&st->txh);

	if (!atts) {
		gf_svg_flatten_attributes((SVG_Element*)st->txh.owner, &all_atts);
		atts = &all_atts;
	}
	if (atts->syncBehavior) lock_scene = (*atts->syncBehavior == SMIL_SYNCBEHAVIOR_LOCKED) ? 1 : 0;
	gf_sr_texture_play_from(&st->txh, &st->txurl, 
		atts->clipBegin ? (*atts->clipBegin) : 0.0,
		0, 
		lock_scene,
		NULL);
}

static void svg_render_bitmap(GF_Node *node, void *rs)
{
	/*video stack is just an extension of image stack, type-casting is OK*/
	SVG_image_stack *st = (SVG_image_stack*)gf_node_get_private(node);
	RenderEffect2D *eff = (RenderEffect2D *)rs;
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	GF_Matrix2D backup_matrix;
	DrawableContext *ctx;
	SVGAllAttributes all_atts;

	if (eff->traversing_mode==TRAVERSE_DRAW) {
		SVG_Draw_bitmap(eff);
		return;
	}
	else if (eff->traversing_mode==TRAVERSE_PICK) {
		drawable_pick(eff);
		return;
	}


	gf_svg_flatten_attributes((SVG_Element *)node, &all_atts);

	svg_render_base(node, &all_atts, (RenderEffect2D *)rs, &backup_props, &backup_flags);

	if (gf_node_dirty_get(node)) {
		GF_FieldInfo href_info;
		SVG_Build_Bitmap_Graph((SVG_image_stack*)gf_node_get_private(node));
		/*if open and changed, stop and play*/
		if (gf_svg_get_attribute_by_tag(node, TAG_SVG_ATT_xlink_href, 0, 0, &href_info) == GF_OK) {
			if (gf_term_check_iri_change(st->txh.compositor->term, &st->txurl, href_info.far_ptr)) {
				const char *cache_dir = gf_cfg_get_key(st->txh.compositor->user->config, "General", "CacheDirectory");
				gf_svg_store_embedded_data(href_info.far_ptr, cache_dir, "embedded_");

				if (gf_term_check_iri_change(st->txh.compositor->term, &st->txurl, href_info.far_ptr) == GF_OK) {
					gf_term_set_mfurl_from_uri(st->txh.compositor->term, &(st->txurl), href_info.far_ptr);
					svg_play_texture(st, &all_atts);
				}
			} 
		}
		gf_node_dirty_clear(node, GF_SG_NODE_DIRTY);
	} 
	
	/*FIXME: setup aspect ratio*/
	if (eff->traversing_mode == TRAVERSE_GET_BOUNDS) {
		svg_apply_local_transformation(eff, &all_atts, &backup_matrix);
		if (!svg_is_display_off(eff->svg_props)) {
			gf_path_get_bounds(st->graph->path, &eff->bounds);
		}
		svg_restore_parent_transformation(eff, &backup_matrix);
		memcpy(eff->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
		eff->svg_flags = backup_flags;
		return;
	}

	if (svg_is_display_off(eff->svg_props) ||
		*(eff->svg_props->visibility) == SVG_VISIBILITY_HIDDEN) {
		memcpy(eff->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
		eff->svg_flags = backup_flags;
		return;
	}

	svg_apply_local_transformation(eff, &all_atts, &backup_matrix);

	ctx = SVG_drawable_init_context(st->graph, eff);
	if (!ctx || !ctx->h_texture ) return;

	/*even if set this is not true*/
	ctx->aspect.pen_props.width = 0;
	ctx->flags |= CTX_NO_ANTIALIAS;

	/*if rotation, transparent*/
	ctx->flags &= ~CTX_IS_TRANSPARENT;
	if (ctx->transform.m[1] || ctx->transform.m[3]) {
		ctx->flags |= CTX_IS_TRANSPARENT;
		ctx->flags &= ~CTX_NO_ANTIALIAS;
	}
	else if (ctx->h_texture->transparent) 
		ctx->flags |= CTX_IS_TRANSPARENT;
	else if (eff->svg_props->opacity && (eff->svg_props->opacity->type==SVG_NUMBER_VALUE) && (eff->svg_props->opacity->value!=FIX_ONE)) {
		ctx->flags = CTX_IS_TRANSPARENT;
		ctx->aspect.fill_color = GF_COL_ARGB(FIX2INT(0xFF * eff->svg_props->opacity->value), 0, 0, 0);
	}

	/*bounds are stored when building graph*/	
	drawable_finalize_render(ctx, eff, NULL);
	svg_restore_parent_transformation(eff, &backup_matrix);
	memcpy(eff->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
	eff->svg_flags = backup_flags;
}

/*********************/
/* SVG image element */
/*********************/

static void SVG_Update_image(GF_TextureHandler *txh)
{	
	MFURL *txurl = &(((SVG_image_stack *)gf_node_get_private(txh->owner))->txurl);

	/*setup texture if needed*/
	if (!txh->is_open && txurl->count) {
		gf_sr_texture_play_from(txh, txurl, 0, 0, 0, NULL);
	}
	gf_sr_texture_update_frame(txh, 0);
	/*URL is present but not opened - redraw till fetch*/
	if (txh->stream && !txh->hwtx) gf_sr_invalidate(txh->compositor, NULL);
}

static void SVG_Render_image(GF_Node *node, void *rs, Bool is_destroy)
{
	if (is_destroy) {
		SVG_image_stack *st = (SVG_image_stack *)gf_node_get_private(node);
		gf_sr_texture_destroy(&st->txh);
		gf_sg_mfurl_del(st->txurl);
		drawable_del(st->graph);
		free(st);
	} else {
		switch (gf_node_get_tag(node)) {
#ifdef GPAC_ENABLE_SVG_SA
		case TAG_SVG_SA_image:
			svg_sa_render_bitmap(node, rs);
			break;
#endif
#ifdef GPAC_ENABLE_SVG_SANI
		case TAG_SVG_SANI_image:
			svg_sa_render_bitmap(node, rs);
			break;
#endif
		case TAG_SVG_image:
			svg_render_bitmap(node, rs);
			break;
		}
	}
}

void svg_init_image(Render2D *sr, GF_Node *node)
{
	u32 tag;
	SVG_image_stack *st;
	GF_SAFEALLOC(st, SVG_image_stack)
	st->graph = drawable_new();

	st->graph->node = node;

	gf_sr_texture_setup(&st->txh, sr->compositor, node);
	st->txh.update_texture_fcnt = SVG_Update_image;
	st->txh.flags = 0;

	tag = gf_node_get_tag(node);
	/* builds the MFURL to be used by the texture */
	switch (tag) {
#ifdef GPAC_ENABLE_SVG_SA
	case TAG_SVG_SA_image:
		gf_term_set_mfurl_from_uri(sr->compositor->term, &(st->txurl), & ((SVG_SA_imageElement*)node)->xlink->href);
		break;
#endif
#ifdef GPAC_ENABLE_SVG_SANI
	case TAG_SVG_SANI_image:
		gf_term_set_mfurl_from_uri(sr->compositor->term, &(st->txurl), & ((SVG_SA_imageElement*)node)->xlink->href);
		break;
#endif
	case TAG_SVG_image:
	{
		GF_FieldInfo href_info;
		if (gf_svg_get_attribute_by_tag(node, TAG_SVG_ATT_xlink_href, 0, 0, &href_info) == GF_OK) {
			gf_term_set_mfurl_from_uri(sr->compositor->term, &(st->txurl), href_info.far_ptr);
		}
	}
		break;
	}

	gf_node_set_private(node, st);
	gf_node_set_callback_function(node, SVG_Render_image);
}

/*********************/
/* SVG video element */
/*********************/
static void SVG_Update_video(GF_TextureHandler *txh)
{
	SVG_video_stack *st = (SVG_video_stack *) gf_node_get_private(txh->owner);
	
	if (!txh->is_open) {
		u32 tag = gf_node_get_tag(txh->owner);
		SVG_InitialVisibility init_vis = SVG_INITIALVISIBILTY_WHENSTARTED;

		switch (tag) {
#ifdef GPAC_ENABLE_SVG_SA
		case TAG_SVG_SA_video:
			init_vis = ((SVG_SA_videoElement *)txh->owner)->initialVisibility;
			break;
#endif
#ifdef GPAC_ENABLE_SVG_SANI
		case TAG_SVG_SANI_video:
			init_vis = ((SVG_SANI_videoElement *)txh->owner)->initialVisibility;
			break;
#endif
		case TAG_SVG_video:
		{
			GF_FieldInfo init_vis_info;
			if (gf_svg_get_attribute_by_tag(txh->owner, TAG_SVG_ATT_initialVisibility, 0, 0, &init_vis_info) == GF_OK) {
				init_vis = *(SVG_InitialVisibility *)init_vis_info.far_ptr;
			}
		}
			break;
		}

		/*opens stream only at first access to fetch first frame if needed*/
		if (!st->first_frame_fetched && (init_vis == SVG_INITIALVISIBILTY_ALWAYS)) {
			//gf_sr_texture_play_from(txh, &st->txurl, 0, 0, 0, NULL);
			svg_play_texture((SVG_image_stack*)st, NULL);
			gf_sr_invalidate(txh->compositor, NULL);
			return;
		} else {
			return;
		}
	} 

	/*when fetching the first frame disable resync*/
	gf_sr_texture_update_frame(txh, 0);

	/* only when needs_refresh = 1, first frame is fetched */
	if (!st->first_frame_fetched && (txh->needs_refresh) ) {
		st->first_frame_fetched = 1;
		/*stop stream if needed*/
		if (!gf_smil_timing_is_active(txh->owner)) {
			gf_sr_texture_stop(txh);
			//make sure the refresh flag is not cleared
			txh->needs_refresh = 1;
		}
	}
	/*we have no choice but retraversing the graph until we're inactive since the movie framerate and
	the renderer framerate are likely to be different */
	if (!txh->stream_finished) 
		gf_sr_invalidate(txh->compositor, NULL);
}

static void svg_video_smil_evaluate(SMIL_Timing_RTI *rti, Fixed normalized_scene_time, u32 status)
{
	SVG_video_stack *stack = (SVG_video_stack *)gf_node_get_private((GF_Node *)rti->timed_elt);

	switch (status) {
	case SMIL_TIMING_EVAL_UPDATE:
		if (!stack->txh.is_open) { 
			svg_play_texture((SVG_image_stack*)stack, NULL);
		}
		else if (stack->txh.stream_finished && (rti->media_duration<0) ) { 
			Double dur = gf_mo_get_duration(stack->txh.stream);
			if (dur <= 0) {
				dur = stack->txh.last_frame_time;
				dur /= 1000;
			}
			gf_smil_set_media_duration(rti, dur);
		}
		break;
	case SMIL_TIMING_EVAL_FREEZE:
		gf_sr_texture_stop(&stack->txh);
		break;
	case SMIL_TIMING_EVAL_REMOVE:
		gf_sr_texture_stop(&stack->txh);
		stack->txh.stream = NULL;
		gf_sr_invalidate(stack->txh.compositor, NULL);
		break;
	case SMIL_TIMING_EVAL_REPEAT:
		gf_sr_texture_restart(&stack->txh);
		break;
	}
}

static void SVG_Render_video(GF_Node *node, void *rs, Bool is_destroy)
{
	if (is_destroy) {
		SVG_video_stack *st = (SVG_video_stack *)gf_node_get_private(node);
		gf_sr_texture_destroy(&st->txh);
		gf_sg_mfurl_del(st->txurl);

		drawable_del(st->graph);
		free(st);
	} else {
		switch (gf_node_get_tag(node)) {
#ifdef GPAC_ENABLE_SVG_SA
		case TAG_SVG_SA_video:
			svg_sa_render_bitmap(node, rs);
			break;
#endif
#ifdef GPAC_ENABLE_SVG_SANI
		case TAG_SVG_SANI_video:
			svg_sa_render_bitmap(node, rs);
			break;
#endif
		case TAG_SVG_video:
			svg_render_bitmap(node, rs);
			break;
		}
	}
}

void svg_init_video(Render2D *sr, GF_Node *node)
{
	u32 tag;
	SVG_video_stack *st;
	GF_SAFEALLOC(st, SVG_video_stack)
	st->graph = drawable_new();

	st->graph->node = node;

	gf_sr_texture_setup(&st->txh, sr->compositor, node);
	st->txh.update_texture_fcnt = SVG_Update_video;
	st->txh.flags = 0;

	/* create an MFURL from the SVG iri */
	tag = gf_node_get_tag(node);
	switch (tag) {
#ifdef GPAC_ENABLE_SVG_SA_BASE
#ifdef GPAC_ENABLE_SVG_SA
	case TAG_SVG_SA_video:
#endif
#ifdef GPAC_ENABLE_SVG_SANI
	case TAG_SVG_SANI_video:
#endif
		gf_term_set_mfurl_from_uri(sr->compositor->term, &(st->txurl), & ((SVG_SA_videoElement *)node)->xlink->href);
		gf_smil_timing_init_runtime_info(node);
		if (((SVG_SA_Element *)node)->timing->runtime) {
			SMIL_Timing_RTI *rti = ((SVG_SA_Element *)node)->timing->runtime;
			rti->evaluate = svg_video_smil_evaluate;
		}
		break;
#endif

	case TAG_SVG_video:
	{
		GF_FieldInfo href_info;
		if (gf_svg_get_attribute_by_tag(node, TAG_SVG_ATT_xlink_href, 0, 0, &href_info) == GF_OK) {
			gf_term_set_mfurl_from_uri(sr->compositor->term, &(st->txurl), href_info.far_ptr);
		}
		if (((SVGTimedAnimBaseElement *)node)->timingp->runtime) {
			SMIL_Timing_RTI *rti = ((SVGTimedAnimBaseElement *)node)->timingp->runtime;
			rti->evaluate = svg_video_smil_evaluate;
		}
	}	
		break;
	}

	gf_node_set_private(node, st);
	gf_node_set_callback_function(node, SVG_Render_video);
}

/*********************/
/* SVG audio element */
/*********************/

static void svg_audio_smil_evaluate(SMIL_Timing_RTI *rti, Fixed normalized_scene_time, u32 status)
{
	SVG_audio_stack *stack = (SVG_audio_stack *)gf_node_get_private((GF_Node *)rti->timed_elt);
	
	switch (status) {
	case SMIL_TIMING_EVAL_UPDATE:
		if (!stack->is_active) { 
			if (gf_sr_audio_open(&stack->input, &stack->aurl) == GF_OK) {
				gf_mo_set_speed(stack->input.stream, FIX_ONE);
				stack->is_active = 1;
			}
		}
		else if (stack->input.stream_finished && (rti->media_duration < 0) ) { 
			Double dur = gf_mo_get_duration(stack->input.stream);
			if (dur <= 0) {
				dur = gf_mo_get_last_frame_time(stack->input.stream);
				dur /= 1000;
			}
			gf_smil_set_media_duration(rti, dur);
		}
		break;
	case SMIL_TIMING_EVAL_REPEAT:
		if (stack->is_active) 
			gf_sr_audio_restart(&stack->input);
		break;
	case SMIL_TIMING_EVAL_FREEZE:
		gf_sr_audio_stop(&stack->input);
		stack->is_active = 0;
		break;
	case SMIL_TIMING_EVAL_REMOVE:
		gf_sr_audio_stop(&stack->input);
		stack->is_active = 0;
		break;
	case SMIL_TIMING_EVAL_DEACTIVATE:
		if (stack->is_active) {
			gf_sr_audio_stop(&stack->input);
			gf_sr_audio_unregister(&stack->input);
			stack->is_active = 0;
		}
		break;
	}
}

static void SVG_Render_audio(GF_Node *node, void *rs, Bool is_destroy)
{
	u32 tag;
	SVGAllAttributes all_atts;
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	RenderEffect2D *eff = (RenderEffect2D*)rs;
	SVG_audio_stack *st = (SVG_audio_stack *)gf_node_get_private(node);

	if (is_destroy) {
		gf_sr_audio_stop(&st->input);
		gf_sr_audio_unregister(&st->input);
		gf_sg_mfurl_del(st->aurl);
		free(st);
		return;
	}
	if (st->is_active) {
		gf_sr_audio_register(&st->input, (GF_BaseEffect*)rs);
	}

	tag = gf_node_get_tag(node);
	switch (tag) {
#ifdef GPAC_ENABLE_SVG_SA_BASE
#ifdef GPAC_ENABLE_SVG_SA
	case TAG_SVG_SA_audio:
#endif
#ifdef GPAC_ENABLE_SVG_SANI
	case TAG_SVG_SANI_audio:
#endif
		/*for heritage and anims*/
		svg_sa_render_base(node, (RenderEffect2D *)rs, &backup_props, &backup_flags);
		break;
#endif
	case TAG_SVG_audio:
		gf_svg_flatten_attributes((SVG_Element *)node, &all_atts);
		svg_render_base(node, &all_atts, (RenderEffect2D *)rs, &backup_props, &backup_flags);
		break;
	}

	/*store mute flag*/
	st->input.is_muted = 0;
	if ((eff->trav_flags & GF_SR_TRAV_SWITCHED_OFF)
		|| svg_is_display_off(eff->svg_props)
		|| (*(eff->svg_props->visibility) == SVG_VISIBILITY_HIDDEN) ) {
	
		st->input.is_muted = 1;
	}

	if (eff->svg_props->audio_level) {
		st->input.intensity = eff->svg_props->audio_level->value;
	} else {
		st->input.intensity = FIX_ONE;
	}

	memcpy(eff->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
	eff->svg_flags = backup_flags;
}

void svg_init_audio(Render2D *sr, GF_Node *node)
{
	u32 tag;
	SVG_audio_stack *st;
	GF_SAFEALLOC(st, SVG_audio_stack)

	gf_sr_audio_setup(&st->input, sr->compositor, node);

	/* create an MFURL from the SVG iri */
	tag = gf_node_get_tag(node);
	switch (tag) {
#ifdef GPAC_ENABLE_SVG_SA_BASE
#ifdef GPAC_ENABLE_SVG_SA
	case TAG_SVG_SA_audio:
#endif
#ifdef GPAC_ENABLE_SVG_SANI
	case TAG_SVG_SANI_audio:
#endif
		gf_term_set_mfurl_from_uri(sr->compositor->term, &(st->aurl), & ((SVG_SA_audioElement *)node)->xlink->href);
		if (((SVG_SA_Element *)node)->timing->runtime) {
			SMIL_Timing_RTI *rti = ((SVG_SA_Element *)node)->timing->runtime;
			rti->evaluate = svg_audio_smil_evaluate;
		}
		break;
#endif
	case TAG_SVG_audio:
	{
		GF_FieldInfo href_info;
		if (gf_svg_get_attribute_by_tag(node, TAG_SVG_ATT_xlink_href, 0, 0, &href_info) == GF_OK) {
			gf_term_set_mfurl_from_uri(sr->compositor->term, &(st->aurl), href_info.far_ptr);
		}
		if (((SVGTimedAnimBaseElement *)node)->timingp->runtime) {
			SMIL_Timing_RTI *rti = ((SVGTimedAnimBaseElement *)node)->timingp->runtime;
			rti->evaluate = svg_audio_smil_evaluate;
		}
	}
		break;
	}
	gf_node_set_private(node, st);
	gf_node_set_callback_function(node, SVG_Render_audio);
}

#endif //GPAC_DISABLE_SVG

