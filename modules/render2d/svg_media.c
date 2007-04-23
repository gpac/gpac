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
	case TAG_SVG_image:
		{
			SVGimageElement *img = (SVGimageElement *)st->graph->node;
			x = img->x.value;
			y = img->y.value;
			width = img->width.value;
			height = img->height.value;
		}
		break;
	case TAG_SVG_video:
		{
			SVGvideoElement *video = (SVGvideoElement *)st->graph->node;
			x = video->x.value;
			y = video->y.value;
			width = video->width.value;
			height = video->height.value;
		}
		break;
	case TAG_SVG2_image:
		{
			SVG2imageElement *img = (SVG2imageElement *)st->graph->node;
			x = img->x.value;
			y = img->y.value;
			width = img->width.value;
			height = img->height.value;
		}
		break;
	case TAG_SVG2_video:
		{
			SVG2videoElement *video = (SVG2videoElement *)st->graph->node;
			x = video->x.value;
			y = video->y.value;
			width = video->width.value;
			height = video->height.value;
		}
		break;
	case TAG_SVG3_image:
	case TAG_SVG3_video:
		{
			SVG3Element *e = (SVG3Element *)st->graph->node;
			SVG3AllAttributes atts;
			gf_svg3_fill_all_attributes(&atts, e);
			x = (atts.x ? atts.x->value : 0);
			y = (atts.y ? atts.y->value : 0);
			width = (atts.width ? atts.width->value : 0);
			height = (atts.height ? atts.height->value : 0);
		}
		break;
	}

	gf_path_get_bounds(st->graph->path, &rc);
	drawable_reset_path(st->graph);
	gf_path_add_rect_center(st->graph->path, x+width/2, y+height/2, width, height);
	gf_path_get_bounds(st->graph->path, &new_rc);
	if (!gf_rect_equal(rc, new_rc)) st->graph->flags |= DRAWABLE_HAS_CHANGED;
	gf_node_dirty_clear(st->graph->node, GF_SG_SVG_GEOMETRY_DIRTY);
}

