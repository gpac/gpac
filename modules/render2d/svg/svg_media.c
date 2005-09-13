/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Cyril Concolato 2004
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
#include "../visualsurface2d.h"

#ifndef GPAC_DISABLE_SVG

#include <gpac/internal/scenegraph_dev.h>

#ifdef DANAE
int processDanaeAudio(void *param, unsigned int scene_time);
#endif

/************************/
/* Generic URI handling */
/************************/

void SVG_SetMFURLFromURI(MFURL *mfurl, char *uri) 
{
	SFURL *sfurl = NULL;
	mfurl->count = 1;
	GF_SAFEALLOC(mfurl->vals, sizeof(SFURL))
	sfurl = mfurl->vals;
	sfurl->OD_ID = 0;
	sfurl->url = strdup(uri);
}

/****************************************/
/* Generic rendering for bitmaps        */
/* used for SVG image and video element */
/****************************************/
static void SVG_BuildGraph_image(SVG_image_stack *st, DrawableContext *ctx);
static void SVG_BuildGraph_video(SVG_video_stack *st, DrawableContext *ctx);

static void SVG_Draw_bitmap(DrawableContext *ctx)
{
	u8 alpha;
	Render2D *sr;
	Bool use_hw, has_scale;

	sr = ctx->surface->render;

	/* no rotation allowed, remove skew components */
	/* ctx->transform.m[1] = ctx->transform.m[3] = 0; */

	has_scale = 0;
	use_hw = 1;
	alpha = GF_COL_A(ctx->aspect.fill_color);

	/*check if driver can handle alpha blit*/
	if (alpha!=255) {
		use_hw = sr->compositor->video_out->bHasAlpha;
		if (has_scale) use_hw = sr->compositor->video_out->bHasAlphaStretch;
	}

	/*this is not a native texture, use graphics*/
	if (!ctx->h_texture->data) {
		use_hw = 0;
	} else {
		if (!ctx->surface->SupportsFormat || !ctx->surface->DrawBitmap ) use_hw = 0;
		/*format not supported directly, try with brush*/
		else if (!ctx->surface->SupportsFormat(ctx->surface, ctx->h_texture->pixelformat) ) use_hw = 0;
	}

	/*no HW, fall back to the graphics driver*/
	if (!use_hw) {
		switch (gf_node_get_tag(ctx->node->owner) ) {
		case TAG_SVG_image:
			{
				SVG_image_stack *st = (SVG_image_stack *) gf_node_get_private(ctx->node->owner);
//				SVG_BuildGraph_image(st, ctx);
				ctx->no_antialias = 1;
				VS2D_TexturePath(ctx->surface, st->graph->path, ctx);
				return;
			}
			break;
		case TAG_SVG_video:
			{
				SVG_video_stack *st = (SVG_video_stack *) gf_node_get_private(ctx->node->owner);
//				SVG_BuildGraph_video(st, ctx);
				ctx->no_antialias = 1;
				VS2D_TexturePath(ctx->surface, st->graph->path, ctx);
				return;
			}
			break;
		}
	}

	/*direct rendering, render without clippers */
	if (ctx->surface->render->top_effect->trav_flags & TF_RENDER_DIRECT) {
		ctx->surface->DrawBitmap(ctx->surface, ctx->h_texture, &ctx->clip, &ctx->unclip);
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
				ctx->surface->DrawBitmap(ctx->surface, ctx->h_texture, &clip, &ctx->unclip);
			}
		}
	}
}

