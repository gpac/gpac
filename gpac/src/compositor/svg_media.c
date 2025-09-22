/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Cyril Concolato - Jean le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2023
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

#include "visual_manager.h"

#if !defined(GPAC_DISABLE_SVG) &&  !defined(GPAC_DISABLE_COMPOSITOR)

#include "nodes_stacks.h"

static void svg_audio_smil_evaluate_ex(SMIL_Timing_RTI *rti, Fixed normalized_scene_time, u32 status, GF_Node *audio, GF_Node *video);
static void svg_traverse_audio_ex(GF_Node *node, void *rs, Bool is_destroy, SVGPropertiesPointers *props);



static Bool svg_video_get_transform_behavior(GF_TraverseState *tr_state, SVGAllAttributes *atts, Fixed *cx, Fixed *cy, Fixed *angle)
{
	SFVec2f pt;
	if (!atts->transformBehavior) return GF_FALSE;
	if (*atts->transformBehavior == SVG_TRANSFORMBEHAVIOR_GEOMETRIC)
		return GF_FALSE;

	pt.x = atts->x ? atts->x->value : 0;
	pt.y = atts->y ? atts->y->value : 0;
	gf_mx2d_apply_point(&tr_state->transform, &pt);
	*cx = pt.x;
	*cy = pt.y;

	*angle = 0;
	switch (*atts->transformBehavior) {
	case SVG_TRANSFORMBEHAVIOR_PINNED:
		break;
	case SVG_TRANSFORMBEHAVIOR_PINNED180:
		*angle = GF_PI;
		break;
	case SVG_TRANSFORMBEHAVIOR_PINNED270:
		*angle = -GF_PI/2;
		break;
	case SVG_TRANSFORMBEHAVIOR_PINNED90:
		*angle = GF_PI/2;
		break;
	}
	return GF_TRUE;
}


static void SVG_Draw_bitmap(GF_TraverseState *tr_state)
{
	DrawableContext *ctx = tr_state->ctx;
	if (!tr_state->visual->DrawBitmap(tr_state->visual, tr_state, ctx)) {
		visual_2d_texture_path(tr_state->visual, ctx->drawable->path, ctx, tr_state);
	}
}

