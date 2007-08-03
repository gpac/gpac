/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Cyril Concolato - Jean le Feuvre
 *				Copyright (c) 2005-200X ENST
 *					All rights reserved
 *
 *  This file is part of GPAC / 2D+3D Rendering sub-project
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

#ifndef GPAC_DISABLE_SVG
#include "node_stacks.h"

#include <gpac/internal/scenegraph_dev.h>


typedef struct
{
	GF_TextureHandler txh;
	Drawable *graph;
	MFURL txurl;	
	Bool first_frame_fetched;
	GF_Node *audio;
	Bool stop_requested;
} SVG_video_stack;

static void SVG_Draw_bitmap(GF_TraverseState *tr_state)
{
	u8 alpha;
	Render *sr;
	Bool use_blit;
	DrawableContext *ctx = tr_state->ctx;
	sr = tr_state->visual->render;

	use_blit = 1;
	alpha = GF_COL_A(ctx->aspect.fill_color);

	/*this is not a native texture, use graphics*/
	if (!ctx->aspect.fill_texture->data || ctx->transform.m[1] || ctx->transform.m[3]) {
		use_blit = 0;
	} else {
		if (!tr_state->visual->SupportsFormat || !tr_state->visual->DrawBitmap ) use_blit = 0;
		/*format not supported directly, try with brush*/
		else if (!tr_state->visual->SupportsFormat(tr_state->visual, ctx->aspect.fill_texture->pixelformat) ) use_blit = 0;
	}

	/*no HW, fall back to the graphics driver*/
	if (!use_blit) {
		visual_2d_texture_path(tr_state->visual, ctx->drawable->path, ctx);
		return;
	}

	/*direct rendering, render without clippers */
	if (tr_state->visual->render->traverse_state->trav_flags & TF_RENDER_DIRECT) {
		tr_state->visual->DrawBitmap(tr_state->visual, ctx->aspect.fill_texture, &ctx->bi->clip, &ctx->bi->unclip, alpha, NULL, ctx->col_mat);
	}
	/*render bitmap for all dirty rects*/
	else {
		u32 i;
		GF_IRect clip;
		for (i=0; i<tr_state->visual->to_redraw.count; i++) {
			/*there's an opaque region above, don't draw*/
#ifdef TRACK_OPAQUE_REGIONS
			if (tr_state->visual->draw_node_index < tr_state->visual->to_redraw.opaque_node_index[i]) continue;
#endif
			clip = ctx->bi->clip;
			gf_irect_intersect(&clip, &tr_state->visual->to_redraw.list[i]);
			if (clip.width && clip.height) {
				tr_state->visual->DrawBitmap(tr_state->visual, ctx->aspect.fill_texture, &clip, &ctx->bi->unclip, alpha, NULL, ctx->col_mat);
			}
		}
	}
}