static void SVG_Render_bitmap(GF_Node *node, void *rs)
{
	GF_Matrix2D backup_matrix;
	SVG_Transform *tr;
	DrawableContext *ctx;
	RenderEffect2D *eff = (RenderEffect2D *)rs;

	switch(gf_node_get_tag(node)) {
	case TAG_SVG_image:
		{
			SVGimageElement *image = (SVGimageElement *)node;
			if (image->display == SVG_DISPLAY_NONE ||
				image->visibility == SVG_VISIBILITY_HIDDEN) return;
		}
		break;
	case TAG_SVG_video:
		{
			SVGvideoElement *video = (SVGvideoElement *)node;
			if (video->display == SVG_DISPLAY_NONE ||
				video->visibility == SVG_VISIBILITY_HIDDEN) return;
		}
		break;
	}

	/*we never cache anything with bitmap...*/
	gf_node_dirty_clear(node, 0);

	gf_mx2d_copy(backup_matrix, eff->transform);

	switch(gf_node_get_tag(node)) {
	case TAG_SVG_image:
		{
			SVG_image_stack *st = (SVG_image_stack *)gf_node_get_private(node);
			SVGimageElement *image = (SVGimageElement *)node;
			tr = gf_list_get(image->transform, 0);
			if (tr) {
				gf_mx2d_copy(eff->transform, tr->matrix);
				gf_mx2d_add_matrix(&eff->transform, &backup_matrix);
			}

			ctx = SVG_drawable_init_context(st->graph, eff);
			if (!ctx || !ctx->h_texture ) return;
			/*always build the path*/
			SVG_BuildGraph_image(st, ctx);
		}
		break;
	case TAG_SVG_video:
		{
			SVG_video_stack *st = (SVG_video_stack *)gf_node_get_private(node);
			SVGvideoElement *video = (SVGvideoElement *)node;
			tr = gf_list_get(video->transform, 0);
			if (tr) {
				gf_mx2d_copy(eff->transform, tr->matrix);
				gf_mx2d_add_matrix(&eff->transform, &backup_matrix);
			}

			ctx = SVG_drawable_init_context(st->graph, eff);
			if (!ctx || !ctx->h_texture ) return;
			/*always build the path*/
			SVG_BuildGraph_video(st, ctx);
		}
		break;
	}
	/*even if set this is not true*/
	ctx->aspect.has_line = 0;
	/*this is to make sure we don't fill the path if the texture is transparent*/
	ctx->aspect.filled = 0;
	ctx->aspect.pen_props.width = 0;

	ctx->no_antialias = 1;

	ctx->transparent = 0;
	/*if clipper then transparent*/

	if (ctx->h_texture->transparent) {
		ctx->transparent = 1;
	}
	/*global transparency is checked independently*/

	/*bounds are stored when building graph*/	
	drawable_finalize_render(ctx, eff);
	gf_mx2d_copy(eff->transform, backup_matrix);  
}

static Bool SVG_PointOver_bitmap(DrawableContext *ctx, Fixed x, Fixed y, Bool check_outline)
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

static void SVG_BuildGraph_image(SVG_image_stack *st, DrawableContext *ctx)
{
	SVGimageElement *img = (SVGimageElement *)st->graph->owner;
	drawable_reset_path(st->graph);
	gf_path_add_rect_center(st->graph->path, img->x.number+img->width.number/2, img->y.number+img->height.number/2, img->width.number, img->height.number);
	gf_path_get_bounds(st->graph->path, &ctx->original);
}

static void SVG_Destroy_image(GF_Node *node)
{
	SVG_image_stack *st = (SVG_image_stack *)gf_node_get_private(node);

	gf_sr_texture_destroy(&st->txh);
	gf_sg_mfurl_del(st->txurl);

	DeleteDrawableNode(st->graph);
	free(st);
}

void SVG_Init_image(Render2D *sr, GF_Node *node)
{
	SVG_image_stack *st;
	GF_SAFEALLOC(st, sizeof(SVG_image_stack))
	st->graph = NewDrawableNode();

	gf_sr_traversable_setup(st->graph, node, sr->compositor);
	st->graph->Draw = SVG_Draw_bitmap;
	st->graph->IsPointOver = SVG_PointOver_bitmap;

	gf_sr_texture_setup(&st->txh, sr->compositor, node);
	st->txh.update_texture_fcnt = SVG_Update_image;
	st->txh.flags = 0;

	/* builds the MFURL to be used by the texture */
	SVG_SetMFURLFromURI(&(st->txurl), ((SVGimageElement*)node)->xlink_href.iri);

	gf_node_set_private(node, st);
	gf_node_set_render_function(node, SVG_Render_bitmap);
	gf_node_set_predestroy_function(node, SVG_Destroy_image);

}

/*********************/
/* SVG video element */
/*********************/

static void SVG_Activate_video(SVG_video_stack *stack, SVGvideoElement *video)
{
	MFURL *txurl = &(((SVG_video_stack *)gf_node_get_private((GF_Node *)video))->txurl);
	stack->isActive = 1;
	if (!stack->txh.is_open) {
		gf_sr_texture_play(&stack->txh, txurl);
	}
	gf_mo_set_speed(stack->txh.stream, FIX_ONE);
}

/*
static void SVG_Deactivate_video(SVG_video_stack *stack, SVGvideoElement *video)
{
	stack->isActive = 0;
	stack->time_handle.needs_unregister = 1;

	if (stack->txh.is_open) {
		gf_sr_texture_stop(&stack->txh);
	}
}
*/