static void SVG_Build_Bitmap_Graph(SVG_video_stack *stack, GF_TraverseState *tr_state)
{
	u32 tag;
	GF_Rect rc, new_rc;
	Fixed x, y, width, height, txwidth, txheight;
	Fixed rectx, recty, rectwidth, rectheight;
	SVGAllAttributes atts;
	SVG_PreserveAspectRatio pAR;
	SVG_Element *e = (SVG_Element *)stack->drawable->node;

	gf_svg_flatten_attributes(e, &atts);

	tag = gf_node_get_tag(stack->drawable->node);
	switch (tag) {
	case TAG_SVG_image:
	case TAG_SVG_video:
		x = (atts.x ? atts.x->value : 0);
		y = (atts.y ? atts.y->value : 0);
		width = (atts.width ? atts.width->value : 0);
		height = (atts.height ? atts.height->value : 0);
		break;
	default:
		return;
	}

	if (!width || !height) return;

	txheight = INT2FIX(stack->txh.height);
	txwidth = INT2FIX(stack->txh.width);

	if (!txwidth || !txheight) return;

	if (!atts.preserveAspectRatio) {
		pAR.defer = GF_FALSE;
		pAR.meetOrSlice = SVG_MEETORSLICE_MEET;
		pAR.align = SVG_PRESERVEASPECTRATIO_XMIDYMID;
	} else {
		pAR = *atts.preserveAspectRatio;
	}
	if (pAR.defer) {
		/* TODO */
		rectwidth = width;
		rectheight = height;
		rectx = x+rectwidth/2;
		recty = y+rectheight/2;
	} else {

		if (pAR.align==SVG_PRESERVEASPECTRATIO_NONE) {
			rectwidth = width;
			rectheight = height;
			rectx = x+rectwidth/2;
			recty = y+rectheight/2;
		} else {
			Fixed scale, scale_w, scale_h;
			scale_w = gf_divfix(width, txwidth);
			scale_h = gf_divfix(height, txheight);
			if (pAR.meetOrSlice==SVG_MEETORSLICE_MEET) {
				if (scale_w > scale_h) {
					scale = scale_h;
					rectwidth = gf_mulfix(txwidth, scale);
					rectheight = height;
				} else {
					scale = scale_w;
					rectwidth = width;
					rectheight = gf_mulfix(txheight, scale);
				}
			} else {
				if (scale_w < scale_h) {
					scale = scale_h;
					rectwidth = gf_mulfix(txwidth, scale);
					rectheight = height;
				} else {
					scale = scale_w;
					rectwidth = width;
					rectheight = gf_mulfix(txheight, scale);
				}
			}

			rectx = x + rectwidth/2;
			recty = y + rectheight/2;
			switch (pAR.align) {
			case SVG_PRESERVEASPECTRATIO_XMINYMIN:
				break;
			case SVG_PRESERVEASPECTRATIO_XMIDYMIN:
				rectx += (width - rectwidth)/ 2;
				break;
			case SVG_PRESERVEASPECTRATIO_XMAXYMIN:
				rectx += width - rectwidth;
				break;
			case SVG_PRESERVEASPECTRATIO_XMINYMID:
				recty += (height - rectheight)/ 2;
				break;
			case SVG_PRESERVEASPECTRATIO_XMIDYMID:
				rectx += (width - rectwidth)/ 2;
				recty += (height - rectheight) / 2;
				break;
			case SVG_PRESERVEASPECTRATIO_XMAXYMID:
				rectx += width - rectwidth;
				recty += ( txheight - rectheight) / 2;
				break;
			case SVG_PRESERVEASPECTRATIO_XMINYMAX:
				recty += height - rectheight;
				break;
			case SVG_PRESERVEASPECTRATIO_XMIDYMAX:
				rectx += (width - rectwidth)/ 2;
				recty += height - rectheight;
				break;
			case SVG_PRESERVEASPECTRATIO_XMAXYMAX:
				rectx += width  - rectwidth;
				recty += height - rectheight;
				break;
			}
		}
	}


	gf_path_get_bounds(stack->drawable->path, &rc);
	drawable_reset_path(stack->drawable);
	gf_path_add_rect_center(stack->drawable->path, rectx, recty, rectwidth, rectheight);
	gf_path_get_bounds(stack->drawable->path, &new_rc);
	if (!gf_rect_equal(&rc, &new_rc))
		drawable_mark_modified(stack->drawable, tr_state);
	else if (stack->txh.flags & GF_SR_TEXTURE_PRIVATE_MEDIA)
		drawable_mark_modified(stack->drawable, tr_state);

	gf_node_dirty_clear(stack->drawable->node, GF_SG_SVG_GEOMETRY_DIRTY);
}

static void svg_open_texture(SVG_video_stack *stack)
{
	gf_sc_texture_open(&stack->txh, &stack->txurl, GF_FALSE);
}

static void svg_play_texture(SVG_video_stack *stack, SVGAllAttributes *atts)
{
	SVGAllAttributes all_atts;
	Bool lock_scene = GF_FALSE;
	if (stack->txh.is_open) gf_sc_texture_stop_no_unregister(&stack->txh);

	if (!atts) {
		gf_svg_flatten_attributes((SVG_Element*)stack->txh.owner, &all_atts);
		atts = &all_atts;
	}
	if (atts->syncBehavior) lock_scene = (*atts->syncBehavior == SMIL_SYNCBEHAVIOR_LOCKED) ? GF_TRUE : GF_FALSE;

	gf_sc_texture_play_from_to(&stack->txh, &stack->txurl,
	                           atts->clipBegin ? (*atts->clipBegin) : 0.0,
	                           atts->clipEnd ? (*atts->clipEnd) : -1.0,
	                           GF_FALSE,
	                           lock_scene);
}

static void svg_traverse_bitmap(GF_Node *node, void *rs, Bool is_destroy)
{
	Fixed cx, cy, angle;
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
		gf_sc_texture_destroy(&stack->txh);
		gf_sg_mfurl_del(stack->txurl);

		drawable_del(stack->drawable);
		if (stack->audio) {
			gf_node_unregister(stack->audio, NULL);
		}
		gf_free(stack);
		return;
	}

	if (tr_state->traversing_mode==TRAVERSE_DRAW_2D) {
		SVG_Draw_bitmap(tr_state);
		return;
	}
	else if (tr_state->traversing_mode==TRAVERSE_PICK) {
		svg_drawable_pick(node, stack->drawable, tr_state);
		return;
	}
