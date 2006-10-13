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

#include "svg_stacks.h"
#include "visualsurface2d.h"

#ifndef GPAC_DISABLE_SVG

#include <gpac/internal/scenegraph_dev.h>

#if 0
static void SVG_ComputeAR(Fixed objw, Fixed objh, Fixed viewportw, Fixed viewporth,
						  SVG_PreserveAspectRatio *par, GF_Matrix2D *par_mat)
{
	gf_mx2d_init(*par_mat);
	if (par->meetOrSlice == SVG_MEETORSLICE_MEET) {
		switch(par->align) {
			case SVG_PRESERVEASPECTRATIO_NONE:
				break;
			case SVG_PRESERVEASPECTRATIO_XMINYMIN:
				break;
			case SVG_PRESERVEASPECTRATIO_XMIDYMIN:
				break;
			case SVG_PRESERVEASPECTRATIO_XMAXYMIN:
				break;
			case SVG_PRESERVEASPECTRATIO_XMINYMID:
				break;
			case SVG_PRESERVEASPECTRATIO_XMIDYMID:
				break;
			case SVG_PRESERVEASPECTRATIO_XMAXYMID:
				break;
			case SVG_PRESERVEASPECTRATIO_XMINYMAX:
				break;
			case SVG_PRESERVEASPECTRATIO_XMIDYMAX:
				break;
			case SVG_PRESERVEASPECTRATIO_XMAXYMAX:
				break;
		}
	} else {
	}
}
#endif
static void SVG_Draw_bitmap(DrawableContext *ctx)
{
	GF_ColorMatrix *cmat;
	u8 alpha;
	Render2D *sr;
	Bool use_blit;
	sr = ctx->surface->render;

	use_blit = 1;
	alpha = GF_COL_A(ctx->aspect.fill_color);

	cmat = NULL;
	if (!ctx->cmat.identity) cmat = &ctx->cmat;
	//else if (ctx->h_texture->has_cmat) cmat = NULL;

	/*this is not a native texture, use graphics*/
	if (!ctx->h_texture->data || ctx->transform.m[1] || ctx->transform.m[3]) {
		use_blit = 0;
	} else {
		if (!ctx->surface->SupportsFormat || !ctx->surface->DrawBitmap ) use_blit = 0;
		/*format not supported directly, try with brush*/
		else if (!ctx->surface->SupportsFormat(ctx->surface, ctx->h_texture->pixelformat) ) use_blit = 0;
	}

	/*no HW, fall back to the graphics driver*/
	if (!use_blit) {
		VS2D_TexturePath(ctx->surface, ctx->node->path, ctx);
		return;
	}

	/*direct rendering, render without clippers */
	if (ctx->surface->render->top_effect->trav_flags & TF_RENDER_DIRECT) {
		ctx->surface->DrawBitmap(ctx->surface, ctx->h_texture, &ctx->clip, &ctx->unclip, alpha, NULL, cmat);
	}
	/*render bitmap for all dirty rects*/
	else {
		u32 i;
		GF_IRect clip;
		for (i=0; i<ctx->surface->to_redraw.count; i++) {
			/*there's an opaque region above, don't draw*/
			if (ctx->surface->draw_node_index<ctx->surface->to_redraw.opaque_node_index[i]) continue;
			clip = ctx->clip;
			gf_irect_intersect(&clip, &ctx->surface->to_redraw.list[i]);
			if (clip.width && clip.height) {
				ctx->surface->DrawBitmap(ctx->surface, ctx->h_texture, &clip, &ctx->unclip, alpha, NULL, cmat);
			}
		}
	}
}

static void SVG_BuildGraph_image(SVG_image_stack *st)
{
	GF_Rect rc, new_rc;
	SVGimageElement *img = (SVGimageElement *)st->graph->owner;
	gf_path_get_bounds(st->graph->path, &rc);
	drawable_reset_path(st->graph);
	gf_path_add_rect_center(st->graph->path, img->x.value+img->width.value/2, img->y.value+img->height.value/2, img->width.value, img->height.value);
	gf_path_get_bounds(st->graph->path, &new_rc);
	/*change in visual aspect*/
	if (!gf_rect_equal(rc, new_rc)) st->graph->node_changed = 1;
	gf_node_dirty_clear(st->graph->owner, GF_SG_SVG_GEOMETRY_DIRTY);
}