static void SVG_Build_Bitmap_Graph(SVG_video_stack *stack, GF_TraverseState *tr_state)
{
	GF_Rect rc, new_rc;
	Fixed x, y, width, height;
	u32 tag = gf_node_get_tag(stack->graph->node);
	switch (tag) {
	case TAG_SVG_image:
	case TAG_SVG_video:
		{
			SVG_Element *e = (SVG_Element *)stack->graph->node;
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

	if (!width) width = INT2FIX(stack->txh.width);
	if (!height) height = INT2FIX(stack->txh.height);

	gf_path_get_bounds(stack->graph->path, &rc);
	drawable_reset_path(stack->graph);
	gf_path_add_rect_center(stack->graph->path, x+width/2, y+height/2, width, height);
	gf_path_get_bounds(stack->graph->path, &new_rc);
	if (!gf_rect_equal(rc, new_rc)) 
		drawable_mark_modified(stack->graph, tr_state);

	gf_node_dirty_clear(stack->graph->node, GF_SG_SVG_GEOMETRY_DIRTY);
}


static void svg_play_texture(SVG_video_stack *stack, SVGAllAttributes *atts)
{
	SVGAllAttributes all_atts;
	Bool lock_scene = 0;
	if (stack->txh.is_open) gf_sr_texture_stop(&stack->txh);

	if (!atts) {
		gf_svg_flatten_attributes((SVG_Element*)stack->txh.owner, &all_atts);
		atts = &all_atts;
	}
	if (atts->syncBehavior) lock_scene = (*atts->syncBehavior == SMIL_SYNCBEHAVIOR_LOCKED) ? 1 : 0;

	gf_sr_texture_play_from_to(&stack->txh, &stack->txurl, 
		atts->clipBegin ? (*atts->clipBegin) : 0.0,
		atts->clipEnd ? (*atts->clipEnd) : -1.0,
		0, 
		lock_scene);
}

static void svg_render_bitmap(GF_Node *node, void *rs, Bool is_destroy)
{
	/*video stack is just an extension of image stack, type-casting is OK*/
	SVG_video_stack *stack = (SVG_video_stack*)gf_node_get_private(node);
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	GF_Matrix2D backup_matrix;
	GF_Matrix mx_3d;
	DrawableContext *ctx;
	SVGAllAttributes all_atts;


	if (is_destroy) {
		gf_sr_texture_destroy(&stack->txh);
		gf_sg_mfurl_del(stack->txurl);

		drawable_del(stack->graph);
		if (stack->audio) {
			gf_node_unregister(stack->audio, NULL);
		}
		free(stack);
		return;
	} 


	/*TRAVERSE_DRAW is NEVER called in 3D mode*/
	if (tr_state->traversing_mode==TRAVERSE_DRAW_2D) {
		SVG_Draw_bitmap(tr_state);
		return;
	}
	else if (tr_state->traversing_mode==TRAVERSE_PICK) {
		/*!! FIXME heritance needed in 3D mode !!*/
		drawable_pick(stack->graph, tr_state);
		return;
	}

	/*flatten attributes and apply animations + inheritance*/
	gf_svg_flatten_attributes((SVG_Element *)node, &all_atts);
	render_svg_render_base(node, &all_atts, (GF_TraverseState *)rs, &backup_props, &backup_flags);

	if (gf_node_dirty_get(node)) {
		GF_FieldInfo href_info;
		SVG_Build_Bitmap_Graph((SVG_video_stack*)gf_node_get_private(node), tr_state);
		/*if open and changed, stop and play*/
		if (gf_svg_get_attribute_by_tag(node, TAG_SVG_ATT_xlink_href, 0, 0, &href_info) == GF_OK) {
			if (gf_term_check_iri_change(stack->txh.compositor->term, &stack->txurl, href_info.far_ptr)) {
				const char *cache_dir = gf_cfg_get_key(stack->txh.compositor->user->config, "General", "CacheDirectory");
				gf_svg_store_embedded_data(href_info.far_ptr, cache_dir, "embedded_");

				if (gf_term_check_iri_change(stack->txh.compositor->term, &stack->txurl, href_info.far_ptr) == GF_OK) {
					gf_term_set_mfurl_from_uri(stack->txh.compositor->term, &(stack->txurl), href_info.far_ptr);
					svg_play_texture(stack, &all_atts);
				}
			} 
		}
		gf_node_dirty_clear(node, GF_SG_NODE_DIRTY);
	} 
	
	/*FIXME: setup aspect ratio*/
	if (tr_state->traversing_mode == TRAVERSE_GET_BOUNDS) {
		if (!render_svg_is_display_off(tr_state->svg_props)) {
			gf_path_get_bounds(stack->graph->path, &tr_state->bounds);
			render_svg_apply_local_transformation(tr_state, &all_atts, &backup_matrix, &mx_3d);
			gf_mx2d_apply_rect(&tr_state->transform, &tr_state->bounds);
			render_svg_restore_parent_transformation(tr_state, &backup_matrix, &mx_3d);
		}
	} else if (tr_state->traversing_mode == TRAVERSE_RENDER) {
		if (!render_svg_is_display_off(tr_state->svg_props) && ( *(tr_state->svg_props->visibility) != SVG_VISIBILITY_HIDDEN) ) {

			render_svg_apply_local_transformation(tr_state, &all_atts, &backup_matrix, &mx_3d);

			ctx = drawable_init_context_svg(stack->graph, tr_state);
			if (!ctx || !ctx->aspect.fill_texture ) return;

			/*even if set this is not true*/
			ctx->aspect.pen_props.width = 0;
			ctx->flags |= CTX_NO_ANTIALIAS;

			/*if rotation, transparent*/
			ctx->flags &= ~CTX_IS_TRANSPARENT;
			if (ctx->transform.m[1] || ctx->transform.m[3]) {
				ctx->flags |= CTX_IS_TRANSPARENT;
				ctx->flags &= ~CTX_NO_ANTIALIAS;
			}
			else if (ctx->aspect.fill_texture->transparent) 
				ctx->flags |= CTX_IS_TRANSPARENT;
			else if (tr_state->svg_props->opacity && (tr_state->svg_props->opacity->type==SVG_NUMBER_VALUE) && (tr_state->svg_props->opacity->value!=FIX_ONE)) {
				ctx->flags = CTX_IS_TRANSPARENT;
				ctx->aspect.fill_color = GF_COL_ARGB(FIX2INT(0xFF * tr_state->svg_props->opacity->value), 0, 0, 0);
			}

#ifndef GPAC_DISABLE_3D
			if (tr_state->visual->type_3d) {
				if (!stack->graph->mesh) {
					stack->graph->mesh = new_mesh();
					mesh_from_path(stack->graph->mesh, stack->graph->path);
				}
				render_3d_draw_bitmap(stack->graph, &ctx->aspect, tr_state, 0, 0, FIX_ONE, FIX_ONE);
				ctx->drawable = NULL;
			} else 
#endif
			{
				drawable_finalize_render(ctx, tr_state, NULL);
			}
			render_svg_restore_parent_transformation(tr_state, &backup_matrix, &mx_3d);
		}
	}
	memcpy(tr_state->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
	tr_state->svg_flags = backup_flags;

	if (stack->audio) gf_node_render(stack->audio, rs);

}

/*********************/
/* SVG image element */
/*********************/

static void SVG_Update_image(GF_TextureHandler *txh)
{	
	MFURL *txurl = &(((SVG_video_stack *)gf_node_get_private(txh->owner))->txurl);

	/*setup texture if needed*/
	if (!txh->is_open && txurl->count) {
		gf_sr_texture_play_from_to(txh, txurl, 0, -1, 0, 0);
	}
	gf_sr_texture_update_frame(txh, 0);
	/*URL is present but not opened - redraw till fetch*/
	if (txh->stream && !txh->hwtx) gf_sr_invalidate(txh->compositor, NULL);
}

static void SVG_Render_image(GF_Node *node, void *rs, Bool is_destroy)
{
	svg_render_bitmap(node, rs, is_destroy);
}

void render_init_svg_image(Render *sr, GF_Node *node)
{
	GF_FieldInfo href_info;
	SVG_video_stack *stack;
	GF_SAFEALLOC(stack, SVG_video_stack)
	stack->graph = drawable_new();
	stack->graph->flags = DRAWABLE_USE_TRAVERSE_DRAW;
	stack->graph->node = node;

	gf_sr_texture_setup(&stack->txh, sr->compositor, node);
	stack->txh.update_texture_fcnt = SVG_Update_image;
	stack->txh.flags = 0;

	if (gf_svg_get_attribute_by_tag(node, TAG_SVG_ATT_xlink_href, 0, 0, &href_info) == GF_OK) {
		gf_term_set_mfurl_from_uri(sr->compositor->term, &(stack->txurl), href_info.far_ptr);
	}

	gf_node_set_private(node, stack);
	gf_node_set_callback_function(node, SVG_Render_image);
}

/*********************/
/* SVG video element */
/*********************/
static void SVG_Update_video(GF_TextureHandler *txh)
{
	GF_FieldInfo init_vis_info;
	SVG_video_stack *stack = (SVG_video_stack *) gf_node_get_private(txh->owner);
	
	if (!txh->is_open) {
		u32 tag;
		SVG_InitialVisibility init_vis;
		if (stack->first_frame_fetched) return;

		tag = gf_node_get_tag(txh->owner);
		init_vis = SVG_INITIALVISIBILTY_WHENSTARTED;

		if (gf_svg_get_attribute_by_tag(txh->owner, TAG_SVG_ATT_initialVisibility, 0, 0, &init_vis_info) == GF_OK) {
			init_vis = *(SVG_InitialVisibility *)init_vis_info.far_ptr;
		}

		/*opens stream only at first access to fetch first frame if needed*/
		if (init_vis == SVG_INITIALVISIBILTY_ALWAYS) {
			svg_play_texture((SVG_video_stack*)stack, NULL);
			gf_sr_invalidate(txh->compositor, NULL);
		}
		return;
	} 

	/*when fetching the first frame disable resync*/
	gf_sr_texture_update_frame(txh, 0);

	/* only when needs_refresh = 1, first frame is fetched */
	if (!stack->first_frame_fetched) {
		if (!stack->audio && gf_mo_has_audio(stack->txh.stream)) {
			GF_FieldInfo att_vid, att_aud;
			stack->audio = gf_node_new(stack->txh.owner->sgprivate->scenegraph, TAG_SVG_audio);
			gf_node_register(stack->audio, NULL);
			if (gf_svg_get_attribute_by_tag(stack->txh.owner, TAG_SVG_ATT_xlink_href, 0, 0, &att_vid)==GF_OK) {
				gf_svg_get_attribute_by_tag(stack->audio, TAG_SVG_ATT_xlink_href, 1, 0, &att_aud);
				gf_svg_attributes_copy(&att_aud, &att_vid, 0);
			}
			/*BYPASS SMIL TIMING MODULE!!*/
			render_init_svg_audio((Render *)stack->txh.compositor->visual_renderer->user_priv, stack->audio, 1);
		}
		if (txh->needs_refresh) {
			stack->first_frame_fetched = 1;
			/*stop stream if needed*/
			if (!gf_smil_timing_is_active(txh->owner)) {
				gf_sr_texture_stop(txh);
				//make sure the refresh flag is not cleared
				txh->needs_refresh = 1;
			}
		}
	}
	/*we have no choice but retraversing the graph until we're inactive since the movie framerate and
	the renderer framerate are likely to be different */
	if (!txh->stream_finished) 
		gf_sr_invalidate(txh->compositor, NULL);

	if (stack->stop_requested) {
		stack->stop_requested = 0;
		gf_sr_texture_stop(&stack->txh);
	}
}

static void svg_audio_smil_evaluate_ex(SMIL_Timing_RTI *rti, Fixed normalized_scene_time, u32 status, GF_Node *audio, GF_Node *video);

static void svg_video_smil_evaluate(SMIL_Timing_RTI *rti, Fixed normalized_scene_time, u32 status)
{
	SVG_video_stack *stack = (SVG_video_stack *)gf_node_get_private((GF_Node *)rti->timed_elt);

	switch (status) {
	case SMIL_TIMING_EVAL_UPDATE:
		if (!stack->txh.is_open) { 
			svg_play_texture((SVG_video_stack*)stack, NULL);
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
	case SMIL_TIMING_EVAL_REMOVE:
		stack->stop_requested = 1;
		break;
	case SMIL_TIMING_EVAL_REPEAT:
		gf_sr_texture_restart(&stack->txh);
		break;
	}
	if (stack->audio) svg_audio_smil_evaluate_ex(rti, normalized_scene_time, status, stack->audio, stack->txh.owner);
}

static void SVG_Render_video(GF_Node *node, void *rs, Bool is_destroy)
{
	svg_render_bitmap(node, rs, is_destroy);
}

void render_init_svg_video(Render *sr, GF_Node *node)
{
	GF_FieldInfo href_info;
	SVG_video_stack *stack;
	GF_SAFEALLOC(stack, SVG_video_stack)
	stack->graph = drawable_new();
	stack->graph->flags = DRAWABLE_USE_TRAVERSE_DRAW;
	stack->graph->node = node;

	gf_sr_texture_setup(&stack->txh, sr->compositor, node);
	stack->txh.update_texture_fcnt = SVG_Update_video;
	stack->txh.flags = 0;

	/* create an MFURL from the SVG iri */
	if (gf_svg_get_attribute_by_tag(node, TAG_SVG_ATT_xlink_href, 0, 0, &href_info) == GF_OK) {
		gf_term_set_mfurl_from_uri(sr->compositor->term, &(stack->txurl), href_info.far_ptr);
	}
	if (((SVGTimedAnimBaseElement *)node)->timingp->runtime) {
		SMIL_Timing_RTI *rti = ((SVGTimedAnimBaseElement *)node)->timingp->runtime;
		rti->evaluate = svg_video_smil_evaluate;
	}

	gf_node_set_private(node, stack);
	gf_node_set_callback_function(node, SVG_Render_video);
}

/*********************/
/* SVG audio element */
/*********************/
typedef struct
{
	GF_AudioInput input;
	Bool is_active;
	MFURL aurl;
} SVG_audio_stack;

static void svg_audio_smil_evaluate_ex(SMIL_Timing_RTI *rti, Fixed normalized_scene_time, u32 status, GF_Node *audio, GF_Node *video)
{
	SVG_audio_stack *stack;
	if (!audio) audio = rti->timed_elt;
	stack = (SVG_audio_stack *)gf_node_get_private(audio);
	
	switch (status) {
	case SMIL_TIMING_EVAL_UPDATE:
		if (!stack->is_active) { 
			SVGAllAttributes atts;
			gf_svg_flatten_attributes((SVG_Element*) (video ? video : audio), &atts);

			if (gf_sr_audio_open(&stack->input, &stack->aurl,
					atts.clipBegin ? (*atts.clipBegin) : 0.0,
					atts.clipEnd ? (*atts.clipEnd) : -1.0) == GF_OK) 
			{
				gf_mo_set_speed(stack->input.stream, FIX_ONE);
				stack->is_active = 1;
			}
		}
		else if (!audio && stack->input.stream_finished && (rti->media_duration < 0) ) { 
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

static void svg_audio_smil_evaluate(SMIL_Timing_RTI *rti, Fixed normalized_scene_time, u32 status)
{
	svg_audio_smil_evaluate_ex(rti, normalized_scene_time, status, NULL, NULL);
}


static void SVG_Render_audio(GF_Node *node, void *rs, Bool is_destroy)
{
	SVGAllAttributes all_atts;
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	GF_TraverseState *tr_state = (GF_TraverseState*)rs;
	SVG_audio_stack *stack = (SVG_audio_stack *)gf_node_get_private(node);

	if (is_destroy) {
		gf_sr_audio_stop(&stack->input);
		gf_sr_audio_unregister(&stack->input);
		gf_sg_mfurl_del(stack->aurl);
		free(stack);
		return;
	}
	if (stack->is_active) {
		gf_sr_audio_register(&stack->input, (GF_BaseEffect*)rs);
	}

	gf_svg_flatten_attributes((SVG_Element *)node, &all_atts);
	render_svg_render_base(node, &all_atts, (GF_TraverseState *)rs, &backup_props, &backup_flags);

	/*store mute flag*/
	stack->input.is_muted = 0;
	if ((tr_state->trav_flags & GF_SR_TRAV_SWITCHED_OFF)
		|| render_svg_is_display_off(tr_state->svg_props)
		|| (*(tr_state->svg_props->visibility) == SVG_VISIBILITY_HIDDEN) ) {
	
		stack->input.is_muted = 1;
	}

	if (tr_state->svg_props->audio_level) {
		stack->input.intensity = tr_state->svg_props->audio_level->value;
	} else {
		stack->input.intensity = FIX_ONE;
	}

	memcpy(tr_state->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
	tr_state->svg_flags = backup_flags;
}

void render_init_svg_audio(Render *sr, GF_Node *node, Bool slaved_timing)
{
	GF_FieldInfo href_info;
	SVG_audio_stack *stack;
	GF_SAFEALLOC(stack, SVG_audio_stack)

	gf_sr_audio_setup(&stack->input, sr->compositor, node);

	/* create an MFURL from the SVG iri */
	if (gf_svg_get_attribute_by_tag(node, TAG_SVG_ATT_xlink_href, 0, 0, &href_info) == GF_OK) {
		gf_term_set_mfurl_from_uri(sr->compositor->term, &(stack->aurl), href_info.far_ptr);
	}
	if (!slaved_timing &&  ((SVGTimedAnimBaseElement *)node)->timingp->runtime) {
		SMIL_Timing_RTI *rti = ((SVGTimedAnimBaseElement *)node)->timingp->runtime;
		rti->evaluate = svg_audio_smil_evaluate;
	}
	gf_node_set_private(node, stack);
	gf_node_set_callback_function(node, SVG_Render_audio);
}

GF_TextureHandler *render_svg_get_image_texture(GF_Node *node)
{
	SVG_video_stack *st = (SVG_video_stack *) gf_node_get_private(node);
	return &(st->txh);
}

#endif //GPAC_DISABLE_SVG