#ifndef GPAC_DISABLE_3D
	else if (tr_state->traversing_mode==TRAVERSE_DRAW_3D) {
		if (!stack->drawable->mesh) {
			stack->drawable->mesh = new_mesh();
			mesh_from_path(stack->drawable->mesh, stack->drawable->path);
		}
		compositor_3d_draw_bitmap(stack->drawable, &tr_state->ctx->aspect, tr_state, 0, 0, FIX_ONE, FIX_ONE);
		return;
	}
#endif

	/*flatten attributes and apply animations + inheritance*/
	gf_svg_flatten_attributes((SVG_Element *)node, &all_atts);
	if (!compositor_svg_traverse_base(node, &all_atts, (GF_TraverseState *)rs, &backup_props, &backup_flags))
		return;

	if (gf_node_dirty_get(node) & GF_SG_SVG_XLINK_HREF_DIRTY) {
		if (!stack->txh.stream || gf_mo_url_changed(stack->txh.stream, &stack->txurl)) {

			gf_sc_get_mfurl_from_xlink(node, &stack->txurl);
			stack->txh.width = stack->txh.height = 0;

			/*remove associated audio if any*/
			if (stack->audio) {
				svg_audio_smil_evaluate_ex(NULL, 0, SMIL_TIMING_EVAL_REMOVE, stack->audio, stack->txh.owner);
				gf_node_unregister(stack->audio, NULL);
				stack->audio = NULL;
			}
			stack->audio_dirty = GF_TRUE;

			if (stack->txurl.count) svg_play_texture(stack, &all_atts);
		}
		gf_node_dirty_clear(node, GF_SG_SVG_XLINK_HREF_DIRTY);
	}

	if (gf_node_dirty_get(node)) {
		/*do not clear dirty state until the image is loaded*/
		if (stack->txh.width) {
			gf_node_dirty_clear(node, 0);
			SVG_Build_Bitmap_Graph((SVG_video_stack*)gf_node_get_private(node), tr_state);
		}
	}

	if (tr_state->traversing_mode == TRAVERSE_GET_BOUNDS) {
		if (!compositor_svg_is_display_off(tr_state->svg_props)) {
			gf_path_get_bounds(stack->drawable->path, &tr_state->bounds);
			compositor_svg_apply_local_transformation(tr_state, &all_atts, &backup_matrix, &mx_3d);

			if (svg_video_get_transform_behavior(tr_state, &all_atts, &cx, &cy, &angle)) {
				GF_Matrix2D mx;
				tr_state->bounds.width = INT2FIX(stack->txh.width);
				tr_state->bounds.height = INT2FIX(stack->txh.height);
				tr_state->bounds.x = cx - tr_state->bounds.width/2;
				tr_state->bounds.y = cy + tr_state->bounds.height/2;
				gf_mx2d_init(mx);
				gf_mx2d_add_rotation(&mx, 0, 0, angle);
				gf_mx2d_apply_rect(&mx, &tr_state->bounds);
			} else {
				gf_mx2d_apply_rect(&tr_state->transform, &tr_state->bounds);
			}

			compositor_svg_restore_parent_transformation(tr_state, &backup_matrix, &mx_3d);
		}
	} else if (tr_state->traversing_mode == TRAVERSE_SORT) {
		if (!compositor_svg_is_display_off(tr_state->svg_props) && ( *(tr_state->svg_props->visibility) != SVG_VISIBILITY_HIDDEN) ) {
			GF_Matrix mx_bck;
			Bool restore_mx = GF_FALSE;

			compositor_svg_apply_local_transformation(tr_state, &all_atts, &backup_matrix, &mx_3d);

			ctx = drawable_init_context_svg(stack->drawable, tr_state, NULL);
			if (!ctx || !ctx->aspect.fill_texture ) return;

			if (svg_video_get_transform_behavior(tr_state, &all_atts, &cx, &cy, &angle)) {
				drawable_reset_path(stack->drawable);
				gf_path_add_rect_center(stack->drawable->path, cx, cy, INT2FIX(stack->txh.width), INT2FIX(stack->txh.height));

				gf_mx2d_copy(mx_bck, tr_state->transform);
				restore_mx = GF_TRUE;

				gf_mx2d_init(tr_state->transform);
				gf_mx2d_add_rotation(&tr_state->transform, cx, cy, angle);
			}

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
				if (!stack->drawable->mesh) {
					stack->drawable->mesh = new_mesh();
					mesh_from_path(stack->drawable->mesh, stack->drawable->path);
				}
				compositor_3d_draw_bitmap(stack->drawable, &ctx->aspect, tr_state, 0, 0, FIX_ONE, FIX_ONE);
				ctx->drawable = NULL;
			} else
#endif
			{
				drawable_finalize_sort(ctx, tr_state, NULL);
			}

			if (restore_mx) gf_mx2d_copy(tr_state->transform, mx_bck);
			compositor_svg_restore_parent_transformation(tr_state, &backup_matrix, &mx_3d);
		}
	}
	if (stack->audio) svg_traverse_audio_ex(stack->audio, rs, GF_FALSE, tr_state->svg_props);

	memcpy(tr_state->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
	tr_state->svg_flags = backup_flags;
}