static void SVG_BuildGraph_video(SVG_video_stack *st)
{
	GF_Rect rc, new_rc;
	SVGvideoElement *video = (SVGvideoElement *)st->graph->owner;
	gf_path_get_bounds(st->graph->path, &rc);
	drawable_reset_path(st->graph);
	gf_path_add_rect_center(st->graph->path, video->x.value+video->width.value/2, video->y.value+video->height.value/2, video->width.value, video->height.value);
	gf_path_get_bounds(st->graph->path, &new_rc);
	/*change in visual aspect*/
	if (!gf_rect_equal(rc, new_rc)) st->graph->node_changed = 1;
	gf_node_dirty_clear(st->graph->owner, GF_SG_SVG_GEOMETRY_DIRTY);
}

static void SVG_Render_bitmap(GF_Node *node, void *rs)
{
	/*video stack is just an extension of image stack, type-casting is OK*/
	SVG_image_stack *st = (SVG_image_stack*)gf_node_get_private(node);
	RenderEffect2D *eff = (RenderEffect2D *)rs;
	SVGPropertiesPointers backup_props;
	GF_Matrix2D backup_matrix;
	SVG_Matrix *m;
	DrawableContext *ctx;

	SVG_Render_base(node, (RenderEffect2D *)rs, &backup_props);

	if (gf_node_dirty_get(node)) {
		if (gf_node_get_tag(node)==TAG_SVG_image) {
			SVG_BuildGraph_image(gf_node_get_private(node));
			m = &((SVGimageElement *)node)->transform;
		} else {
			SVG_BuildGraph_video(gf_node_get_private(node));
			m = &((SVGvideoElement *)node)->transform;
		}

		/*if open and changed, stop and play*/
		if (gf_svg_check_url_change(&st->txurl, & ((SVGElement *)node)->xlink->href)) {
			const char *cache_dir = gf_cfg_get_key(st->txh.compositor->user->config, "General", "CacheDirectory");
			gf_svg_store_embedded_data(& ((SVGElement *)node)->xlink->href, cache_dir, "embedded_");

			if (gf_svg_check_url_change(&st->txurl, & ((SVGElement *)node)->xlink->href)) {
				gf_term_set_mfurl_from_uri(st->txh.compositor->term, &(st->txurl), & ((SVGElement*)node)->xlink->href);
				if (st->txh.is_open) gf_sr_texture_stop(&st->txh);
				fprintf(stdout, "URL changed to %s\n", st->txurl.vals[0].url);
				gf_sr_texture_play(&st->txh, &st->txurl);
			}
		} 
		gf_node_dirty_clear(node, 0);
	} else {
		if (gf_node_get_tag(node)==TAG_SVG_image) {
			m = &((SVGimageElement *)node)->transform;
		} else {
			m = &((SVGvideoElement *)node)->transform;
		}
	}

	/*FIXME: setup aspect ratio*/

	if (eff->trav_flags & TF_RENDER_GET_BOUNDS) {
		gf_mx2d_pre_multiply(&eff->transform, m);
		if (*(eff->svg_props->display) != SVG_DISPLAY_NONE) {
			gf_path_get_bounds(st->graph->path, &eff->bounds);
		}
		memcpy(eff->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
		return;
	}

	if (*(eff->svg_props->display) == SVG_DISPLAY_NONE ||
		*(eff->svg_props->visibility) == SVG_VISIBILITY_HIDDEN) {
		memcpy(eff->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
		return;
	}

	gf_mx2d_copy(backup_matrix, eff->transform);
	gf_mx2d_pre_multiply(&eff->transform, m);

	ctx = SVG_drawable_init_context(st->graph, eff);
	if (!ctx || !ctx->h_texture ) return;
	/*store bounds*/
	gf_path_get_bounds(ctx->node->path, &ctx->original);

	/*even if set this is not true*/
	ctx->aspect.has_line = 0;
	/*this is to make sure we don't fill the path if the texture is transparent*/
	ctx->aspect.filled = 0;
	ctx->aspect.pen_props.width = 0;
	ctx->no_antialias = 1;

	/*if rotation, transparent*/
	ctx->transparent = 0;
	if (ctx->transform.m[1] || ctx->transform.m[3]) {
		ctx->transparent = 1;
		ctx->no_antialias = 0;
	}
	else if (ctx->h_texture->transparent) 
		ctx->transparent = 1;
	else if (eff->svg_props->opacity && (eff->svg_props->opacity->type==SVG_NUMBER_VALUE) && (eff->svg_props->opacity->value!=FIX_ONE)) {
		ctx->transparent = 1;
		ctx->aspect.fill_alpha = FIX2INT(0xFF * eff->svg_props->opacity->value);
		ctx->aspect.fill_color = ctx->aspect.fill_alpha << 24;
	}

	/*bounds are stored when building graph*/	
	drawable_finalize_render(ctx, eff);
	gf_mx2d_copy(eff->transform, backup_matrix);  
	memcpy(eff->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
}

static Bool SVG_PointOver_bitmap(DrawableContext *ctx, Fixed x, Fixed y, u32 check_type)
{
	return 1;
}


/*********************/
/* SVG image element */
/*********************/

static void SVG_Update_image(GF_TextureHandler *txh)
{
	SVGimageElement *txnode = (SVGimageElement *) txh->owner;
	MFURL *txurl = &(((SVG_image_stack *)gf_node_get_private((GF_Node *)txnode))->txurl);

	/*setup texture if needed*/
	if (!txh->is_open && txurl->count) {
		gf_sr_texture_play(txh, txurl);
	}
	gf_sr_texture_update_frame(txh, 0);
	/*URL is present but not opened - redraw till fetch*/
	if (txh->stream && !txh->hwtx) gf_sr_invalidate(txh->compositor, NULL);
}

static void SVG_Destroy_image(GF_Node *node)
{
	SVG_image_stack *st = (SVG_image_stack *)gf_node_get_private(node);

	gf_sr_texture_destroy(&st->txh);
	gf_sg_mfurl_del(st->txurl);

	drawable_del(st->graph);
	free(st);
}

void SVG_Init_image(Render2D *sr, GF_Node *node)
{
	SVG_image_stack *st;
	GF_SAFEALLOC(st, SVG_image_stack)
	st->graph = drawable_new();

	gf_sr_traversable_setup(st->graph, node, sr->compositor);
	st->graph->Draw = SVG_Draw_bitmap;
	st->graph->IsPointOver = SVG_PointOver_bitmap;

	gf_sr_texture_setup(&st->txh, sr->compositor, node);
	st->txh.update_texture_fcnt = SVG_Update_image;
	st->txh.flags = 0;

	/* builds the MFURL to be used by the texture */
	gf_term_set_mfurl_from_uri(sr->compositor->term, &(st->txurl), & ((SVGimageElement*)node)->xlink->href);

	gf_node_set_private(node, st);
	gf_node_set_render_function(node, SVG_Render_bitmap);
	gf_node_set_predestroy_function(node, SVG_Destroy_image);

}

/*********************/
/* SVG video element */
/*********************/
static void SVG_Update_video(GF_TextureHandler *txh)
{
	SVG_video_stack *st = (SVG_video_stack *) gf_node_get_private(txh->owner);

	if (!txh->is_open) {
		/*opens stream only at first access to fetch first frame if needed*/
		if (!st->first_frame_fetched &&
			(((SVGvideoElement *)txh->owner)->initialVisibility == SVG_INITIALVISIBILTY_ALWAYS)) {
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
	case SMIL_TIMING_EVAL_RESTART:
		gf_sr_texture_restart(&stack->txh);
		break;
	}
}

static void SVG_Destroy_video(GF_Node *node)
{
	SVG_video_stack *st = (SVG_video_stack *)gf_node_get_private(node);
	gf_sr_texture_destroy(&st->txh);
	gf_sg_mfurl_del(st->txurl);

	drawable_del(st->graph);
	free(st);
}

void SVG_Init_video(Render2D *sr, GF_Node *node)
{
	SVG_video_stack *st;
	GF_SAFEALLOC(st, SVG_video_stack)
	st->graph = drawable_new();

	gf_sr_traversable_setup(st->graph, node, sr->compositor);
	st->graph->Draw = SVG_Draw_bitmap;
	st->graph->IsPointOver = SVG_PointOver_bitmap;

	gf_sr_texture_setup(&st->txh, sr->compositor, node);
	st->txh.update_texture_fcnt = SVG_Update_video;
	st->txh.flags = 0;

	/* create an MFURL from the SVG iri */
	gf_term_set_mfurl_from_uri(sr->compositor->term, &(st->txurl), & ((SVGvideoElement *)node)->xlink->href);

	gf_smil_timing_init_runtime_info((SVGElement *)node);
	if (((SVGElement *)node)->timing->runtime) {
		SMIL_Timing_RTI *rti = ((SVGElement *)node)->timing->runtime;
		rti->evaluate = svg_video_smil_evaluate;
	}
	
	gf_node_set_private(node, st);
	gf_node_set_render_function(node, SVG_Render_bitmap);
	gf_node_set_predestroy_function(node, SVG_Destroy_video);


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
	case SMIL_TIMING_EVAL_RESTART:
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

static void SVG_Render_audio(GF_Node *node, void *rs)
{
	SVGPropertiesPointers backup_props;
	RenderEffect2D *eff = (RenderEffect2D*)rs;
	SVG_audio_stack *st = (SVG_audio_stack *)gf_node_get_private(node);

	if (st->is_active) {
		gf_sr_audio_register(&st->input, (GF_BaseEffect*)rs);
	}

	/*for heritage and anims*/
	SVG_Render_base(node, (RenderEffect2D *)rs, &backup_props);

	/*store mute flag*/
	st->input.is_muted = 0;
	if ((eff->trav_flags & GF_SR_TRAV_SWITCHED_OFF)
		|| (*(eff->svg_props->display) == SVG_DISPLAY_NONE) 
		|| (*(eff->svg_props->visibility) == SVG_VISIBILITY_HIDDEN) ) {
	
		st->input.is_muted = 1;
	}

	memcpy(eff->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
}

static void SVG_Destroy_audio(GF_Node *node)
{
	SVG_audio_stack *st = (SVG_audio_stack *)gf_node_get_private(node);
	gf_sr_audio_stop(&st->input);
	gf_sr_audio_unregister(&st->input);
	gf_sg_mfurl_del(st->aurl);
	free(st);
}

void SVG_Init_audio(Render2D *sr, GF_Node *node)
{
	SVG_audio_stack *st;
	GF_SAFEALLOC(st, SVG_audio_stack)

	gf_sr_audio_setup(&st->input, sr->compositor, node);

	/* creates an MFURL from the URI of the SVG element */
	gf_term_set_mfurl_from_uri(sr->compositor->term, &(st->aurl), & ((SVGaudioElement *)node)->xlink->href);

	gf_smil_timing_init_runtime_info((SVGElement *)node);
	if (((SVGElement *)node)->timing->runtime) {
		SMIL_Timing_RTI *rti = ((SVGElement *)node)->timing->runtime;
		rti->evaluate = svg_audio_smil_evaluate;
	}
	
	gf_node_set_private(node, st);
	gf_node_set_render_function(node, SVG_Render_audio);
	gf_node_set_predestroy_function(node, SVG_Destroy_audio);
}



void R2D_RenderUse(GF_Node *node, GF_Node *sub_root, void *rs)
{
	GF_Matrix2D backup_matrix;
  	GF_Matrix2D translate;
	GF_Node *prev_use;
	SVGuseElement *use = (SVGuseElement *)node;
	SVGPropertiesPointers backup_props;
	RenderEffect2D *eff = rs;
	SVG_Render_base(node, (RenderEffect2D *)rs, &backup_props);

	gf_mx2d_init(translate);
	translate.m[2] = use->x.value;
	translate.m[5] = use->y.value;

	if (eff->trav_flags & TF_RENDER_GET_BOUNDS) {
		gf_svg_apply_local_transformation(eff, node, &backup_matrix);
		if (*(eff->svg_props->display) != SVG_DISPLAY_NONE) {
			gf_node_render(sub_root, eff);
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
	eff->parent_use = (GF_Node *)sub_root;
	gf_node_render(sub_root, eff);
	eff->parent_use = prev_use;
	gf_svg_restore_parent_transformation(eff, &backup_matrix);  

end:
	memcpy(eff->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
}


void R2D_RenderInlineAnimation(GF_Node *anim, GF_Node *sub_root, void *rs)
{
	GF_Matrix2D backup_matrix;
	SVGPropertiesPointers backup_props;
	RenderEffect2D *eff = (RenderEffect2D*)rs;
	SVGanimationElement *a = (SVGanimationElement*)anim;
  	GF_Matrix2D translate;
	SVGPropertiesPointers new_props, *old_props;

	memset(&new_props, 0, sizeof(SVGPropertiesPointers));

	/*for heritage and anims*/
	SVG_Render_base(anim, (RenderEffect2D *)rs, &backup_props);


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
	memcpy(eff->svg_props, &backup_props, sizeof(SVGPropertiesPointers));

end:
	memcpy(eff->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
}

#endif //GPAC_DISABLE_SVG