static void SVG_Update_video(GF_TextureHandler *txh)
{
	//SVGvideoElement *txnode = (SVGvideoElement *) txh->owner;
	SVG_video_stack *st = (SVG_video_stack *) gf_node_get_private(txh->owner);
	//MFURL *txurl = &(st->txurl);

	/*setup texture if needed*/
	if (!txh->is_open) return;
	if (!st->isActive && st->first_frame_fetched) return;

	/*when fetching the first frame disable resync*/
	gf_sr_texture_update_frame(txh, 0);

	if (txh->stream_finished) {
		/*
		if (MT_GetLoop(st, txnode)) {
			gf_sr_texture_restart(txh);
		}
		//if active deactivate
		else if (txnode->isActive && gf_mo_should_deactivate(st->txh.stream) ) {
			MT_Deactivate(st, txnode);
		}
		*/
	}
	/* first frame is fetched */
	if (!st->first_frame_fetched && (txh->needs_refresh) ) {
		st->first_frame_fetched = 1;
		/*
		txnode->duration_changed = gf_mo_get_duration(txh->stream);
		gf_node_event_out_str(txh->owner, "duration_changed");
		*/
		/*stop stream if needed
		if (!txnode->isActive && txh->is_open) {
			gf_sr_texture_stop(txh);
			//make sure the refresh flag is not cleared
			txh->needs_refresh = 1;
		}
		*/
	}
	/*we have no choice but retraversing the graph until we're inactive since the movie framerate and
	the renderer framerate are likely to be different */
	if (!txh->stream_finished) 
		gf_sr_invalidate(txh->compositor, NULL);

}

static void SVG_UpdateTime_video(GF_TimeNode *st)
{
	Double sceneTime;
	SVGvideoElement *video = (SVGvideoElement *)st->obj;
	SVG_video_stack *stack = (SVG_video_stack *)gf_node_get_private(st->obj);
	MFURL *txurl = &(stack->txurl);
	
	/*not active, store start time and speed */
	if (!stack->isActive) {
		//stack->start_time = mt->startTime;
		stack->start_time = 0;
	}

	sceneTime = gf_node_get_scene_time(st->obj);

	if (sceneTime < stack->start_time //||
		/*special case if we're getting active AFTER stoptime 
		(!mt->isActive && (mt->stopTime > stack->start_time) && (time>=mt->stopTime)) */
		) {
		/*opens stream only at first access to fetch first frame*/
		if (stack->fetch_first_frame) {
			stack->fetch_first_frame = 0;
			if (!stack->txh.is_open)
				gf_sr_texture_play(&stack->txh, txurl);
		}
		return;
	}

	/*
	if (MT_GetSpeed(stack, mt) && mt->isActive) {
		// if stoptime is reached (>startTime) deactivate
		if ((mt->stopTime > stack->start_time) && (time >= mt->stopTime) ) {
			MT_Deactivate(stack, mt);
			return;
		}
	}
	*/

	if (!stack->isActive) SVG_Activate_video(stack, video);
}

static void SVG_BuildGraph_video(SVG_video_stack *st, DrawableContext *ctx)
{
	SVGvideoElement *video = (SVGvideoElement *)st->graph->owner;
	drawable_reset_path(st->graph);
	gf_path_add_rect_center(st->graph->path, video->x.number+video->width.number/2, video->y.number+video->height.number/2, video->width.number, video->height.number);
	gf_path_get_bounds(st->graph->path, &ctx->original);
}

static void SVG_Destroy_video(GF_Node *node)
{
	SVG_video_stack *st = (SVG_video_stack *)gf_node_get_private(node);
	gf_sr_texture_destroy(&st->txh);
	gf_sg_mfurl_del(st->txurl);

	if (st->time_handle.is_registered) gf_sr_unregister_time_node(st->txh.compositor, &st->time_handle);
	DeleteDrawableNode(st->graph);
	free(st);
}

void SVG_Init_video(Render2D *sr, GF_Node *node)
{
	SVG_video_stack *st;
	GF_SAFEALLOC(st, sizeof(SVG_video_stack))
	st->graph = NewDrawableNode();

	gf_sr_traversable_setup(st->graph, node, sr->compositor);
	st->graph->Draw = SVG_Draw_bitmap;
	st->graph->IsPointOver = SVG_PointOver_bitmap;

	gf_sr_texture_setup(&st->txh, sr->compositor, node);
	st->txh.update_texture_fcnt = SVG_Update_video;
	st->txh.flags = 0;
	st->time_handle.UpdateTimeNode = SVG_UpdateTime_video;
	st->time_handle.obj = node;
	st->fetch_first_frame = 1;
	
	/* create an MFURL from the SVG iri */
	SVG_SetMFURLFromURI(&(st->txurl), ((SVGvideoElement *)node)->xlink_href.iri);

	gf_sr_register_time_node(st->txh.compositor, &st->time_handle);	
	
	gf_node_set_private(node, st);
	gf_node_set_render_function(node, SVG_Render_bitmap);
	gf_node_set_predestroy_function(node, SVG_Destroy_video);
}