/*********************/
/* SVG image element */
/*********************/

static void SVG_Update_image(GF_TextureHandler *txh)
{
	MFURL *txurl = &(((SVG_video_stack *)gf_node_get_private(txh->owner))->txurl);

	/*setup texture if needed*/
	if (!txh->is_open && txurl->count) {
		gf_sc_texture_play_from_to(txh, txurl, 0, -1, GF_FALSE, GF_FALSE);
	}

	gf_sc_texture_update_frame(txh, GF_FALSE);
	/*URL is present but not opened - redraw till fetch*/
	if (txh->stream && !txh->stream_finished && (!txh->tx_io || txh->needs_refresh) ) {
		/*mark all subtrees using this image as dirty*/
		gf_node_dirty_parents(txh->owner);
		gf_sc_invalidate(txh->compositor, NULL);
	}
}

static void svg_traverse_image(GF_Node *node, void *rs, Bool is_destroy)
{
	svg_traverse_bitmap(node, rs, is_destroy);
}

void compositor_init_svg_image(GF_Compositor *compositor, GF_Node *node)
{
	SVG_video_stack *stack;
	GF_SAFEALLOC(stack, SVG_video_stack)
	if (!stack) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate svg image stack\n"));
		return;
	}
	stack->drawable = drawable_new();
	stack->drawable->flags = DRAWABLE_USE_TRAVERSE_DRAW;
	stack->drawable->node = node;

	gf_sc_texture_setup(&stack->txh, compositor, node);
	stack->txh.update_texture_fcnt = SVG_Update_image;
	stack->txh.flags = GF_SR_TEXTURE_SVG;

	/*force first processing of xlink-href*/
	gf_node_dirty_set(node, GF_SG_SVG_XLINK_HREF_DIRTY, GF_FALSE);

	gf_node_set_private(node, stack);
	gf_node_set_callback_function(node, svg_traverse_image);
}