static void SVG_Render_bitmap(GF_Node *node, void *rs)
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

	SVG_Render_base(node, eff, &backup_props, &backup_flags);

	if (gf_node_dirty_get(node)) {
		SVG_Build_Bitmap_Graph((SVG_image_stack*)gf_node_get_private(node));
		/*if open and changed, stop and play*/
		if (gf_term_check_iri_change(st->txh.compositor->term, &st->txurl, & ((SVGElement *)node)->xlink->href)) {
			const char *cache_dir = gf_cfg_get_key(st->txh.compositor->user->config, "General", "CacheDirectory");
			gf_svg_store_embedded_data(& ((SVGElement *)node)->xlink->href, cache_dir, "embedded_");

			if (gf_term_check_iri_change(st->txh.compositor->term, &st->txurl, & ((SVGElement *)node)->xlink->href)) {
				gf_term_set_mfurl_from_uri(st->txh.compositor->term, &(st->txurl), & ((SVGElement*)node)->xlink->href);
				if (st->txh.is_open) gf_sr_texture_stop(&st->txh);
				gf_sr_texture_play(&st->txh, &st->txurl);
			}
		} 
		gf_node_dirty_clear(node, GF_SG_NODE_DIRTY);
	} 
	
	if (gf_node_get_tag(node)==TAG_SVG_image) {
		m = &((SVGimageElement *)node)->transform.mat;
	} else {
		m = &((SVGvideoElement *)node)->transform.mat;
	}

	/*FIXME: setup aspect ratio*/

	if (eff->traversing_mode == TRAVERSE_GET_BOUNDS) {
		gf_mx2d_pre_multiply(&eff->transform, m);
		if (*(eff->svg_props->display) != SVG_DISPLAY_NONE) {
			gf_path_get_bounds(st->graph->path, &eff->bounds);
		}
		memcpy(eff->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
		eff->svg_flags = backup_flags;
		return;
	}

	if (*(eff->svg_props->display) == SVG_DISPLAY_NONE ||
		*(eff->svg_props->visibility) == SVG_VISIBILITY_HIDDEN) {
		memcpy(eff->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
		eff->svg_flags = backup_flags;
		return;
	}

	gf_mx2d_copy(backup_matrix, eff->transform);
	gf_mx2d_pre_multiply(&eff->transform, m);

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
	gf_mx2d_copy(eff->transform, backup_matrix);  
	memcpy(eff->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
	eff->svg_flags = backup_flags;
}

static void SVG3_Render_bitmap(GF_Node *node, void *rs)
{
	/*video stack is just an extension of image stack, type-casting is OK*/
	SVG_image_stack *st = (SVG_image_stack*)gf_node_get_private(node);
	RenderEffect2D *eff = (RenderEffect2D *)rs;
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	GF_Matrix2D backup_matrix;
	DrawableContext *ctx;
	SVG3AllAttributes all_atts;

	if (eff->traversing_mode==TRAVERSE_DRAW) {
		SVG_Draw_bitmap(eff);
		return;
	}
	else if (eff->traversing_mode==TRAVERSE_PICK) {
		drawable_pick(eff);
		return;
	}


	memset(&all_atts, 0, sizeof(SVG3AllAttributes));
	gf_svg3_fill_all_attributes(&all_atts, (SVG3Element *)node);

	SVG3_Render_base(node, &all_atts, (RenderEffect2D *)rs, &backup_props, &backup_flags);

	if (gf_node_dirty_get(node)) {
		GF_FieldInfo href_info;
		SVG_Build_Bitmap_Graph((SVG_image_stack*)gf_node_get_private(node));
		/*if open and changed, stop and play*/
		if (gf_svg3_get_attribute_by_tag(node, TAG_SVG3_ATT_xlink_href, 0, 0, &href_info) == GF_OK) {
			if (gf_term_check_iri_change(st->txh.compositor->term, &st->txurl, href_info.far_ptr)) {
				const char *cache_dir = gf_cfg_get_key(st->txh.compositor->user->config, "General", "CacheDirectory");
				gf_svg_store_embedded_data(href_info.far_ptr, cache_dir, "embedded_");

				if (gf_term_check_iri_change(st->txh.compositor->term, &st->txurl, href_info.far_ptr) == GF_OK) {
					gf_term_set_mfurl_from_uri(st->txh.compositor->term, &(st->txurl), href_info.far_ptr);
					if (st->txh.is_open) gf_sr_texture_stop(&st->txh);
					gf_sr_texture_play(&st->txh, &st->txurl);
				}
			} 
		}

		gf_node_dirty_clear(node, GF_SG_NODE_DIRTY);
	} 
	
	
	/*FIXME: setup aspect ratio*/
	if (eff->traversing_mode == TRAVERSE_GET_BOUNDS) {
		gf_svg3_apply_local_transformation(eff, &all_atts, &backup_matrix);
		if (*(eff->svg_props->display) != SVG_DISPLAY_NONE) {
			gf_path_get_bounds(st->graph->path, &eff->bounds);
		}
		gf_svg_restore_parent_transformation(eff, &backup_matrix);
		memcpy(eff->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
		eff->svg_flags = backup_flags;
		return;
	}

	if (*(eff->svg_props->display) == SVG_DISPLAY_NONE ||
		*(eff->svg_props->visibility) == SVG_VISIBILITY_HIDDEN) {
		memcpy(eff->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
		eff->svg_flags = backup_flags;
		return;
	}

	gf_svg3_apply_local_transformation(eff, &all_atts, &backup_matrix);

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
	gf_svg_restore_parent_transformation(eff, &backup_matrix);
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
		gf_sr_texture_play(txh, txurl);
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
		u32 tag = gf_node_get_tag(node);
		if (tag == TAG_SVG_image) {
			SVG_Render_bitmap(node, rs);
		} else if (tag == TAG_SVG3_image) {
			SVG3_Render_bitmap(node, rs);
		}
	}
}

void SVG_Init_image(Render2D *sr, GF_Node *node)
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
	if (tag == TAG_SVG_image) {
		gf_term_set_mfurl_from_uri(sr->compositor->term, &(st->txurl), & ((SVGimageElement*)node)->xlink->href);
	} else if (tag == TAG_SVG3_image) {
		GF_FieldInfo href_info;
		if (gf_svg3_get_attribute_by_tag(node, TAG_SVG3_ATT_xlink_href, 0, 0, &href_info) == GF_OK) {
			gf_term_set_mfurl_from_uri(sr->compositor->term, &(st->txurl), href_info.far_ptr);
		}
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
	u32 tag = gf_node_get_tag(txh->owner);
	SVG_InitialVisibility init_vis;

	if (tag == TAG_SVG_video) {
		init_vis = ((SVGvideoElement *)txh->owner)->initialVisibility;
	} else if (tag == TAG_SVG3_video) {
		GF_FieldInfo init_vis_info;
		if (gf_svg3_get_attribute_by_tag(txh->owner, TAG_SVG3_ATT_initialVisibility, 0, 0, &init_vis_info) == GF_OK) {
			init_vis = *(SVG_InitialVisibility *)init_vis_info.far_ptr;
		} else {
			init_vis = SVG_INITIALVISIBILTY_WHENSTARTED;
		}
	}

	if (!txh->is_open) {
		/*opens stream only at first access to fetch first frame if needed*/
		if (!st->first_frame_fetched && (init_vis == SVG_INITIALVISIBILTY_ALWAYS)) {
			gf_sr_texture_play(txh, &st->txurl);
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
			gf_sr_texture_play(&stack->txh, &stack->txurl);
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
		u32 tag = gf_node_get_tag(node);
		if (tag == TAG_SVG_video) {
			SVG_Render_bitmap(node, rs);
		} else if (tag == TAG_SVG3_video) {
			SVG3_Render_bitmap(node, rs);
		}
	}
}

void SVG_Init_video(Render2D *sr, GF_Node *node)
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
	if (tag == TAG_SVG_video) {
		gf_term_set_mfurl_from_uri(sr->compositor->term, &(st->txurl), & ((SVGvideoElement *)node)->xlink->href);
	} else if (tag == TAG_SVG3_video) {
		GF_FieldInfo href_info;
		if (gf_svg3_get_attribute_by_tag(node, TAG_SVG3_ATT_xlink_href, 0, 0, &href_info) == GF_OK) {
			gf_term_set_mfurl_from_uri(sr->compositor->term, &(st->txurl), href_info.far_ptr);
		}
	}
	
	gf_smil_timing_init_runtime_info(node);
	if (tag == TAG_SVG_video) {
		if (((SVGElement *)node)->timing->runtime) {
			SMIL_Timing_RTI *rti = ((SVGElement *)node)->timing->runtime;
			rti->evaluate = svg_video_smil_evaluate;
		}
	} else if (tag == TAG_SVG3_video) {
		if (((SVG3TimedAnimBaseElement *)node)->timingp->runtime) {
			SMIL_Timing_RTI *rti = ((SVG3TimedAnimBaseElement *)node)->timingp->runtime;
			rti->evaluate = svg_video_smil_evaluate;
		}
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
			gf_sr_audio_open(&stack->input, &stack->aurl);
			gf_mo_set_speed(stack->input.stream, FIX_ONE);
			stack->is_active = 1;
		}
		break;
	case SMIL_TIMING_EVAL_REPEAT:
		if (stack->is_active) gf_sr_audio_restart(&stack->input);
		break;
	case SMIL_TIMING_EVAL_FREEZE:
		gf_sr_audio_stop(&stack->input);
		stack->is_active = 0;
		break;
	case SMIL_TIMING_EVAL_REMOVE:
		gf_sr_audio_stop(&stack->input);
		stack->is_active = 0;
		break;
	}
}

static void SVG_Render_audio(GF_Node *node, void *rs, Bool is_destroy)
{
	u32 tag;
	SVG3AllAttributes all_atts;
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
	if (tag == TAG_SVG_audio) {
		/*for heritage and anims*/
		SVG_Render_base(node, (RenderEffect2D *)rs, &backup_props, &backup_flags);
	} else if (tag == TAG_SVG3_audio) {
		memset(&all_atts, 0, sizeof(SVG3AllAttributes));
		gf_svg3_fill_all_attributes(&all_atts, (SVG3Element *)node);
		SVG3_Render_base(node, &all_atts, (RenderEffect2D *)rs, &backup_props, &backup_flags);
	}

	/*store mute flag*/
	st->input.is_muted = 0;
	if ((eff->trav_flags & GF_SR_TRAV_SWITCHED_OFF)
		|| (*(eff->svg_props->display) == SVG_DISPLAY_NONE) 
		|| (*(eff->svg_props->visibility) == SVG_VISIBILITY_HIDDEN) ) {
	
		st->input.is_muted = 1;
	}

	memcpy(eff->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
	eff->svg_flags = backup_flags;
}

void SVG_Init_audio(Render2D *sr, GF_Node *node)
{
	u32 tag;
	SVG_audio_stack *st;
	GF_SAFEALLOC(st, SVG_audio_stack)

	gf_sr_audio_setup(&st->input, sr->compositor, node);

	/* create an MFURL from the SVG iri */
	tag = gf_node_get_tag(node);
	if (tag == TAG_SVG_audio) {
		gf_term_set_mfurl_from_uri(sr->compositor->term, &(st->aurl), & ((SVGaudioElement *)node)->xlink->href);
	} else if (tag == TAG_SVG3_audio) {
		GF_FieldInfo href_info;
		if (gf_svg3_get_attribute_by_tag(node, TAG_SVG3_ATT_xlink_href, 0, 0, &href_info) == GF_OK) {
			gf_term_set_mfurl_from_uri(sr->compositor->term, &(st->aurl), href_info.far_ptr);
		}
	}

	gf_smil_timing_init_runtime_info(node);
	if (tag == TAG_SVG_audio) {
		if (((SVGElement *)node)->timing->runtime) {
			SMIL_Timing_RTI *rti = ((SVGElement *)node)->timing->runtime;
			rti->evaluate = svg_audio_smil_evaluate;
		}
	} else if (tag == TAG_SVG3_audio) {
		if (((SVG3TimedAnimBaseElement *)node)->timingp->runtime) {
			SMIL_Timing_RTI *rti = ((SVG3TimedAnimBaseElement *)node)->timingp->runtime;
			rti->evaluate = svg_audio_smil_evaluate;
		}
	}
	
	gf_node_set_private(node, st);
	gf_node_set_callback_function(node, SVG_Render_audio);
}


/* TODO: FIX ME we actually ignore the given sub_root since it is only valid 
	     when animations have been performed,
         animations evaluation (SVG_Render_base) should be part of the core renderer */
void R2D_RenderUse(GF_Node *node, GF_Node *sub_root, void *rs)
{
	GF_Matrix2D backup_matrix;
  	GF_Matrix2D translate;
	GF_Node *prev_use;
	SVGuseElement *use = (SVGuseElement *)node;
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	RenderEffect2D *eff = (RenderEffect2D *)rs;

	SVG_Render_base(node, eff, &backup_props, &backup_flags);

	gf_mx2d_init(translate);
	translate.m[2] = use->x.value;
	translate.m[5] = use->y.value;

	if (eff->traversing_mode == TRAVERSE_GET_BOUNDS) {
		gf_svg_apply_local_transformation(eff, node, &backup_matrix);
		if (*(eff->svg_props->display) != SVG_DISPLAY_NONE) {
			gf_node_render((GF_Node*)use->xlink->href.target, eff);
			gf_mx2d_apply_rect(&translate, &eff->bounds);
		} 
		gf_svg_restore_parent_transformation(eff, &backup_matrix);
		goto end;
	}

	if (*(eff->svg_props->display) == SVG_DISPLAY_NONE ||
		*(eff->svg_props->visibility) == SVG_VISIBILITY_HIDDEN) {
		goto end;
	}

	gf_svg_apply_local_transformation(eff, node, &backup_matrix);

	gf_mx2d_pre_multiply(&eff->transform, &translate);
	prev_use = eff->parent_use;
	eff->parent_use = (GF_Node *)use->xlink->href.target;
	gf_node_render((GF_Node *)use->xlink->href.target, eff);
	eff->parent_use = prev_use;
	gf_svg_restore_parent_transformation(eff, &backup_matrix);  

end:
	memcpy(eff->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
	eff->svg_flags = backup_flags;
}


void R2D_RenderInlineAnimation(GF_Node *anim, GF_Node *sub_root, void *rs)
{
	GF_Matrix2D backup_matrix;
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	RenderEffect2D *eff = (RenderEffect2D*)rs;
	SVGanimationElement *a = (SVGanimationElement*)anim;
  	GF_Matrix2D translate;
	SVGPropertiesPointers new_props, *old_props;

	memset(&new_props, 0, sizeof(SVGPropertiesPointers));

	/*for heritage and anims*/
	SVG_Render_base(anim, (RenderEffect2D *)rs, &backup_props, &backup_flags);


	gf_mx2d_init(translate);
	translate.m[2] = a->x.value;
	translate.m[5] = a->y.value;
	
	if (*(eff->svg_props->display) == SVG_DISPLAY_NONE ||
		*(eff->svg_props->visibility) == SVG_VISIBILITY_HIDDEN) {
		goto end;
	}

	gf_svg_apply_local_transformation(eff, anim, &backup_matrix);
	gf_mx2d_pre_multiply(&eff->transform, &translate);

#if 0
	st = gf_node_get_private(n);
	if (!st->is) return;
	root = gf_sg_get_root_node(st->is->graph);
	if (root) {
		old_props = eff->svg_props;
		eff->svg_props = &new_props;
		gf_svg_properties_init_pointers(eff->svg_props);
		//gf_sr_render_inline(st->is->root_od->term->renderer, root, rs);
		gf_node_render(root, rs);
		eff->svg_props = old_props;
		gf_svg_properties_reset_pointers(&new_props);
//	}
#endif

	old_props = eff->svg_props;
	eff->svg_props = &new_props;
	gf_svg_properties_init_pointers(eff->svg_props);

	gf_node_render(sub_root, rs);
	eff->svg_props = old_props;
	gf_svg_properties_reset_pointers(&new_props);

	gf_svg_restore_parent_transformation(eff, &backup_matrix);  
end:
	memcpy(eff->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
	eff->svg_flags = backup_flags;
}

#endif //GPAC_DISABLE_SVG