/*********************/
/* SVG audio element */
/*********************/

static void SVG_Activate_audio(SVG_audio_stack *st, SVGaudioElement *audio)
{
	MFURL *aurl = &(((SVG_audio_stack *)gf_node_get_private((GF_Node*)audio))->aurl);

	gf_sr_audio_open(&st->input, aurl);
	st->is_active = 1;
	gf_mo_set_speed(st->input.stream, FIX_ONE);
	/*rerender all graph to get parent audio group*/
	gf_sr_invalidate(st->input.compositor, NULL);
}

/*
static void SVG_Deactivate_audio(SVG_audio_stack *st, SVGaudioElement *audio)
{
	gf_sr_audio_stop(&st->input);
	st->is_active = 0;
	st->time_handle.needs_unregister = 1;
}
*/

static void SVG_Render_audio(GF_Node *node, void *rs)
{
	GF_BaseEffect*eff = (GF_BaseEffect*)rs;
	//SVGaudioElement *audio = (SVGaudioElement *)node;
	SVG_audio_stack *st = (SVG_audio_stack *)gf_node_get_private(node);

#ifdef DANAE
	if (st->is_active) {
		if (!st->dmo) st->dmo = getDanaeMediaOjbectFromUrl(st->comp->danae_session, st->aurl.vals[0].url, 2);
		if (st->dmo) {
				processDanaeAudio(st->dmo, (u32) (gf_node_get_scene_time(node)*1000));
				st->comp->draw_next_frame = 1;
		}
	}

#else
	/*check end of stream*/
	if (st->input.stream && st->input.stream_finished) {
		/*
		if (gf_mo_get_loop(st->input.stream, 0)) {
			gf_sr_audio_restart(&st->input);
		} else if (st->is_active && gf_mo_should_deactivate(st->input.stream)) {
			SVG_Deactivate_audio(st, audio);
		}
		*/
	}
	if (st->is_active) {
		gf_sr_audio_register(&st->input, (GF_BaseEffect*)rs);
	}
	/*store mute flag*/
	st->input.is_muted = (eff->trav_flags & GF_SR_TRAV_SWITCHED_OFF);
#endif
}

static void SVG_UpdateTime_audio(GF_TimeNode *tn)
{
	Double sceneTime;
	SVGaudioElement *audio = (SVGaudioElement *)tn->obj;
	SVG_audio_stack *st = (SVG_audio_stack *)gf_node_get_private(tn->obj);

	if (! st->is_active) {
		//st->start_time = as->startTime;
		st->start_time = 0;
		//st->input.speed = as->speed;
	}
	sceneTime = gf_node_get_scene_time(tn->obj);
	//if ((sceneTime<st->start_time) || (st->start_time<0)) return;
	
	/*
	if (st->input.input_ifce.GetSpeed(st->input.input_ifce.callback) && st->is_active) {
		if ( (as->stopTime > st->start_time) && (time>=as->stopTime)) {
			SVG_Deactivate_audio(st, audio);
			return;
		}
	}
	*/
	if (!st->is_active) SVG_Activate_audio(st, audio);
}

static void SVG_Destroy_audio(GF_Node *node)
{
	SVG_audio_stack *st = (SVG_audio_stack *)gf_node_get_private(node);
	gf_sr_audio_stop(&st->input);
	gf_sr_audio_unregister(&st->input);
	if (st->time_handle.is_registered) {
		gf_sr_unregister_time_node(st->input.compositor, &st->time_handle);
	}
	gf_sg_mfurl_del(st->aurl);
#ifdef DANAE
	if (st->dmo) releaseDanaeMediaObject(st->dmo); 
#endif
	free(st);
}

void SVG_Init_audio(Render2D *sr, GF_Node *node)
{
	SVG_audio_stack *st;
	GF_SAFEALLOC(st, sizeof(SVG_audio_stack))

	gf_sr_audio_setup(&st->input, sr->compositor, node);

	st->time_handle.UpdateTimeNode = SVG_UpdateTime_audio;
	st->time_handle.obj = node;

	/* creates an MFURL from the URI of the SVG element */
	SVG_SetMFURLFromURI(&(st->aurl), ((SVGaudioElement *)node)->xlink_href.iri);

	gf_node_set_private(node, st);
	gf_node_set_render_function(node, SVG_Render_audio);
	gf_node_set_predestroy_function(node, SVG_Destroy_audio);

	gf_sr_register_time_node(sr->compositor, &st->time_handle);
#ifdef DANAE
	st->comp = sr->compositor;
#endif
}

#endif //GPAC_DISABLE_SVG