/*********************/
/* SVG video element */
/*********************/
static void SVG_Update_video(GF_TextureHandler *txh)
{
	GF_FieldInfo init_vis_info;
	SVG_video_stack *stack = (SVG_video_stack *) gf_node_get_private(txh->owner);

	if (!txh->stream) {
		svg_open_texture(stack);

		if (!txh->is_open) {
			SVG_InitialVisibility init_vis;
			if (stack->first_frame_fetched) return;

			init_vis = SVG_INITIALVISIBILTY_WHENSTARTED;

			if (gf_node_get_attribute_by_tag(txh->owner, TAG_SVG_ATT_initialVisibility, GF_FALSE, GF_FALSE, &init_vis_info) == GF_OK) {
				init_vis = *(SVG_InitialVisibility *)init_vis_info.far_ptr;
			}

			/*opens stream only at first access to fetch first frame if needed*/
			if (init_vis == SVG_INITIALVISIBILTY_ALWAYS) {
				svg_play_texture((SVG_video_stack*)stack, NULL);
				gf_sc_invalidate(txh->compositor, NULL);
			}
		}
		return;
	}

	/*when fetching the first frame disable resync*/
	gf_sc_texture_update_frame(txh, GF_FALSE);

	/* only when needs_refresh = 1, first frame is fetched */
	if (!stack->first_frame_fetched) {
		if (txh->needs_refresh) {
			stack->first_frame_fetched = GF_TRUE;
			/*stop stream if needed*/
			if (!gf_smil_timing_is_active(txh->owner)) {
				gf_sc_texture_stop_no_unregister(txh);
				//make sure the refresh flag is not cleared
				txh->needs_refresh = GF_TRUE;
			}
		}
	}

	if (!stack->audio && stack->audio_dirty) {
		u32 res = gf_mo_has_audio(stack->txh.stream);
		if (res != 2) {
			stack->audio_dirty = GF_FALSE;
			if (res) {
				GF_FieldInfo att_vid, att_aud;
				stack->audio = gf_node_new(gf_node_get_graph(stack->txh.owner), TAG_SVG_audio);
				gf_node_register(stack->audio, NULL);
				if (gf_node_get_attribute_by_tag(stack->txh.owner, TAG_XLINK_ATT_href, GF_FALSE, GF_FALSE, &att_vid)==GF_OK) {
					gf_node_get_attribute_by_tag(stack->audio, TAG_XLINK_ATT_href, GF_TRUE, GF_FALSE, &att_aud);
					gf_svg_attributes_copy(&att_aud, &att_vid, GF_FALSE);
				}
				/*BYPASS SMIL TIMING MODULE!!*/
				compositor_init_svg_audio(stack->txh.compositor, stack->audio, GF_TRUE);
			}
		}
	}

	/*we have no choice but retraversing the drawable until we're inactive since the movie framerate and
	the compositor framerate are likely to be different */
	if (!txh->stream_finished)
		if (txh->needs_refresh)
			gf_sc_invalidate(txh->compositor, NULL);

	if (stack->stop_requested) {
		stack->stop_requested = GF_FALSE;
		gf_sc_texture_stop_no_unregister(&stack->txh);
	}
}

static void svg_video_smil_evaluate(SMIL_Timing_RTI *rti, Fixed normalized_scene_time, GF_SGSMILTimingEvalState status)
{
	SVG_video_stack *stack = (SVG_video_stack *)gf_node_get_private(gf_smil_get_element(rti));

	switch (status) {
	case SMIL_TIMING_EVAL_UPDATE:
		if (!stack->txh.is_open) {
			if (stack->txurl.count) {
				svg_play_texture((SVG_video_stack*)stack, NULL);
			}
		}
		else if (stack->txh.stream_finished && (gf_smil_get_media_duration(rti)<0) ) {
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
		stack->stop_requested = GF_TRUE;
		break;
	case SMIL_TIMING_EVAL_REPEAT:
		gf_sc_texture_restart(&stack->txh);
		break;
	default:
		break;
	}
	if (stack->audio) svg_audio_smil_evaluate_ex(rti, normalized_scene_time, status, stack->audio, stack->txh.owner);
}

static void svg_traverse_video(GF_Node *node, void *rs, Bool is_destroy)
{
	svg_traverse_bitmap(node, rs, is_destroy);
}

void compositor_init_svg_video(GF_Compositor *compositor, GF_Node *node)
{
	SVG_video_stack *stack;
	GF_SAFEALLOC(stack, SVG_video_stack)
	if (!stack) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate svg video stack\n"));
		return;
	}
	stack->drawable = drawable_new();
	stack->drawable->flags = DRAWABLE_USE_TRAVERSE_DRAW;
	stack->drawable->node = node;

	gf_sc_texture_setup(&stack->txh, compositor, node);
	stack->txh.update_texture_fcnt = SVG_Update_video;
	stack->txh.flags = GF_SR_TEXTURE_SVG;

	/*force first processing of xlink-href*/
	gf_node_dirty_set(node, GF_SG_SVG_XLINK_HREF_DIRTY, GF_FALSE);

	gf_smil_set_evaluation_callback(node, svg_video_smil_evaluate);

	gf_node_set_private(node, stack);
	gf_node_set_callback_function(node, svg_traverse_video);
}

void svg_pause_video(GF_Node *n, Bool pause)
{
	SVG_video_stack *st = (SVG_video_stack *)gf_node_get_private(n);
	if (!st) return;
	if (pause) gf_mo_pause(st->txh.stream);
	else gf_mo_resume(st->txh.stream);
}

void compositor_svg_video_modified(GF_Compositor *compositor, GF_Node *node)
{
	/*if href has been modified, stop the video (and associated audio if any) right away - we cannot wait for next traversal to
	process this as the video could be in a hidden subtree not traversed*/
	if (gf_node_dirty_get(node) & GF_SG_SVG_XLINK_HREF_DIRTY) {
		SVG_video_stack *st = (SVG_video_stack *)gf_node_get_private(node);
		/*WARNING - stack may be NULL at this point when inserting the video from script*/
		if (st && st->txh.is_open) {
			if (st->audio) {
				svg_audio_smil_evaluate_ex(NULL, 0, SMIL_TIMING_EVAL_REMOVE, st->audio, st->txh.owner);
				gf_node_unregister(st->audio, NULL);
				st->audio = NULL;
			}
			/*reset cached URL to avoid reopening the resource in the smil timing callback*/
			gf_sg_vrml_mf_reset(&st->txurl, GF_SG_VRML_MFURL);
			gf_sc_texture_stop(&st->txh);
		}
	}
	gf_node_dirty_set(node, 0, GF_FALSE);
	/*and force a redraw of next frame*/
	gf_sc_next_frame_state(compositor, GF_SC_DRAW_FRAME);
}


/*********************/
/* SVG audio element */
/*********************/

static void svg_audio_smil_evaluate_ex(SMIL_Timing_RTI *rti, Fixed normalized_scene_time, u32 status, GF_Node *slave_audio, GF_Node *video)
{
	GF_Node *audio;
	SVG_audio_stack *stack;

	audio = slave_audio;
	if (!audio) audio = gf_smil_get_element(rti);

	stack = (SVG_audio_stack *)gf_node_get_private(audio);

	switch (status) {
	case SMIL_TIMING_EVAL_UPDATE:
		if (!stack->is_active && !stack->is_error) {
			if (stack->aurl.count) {
				SVGAllAttributes atts;
				Bool lock_timeline = GF_FALSE;
				gf_svg_flatten_attributes((SVG_Element*) (video ? video : audio), &atts);

				if (atts.syncBehavior) lock_timeline = (*atts.syncBehavior == SMIL_SYNCBEHAVIOR_LOCKED) ? GF_TRUE : GF_FALSE;

				if (gf_sc_audio_open(&stack->input, &stack->aurl,
				                     atts.clipBegin ? (*atts.clipBegin) : 0.0,
				                     atts.clipEnd ? (*atts.clipEnd) : -1.0,
				                     lock_timeline) == GF_OK)
				{
					gf_mo_set_speed(stack->input.stream, FIX_ONE);
					stack->is_active = GF_TRUE;
				} else {
					stack->is_error = GF_TRUE;
				}
			}
		}
		else if (!slave_audio && stack->input.stream_finished && (gf_smil_get_media_duration(rti) < 0) ) {
			Double dur = gf_mo_get_duration(stack->input.stream);
			if (dur <= 0) {
				dur = stack->input.stream ? stack->input.stream->timestamp : 0;
				dur /= 1000;
			}
			gf_smil_set_media_duration(rti, dur);
		}
		break;
	case SMIL_TIMING_EVAL_REPEAT:
		if (stack->is_active)
			gf_sc_audio_restart(&stack->input);
		break;
	case SMIL_TIMING_EVAL_FREEZE:
		gf_sc_audio_stop(&stack->input);
		stack->is_active = GF_FALSE;
		break;
	case SMIL_TIMING_EVAL_REMOVE:
		gf_sc_audio_stop(&stack->input);
		stack->is_active = GF_FALSE;
		break;
	case SMIL_TIMING_EVAL_DEACTIVATE:
		if (stack->is_active) {
			gf_sc_audio_stop(&stack->input);
			gf_sc_audio_unregister(&stack->input);
			stack->is_active = GF_FALSE;
		}
		break;
	}
}

static void svg_audio_smil_evaluate(SMIL_Timing_RTI *rti, Fixed normalized_scene_time, GF_SGSMILTimingEvalState status)
{
	svg_audio_smil_evaluate_ex(rti, normalized_scene_time, status, NULL, NULL);
}


static void svg_traverse_audio_ex(GF_Node *node, void *rs, Bool is_destroy, SVGPropertiesPointers *props)
{
	SVGAllAttributes all_atts;
	SVGPropertiesPointers backup_props;
	u32 backup_flags, restore;
	GF_TraverseState *tr_state = (GF_TraverseState*)rs;
	SVG_audio_stack *stack = (SVG_audio_stack *)gf_node_get_private(node);

	if (is_destroy) {
		gf_sc_audio_predestroy(&stack->input);
		gf_sg_mfurl_del(stack->aurl);
		gf_free(stack);
		return;
	}
	if (stack->is_active) {
		gf_sc_audio_register(&stack->input, (GF_TraverseState*)rs);
	}

	restore = 0;
	if (!props) {
		restore = 1;
		gf_svg_flatten_attributes((SVG_Element *)node, &all_atts);
		if (!compositor_svg_traverse_base(node, &all_atts, (GF_TraverseState *)rs, &backup_props, &backup_flags))
			return;
		props = tr_state->svg_props;
	}

	if (gf_node_dirty_get(node) & GF_SG_SVG_XLINK_HREF_DIRTY) {
		SVGAllAttributes atts;
		Bool lock_timeline = GF_FALSE;
		if (stack->is_active)
			gf_sc_audio_stop(&stack->input);

		stack->is_error = GF_FALSE;

		gf_node_dirty_clear(node, GF_SG_SVG_XLINK_HREF_DIRTY);
		gf_sc_get_mfurl_from_xlink(node, &(stack->aurl));

		gf_svg_flatten_attributes((SVG_Element*) node, &atts);
		if (atts.syncBehavior) lock_timeline = (*atts.syncBehavior == SMIL_SYNCBEHAVIOR_LOCKED) ? GF_TRUE : GF_FALSE;

		if (stack->aurl.count && (gf_sc_audio_open(&stack->input, &stack->aurl,
		                          atts.clipBegin ? (*atts.clipBegin) : 0.0,
		                          atts.clipEnd ? (*atts.clipEnd) : -1.0,
		                          lock_timeline) == GF_OK)

		   ) {
			gf_mo_set_speed(stack->input.stream, FIX_ONE);
			stack->is_active = GF_TRUE;
		} else if (stack->is_active) {
			gf_sc_audio_unregister(&stack->input);
			stack->is_active = GF_FALSE;
		}
	}

	/*store mute flag*/
	stack->input.is_muted = GF_FALSE;
	if (tr_state->switched_off
	        || compositor_svg_is_display_off(props)
	        || (*(props->visibility) == SVG_VISIBILITY_HIDDEN) ) {

		stack->input.is_muted = GF_TRUE;
	}

	stack->input.intensity = tr_state->svg_props->computed_audio_level;

	if (restore) {
		memcpy(tr_state->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
		tr_state->svg_flags = backup_flags;
	}
}
static void svg_traverse_audio(GF_Node *node, void *rs, Bool is_destroy)
{
	svg_traverse_audio_ex(node, rs, is_destroy, NULL);
}

void compositor_init_svg_audio(GF_Compositor *compositor, GF_Node *node, Bool slaved_timing)
{
	SVG_audio_stack *stack;
	GF_SAFEALLOC(stack, SVG_audio_stack)
	if (!stack) return;
	gf_sc_audio_setup(&stack->input, compositor, node);

	/*force first processing of xlink-href*/
	gf_node_dirty_set(node, GF_SG_SVG_XLINK_HREF_DIRTY, GF_FALSE);

	if (!slaved_timing)
		gf_smil_set_evaluation_callback(node, svg_audio_smil_evaluate);

	gf_node_set_private(node, stack);
	gf_node_set_callback_function(node, svg_traverse_audio);
}

void svg_pause_audio(GF_Node *n, Bool pause)
{
	SVG_audio_stack *st = (SVG_audio_stack *)gf_node_get_private(n);
	if (!st) return;
	if (pause) gf_mo_pause(st->input.stream);
	else gf_mo_resume(st->input.stream);
}

GF_TextureHandler *compositor_svg_get_image_texture(GF_Node *node)
{
	SVG_video_stack *st = (SVG_video_stack *) gf_node_get_private(node);
	return &(st->txh);
}




typedef struct
{
	/*media stream*/
	GF_MediaObject *resource;
	Bool stop_requested, is_open;
	Double clipBegin, clipEnd;
} SVG_updates_stack;

static void svg_updates_smil_evaluate(SMIL_Timing_RTI *rti, Fixed normalized_scene_time, GF_SGSMILTimingEvalState status)
{
	SVG_updates_stack *stack = (SVG_updates_stack *)gf_node_get_private(gf_smil_get_element(rti));

	switch (status) {
	case SMIL_TIMING_EVAL_UPDATE:
		if (!stack->is_open) {
			if (stack->resource ) gf_mo_play(stack->resource, stack->clipBegin, stack->clipEnd, GF_FALSE);
			stack->is_open = GF_TRUE;
		}
		else if (gf_mo_is_done(stack->resource) && (gf_smil_get_media_duration(rti)<0) ) {
			Double dur = gf_mo_get_duration(stack->resource);
			gf_smil_set_media_duration(rti, dur);
		}
		break;
	case SMIL_TIMING_EVAL_FREEZE:
	case SMIL_TIMING_EVAL_REMOVE:
		stack->is_open = GF_FALSE;
		gf_mo_set_flag(stack->resource, GF_MO_DISPLAY_REMOVE, GF_TRUE);
		gf_mo_stop(&stack->resource);
		break;
	case SMIL_TIMING_EVAL_REPEAT:
		gf_mo_restart(stack->resource);
		break;
	default:
		break;
	}
}

static void svg_traverse_updates(GF_Node *node, void *rs, Bool is_destroy)
{
	/*video stack is just an extension of image stack, type-casting is OK*/
	SVG_updates_stack *stack = (SVG_updates_stack*)gf_node_get_private(node);
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;
	SVGAllAttributes all_atts;
	SVGPropertiesPointers backup_props;
	u32 backup_flags, dirty_flags;

	if (is_destroy) {
		if (stack->resource) {
			if (stack->is_open) {
				gf_mo_set_flag(stack->resource, GF_MO_DISPLAY_REMOVE, GF_TRUE);
				gf_mo_stop(&stack->resource);
			}
			gf_mo_unregister(node, stack->resource);
		}
		gf_free(stack);
		return;
	}

	if (tr_state->traversing_mode!=TRAVERSE_SORT) return;

	/*flatten attributes and apply animations + inheritance*/
	gf_svg_flatten_attributes((SVG_Element *)node, &all_atts);
	if (!compositor_svg_traverse_base(node, &all_atts, (GF_TraverseState *)rs, &backup_props, &backup_flags))
		return;

	dirty_flags = gf_node_dirty_get(node);
	if (dirty_flags) {
		stack->clipBegin = all_atts.clipBegin ? *all_atts.clipBegin : 0;
		stack->clipEnd = all_atts.clipEnd ? *all_atts.clipEnd : -1;
		if (dirty_flags & GF_SG_SVG_XLINK_HREF_DIRTY) {
			GF_MediaObject *new_res;
			MFURL url;
			Bool lock_timeline=GF_FALSE;
			url.vals = NULL;
			url.count = 0;

			if (all_atts.syncBehavior) lock_timeline = (*all_atts.syncBehavior == SMIL_SYNCBEHAVIOR_LOCKED) ? GF_TRUE : GF_FALSE;

			gf_sc_get_mfurl_from_xlink(node, &url);

			new_res = gf_mo_register(node, &url, lock_timeline, GF_FALSE);
			gf_sg_mfurl_del(url);

			if (stack->resource!=new_res) {
				if (stack->resource) {
					gf_mo_stop(&stack->resource);
					gf_mo_unregister(node, stack->resource);
				}
				stack->resource = new_res;
				if (stack->resource && stack->is_open) gf_mo_play(stack->resource, stack->clipBegin, stack->clipEnd, GF_FALSE);
			}
		}
		gf_node_dirty_clear(node, 0);
	}
	memcpy(tr_state->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
	tr_state->svg_flags = backup_flags;
}

void compositor_init_svg_updates(GF_Compositor *compositor, GF_Node *node)
{
	SVG_updates_stack *stack;
	GF_SAFEALLOC(stack, SVG_updates_stack)
	if (!stack) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate laser updates stack\n"));
		return;
	}

	/*force first processing of xlink-href*/
	gf_node_dirty_set(node, GF_SG_SVG_XLINK_HREF_DIRTY, GF_FALSE);

	gf_smil_set_evaluation_callback(node, svg_updates_smil_evaluate);

	gf_node_set_private(node, stack);
	gf_node_set_callback_function(node, svg_traverse_updates);
	stack->clipEnd = -1;
}

#endif //!defined(GPAC_DISABLE_SVG) &&  !defined(GPAC_DISABLE_COMPOSITOR)
