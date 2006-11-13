/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / Scene Rendering sub-project
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

#include "common_stacks.h"

#include <gpac/nodes_mpeg4.h>
#include <gpac/nodes_x3d.h>

typedef struct
{
	GF_TextureHandler txh;

	GF_TimeNode time_handle;
	Bool fetch_first_frame, first_frame_fetched, is_vrml;
	Double start_time;
} MovieTextureStack;

static void DestroyMovieTexture(GF_Node *node)
{
	MovieTextureStack *st = (MovieTextureStack *) gf_node_get_private(node);
	gf_sr_texture_destroy(&st->txh);
	if (st->time_handle.is_registered) gf_sr_unregister_time_node(st->txh.compositor, &st->time_handle);
	free(st);
}
static Fixed MT_GetSpeed(MovieTextureStack *stack, M_MovieTexture *mt)
{
	return gf_mo_get_speed(stack->txh.stream, mt->speed);
}
static Bool MT_GetLoop(MovieTextureStack *stack, M_MovieTexture *mt)
{
	return gf_mo_get_loop(stack->txh.stream, mt->loop);
}
static void MT_Activate(MovieTextureStack *stack, M_MovieTexture *mt, Double scene_time)
{
	mt->isActive = 1;
	gf_node_event_out_str((GF_Node*)mt, "isActive");
	if (!stack->txh.is_open) {
		scene_time -= mt->startTime;
		gf_sr_texture_play_from(&stack->txh, &mt->url, scene_time, gf_mo_get_loop(stack->txh.stream, mt->loop));
	}
	gf_mo_set_speed(stack->txh.stream, mt->speed);
}
static void MT_Deactivate(MovieTextureStack *stack, M_MovieTexture *mt)
{
	mt->isActive = 0;
	gf_node_event_out_str((GF_Node*)mt, "isActive");
	stack->time_handle.needs_unregister = 1;

	if (stack->txh.is_open) {
		gf_sr_texture_stop(&stack->txh);
	}
}
static void UpdateMovieTexture(GF_TextureHandler *txh)
{
	M_MovieTexture *txnode = (M_MovieTexture *) txh->owner;
	MovieTextureStack *st = (MovieTextureStack *) gf_node_get_private(txh->owner);
	
	/*setup texture if needed*/
	if (!txh->is_open) return;
	if (!txnode->isActive && st->first_frame_fetched) return;

	/*when fetching the first frame disable resync*/
	gf_sr_texture_update_frame(txh, !txnode->isActive);

	if (txh->stream_finished) {
		if (MT_GetLoop(st, txnode)) {
			gf_sr_texture_restart(txh);
		}
		/*if active deactivate*/
		else if (txnode->isActive && gf_mo_should_deactivate(st->txh.stream) ) {
			MT_Deactivate(st, txnode);
		}
	}
	/*first frame is fetched*/
	if (!st->first_frame_fetched && (txh->needs_refresh) ) {
		st->first_frame_fetched = 1;
		txnode->duration_changed = gf_mo_get_duration(txh->stream);
		gf_node_event_out_str(txh->owner, "duration_changed");
		/*stop stream if needed*/
		if (!txnode->isActive && txh->is_open) {
			gf_sr_texture_stop(txh);
			/*make sure the refresh flag is not cleared*/
			txh->needs_refresh = 1;
		}
	}
	/*we have no choice but retraversing the graph until we're inactive since the movie framerate and
	the renderer framerate are likely to be different*/
	if (!txh->stream_finished) gf_sr_invalidate(txh->compositor, NULL);

}
static void MT_UpdateTime(GF_TimeNode *st)
{
	Double time;
	M_MovieTexture *mt = (M_MovieTexture *)st->obj;
	MovieTextureStack *stack = (MovieTextureStack *)gf_node_get_private(st->obj);
	
	/*not active, store start time and speed*/
	if ( ! mt->isActive) {
		stack->start_time = mt->startTime;
	}
	time = gf_node_get_scene_time(st->obj);

	if (time < stack->start_time ||
		/*special case if we're getting active AFTER stoptime */
		(!mt->isActive && (mt->stopTime > stack->start_time) && (time>=mt->stopTime))
		|| (!stack->start_time && stack->is_vrml && !mt->loop)
		) {
		/*opens stream only at first access to fetch first frame*/
		if (stack->fetch_first_frame) {
			stack->fetch_first_frame = 0;
			if (!stack->txh.is_open)
				gf_sr_texture_play(&stack->txh, &mt->url);
		}
		return;
	}

	if (MT_GetSpeed(stack, mt) && mt->isActive) {
		/*if stoptime is reached (>startTime) deactivate*/
		if ((mt->stopTime > stack->start_time) && (time >= mt->stopTime) ) {
			MT_Deactivate(stack, mt);
			return;
		}
	}

	/*we're (about to be) active: VRML:
	"A time-dependent node is inactive until its startTime is reached. When time now becomes greater than or 
	equal to startTime, an isActive TRUE event is generated and the time-dependent node becomes active 	*/

	if (! mt->isActive) MT_Activate(stack, mt, time);
}
void InitMovieTexture(GF_Renderer *sr, GF_Node *node)
{
	MovieTextureStack *st;
	GF_SAFEALLOC(st, MovieTextureStack);
	gf_sr_texture_setup(&st->txh, sr, node);
	st->txh.update_texture_fcnt = UpdateMovieTexture;
	st->time_handle.UpdateTimeNode = MT_UpdateTime;
	st->time_handle.obj = node;
	st->fetch_first_frame = 1;
	st->txh.flags = 0;
	if (((M_MovieTexture*)node)->repeatS) st->txh.flags |= GF_SR_TEXTURE_REPEAT_S;
	if (((M_MovieTexture*)node)->repeatT) st->txh.flags |= GF_SR_TEXTURE_REPEAT_T;
	
	st->is_vrml = (gf_node_get_tag(node)==TAG_X3D_MovieTexture) ? 1 : 0;

	gf_node_set_private(node, st);
	gf_node_set_predestroy_function(node, DestroyMovieTexture);
	
	gf_sr_register_time_node(sr, &st->time_handle);
}

GF_TextureHandler *mt_get_texture(GF_Node *node)
{
	MovieTextureStack *st = (MovieTextureStack *) gf_node_get_private(node);
	return &st->txh;
}

void MovieTextureModified(GF_Node *node)
{
	M_MovieTexture *mt = (M_MovieTexture *)node;
	MovieTextureStack *st = (MovieTextureStack *) gf_node_get_private(node);
	if (!st) return;

	/*if open and changed, stop and play*/
	if (st->txh.is_open && gf_sr_texture_check_url_change(&st->txh, &mt->url)) {
		gf_sr_texture_stop(&st->txh);
		gf_sr_texture_play(&st->txh, &mt->url);
	} 
	/*update state if we're active*/
	else if (mt->isActive) {
		MT_UpdateTime(&st->time_handle);
		if (!mt->isActive) return;
	}
	/*reregister if needed*/
	st->time_handle.needs_unregister = 0;
	if (!st->time_handle.is_registered) gf_sr_register_time_node(st->txh.compositor, &st->time_handle);
}

typedef struct
{
	GF_TextureHandler txh;
} ImageTextureStack;

static void DestroyImageTexture(GF_Node *node)
{
	ImageTextureStack *st = (ImageTextureStack *) gf_node_get_private(node);
	gf_sr_texture_destroy(&st->txh);
	free(st);
}
static void UpdateImageTexture(GF_TextureHandler *txh)
{
	M_ImageTexture *txnode = (M_ImageTexture *) txh->owner;
	
	/*setup texture if needed*/
	if (!txh->is_open && txnode->url.count) {
		gf_sr_texture_play(txh, &txnode->url);
	}
	gf_sr_texture_update_frame(txh, 0);
	/*URL is present but not opened - redraw till fetch*/
	if (txh->stream && !txh->hwtx) gf_sr_invalidate(txh->compositor, NULL);
}

void InitImageTexture(GF_Renderer *sr, GF_Node *node)
{
	ImageTextureStack *st;
	GF_SAFEALLOC(st, ImageTextureStack);
	gf_sr_texture_setup(&st->txh, sr, node);
	st->txh.update_texture_fcnt = UpdateImageTexture;
	gf_node_set_private(node, st);
	gf_node_set_predestroy_function(node, DestroyImageTexture);
	st->txh.flags = 0;
	if (((M_ImageTexture*)node)->repeatS) st->txh.flags |= GF_SR_TEXTURE_REPEAT_S;
	if (((M_ImageTexture*)node)->repeatT) st->txh.flags |= GF_SR_TEXTURE_REPEAT_T;
}

GF_TextureHandler *it_get_texture(GF_Node *node)
{
	ImageTextureStack *st = (ImageTextureStack *) gf_node_get_private(node);
	return &st->txh;
}
void ImageTextureModified(GF_Node *node)
{
	M_ImageTexture *im = (M_ImageTexture *)node;
	ImageTextureStack *st = (ImageTextureStack *) gf_node_get_private(node);
	if (!st) return;

	/*if open and changed, stop and play*/
	if (st->txh.is_open) {
		if (! gf_sr_texture_check_url_change(&st->txh, &im->url)) return;
		gf_sr_texture_stop(&st->txh);
		gf_sr_texture_play(&st->txh, &im->url);
		return;
	}
	/*if not open and changed play*/
	if (im->url.count) 
		gf_sr_texture_play(&st->txh, &im->url);
}



typedef struct
{
	GF_TextureHandler txh;
	char *pixels;
} PixelTextureStack;

static void DestroyPixelTexture(GF_Node *node)
{
	PixelTextureStack *st = (PixelTextureStack *) gf_node_get_private(node);
	if (st->pixels) free(st->pixels);
	gf_sr_texture_destroy(&st->txh);
	free(st);
}
static void UpdatePixelTexture(GF_TextureHandler *txh)
{
	u32 pix_format, stride, i;
	M_PixelTexture *pt = (M_PixelTexture *) txh->owner;
	PixelTextureStack *st = (PixelTextureStack *) gf_node_get_private(txh->owner);

	if (!gf_node_dirty_get(txh->owner)) return;
	gf_node_dirty_clear(txh->owner, 0);

	
	/*pixel texture doesn not use any media object but has data in the content. 
	However we still use the same texture object, just be carefull not to use media funtcions*/
	txh->transparent = 0;
	stride = pt->image.width;
	/*num_components are as in VRML (1->4) not as in BIFS bitstream (0->3)*/
	switch (pt->image.numComponents) {
	case 1:
		pix_format = GF_PIXEL_GREYSCALE;
		break;
	case 2:
		pix_format = GF_PIXEL_ALPHAGREY;
		txh->transparent = 1;
		stride *= 2;
		break;
	case 3:
		pix_format = GF_PIXEL_RGB_24;
		txh->transparent = 0;
		stride *= 3;
		break;
	case 4:
		pix_format = GF_PIXEL_RGBA;
		txh->transparent = 1;
		stride *= 4;
		break;
	default:
		return;
	}

	if (!txh->hwtx) txh->compositor->visual_renderer->AllocTexture(txh);
	if (!txh->hwtx) return;

	if (st->pixels) free(st->pixels);
	st->pixels = (char*)malloc(sizeof(char) * stride * pt->image.height);
	if (txh->compositor->visual_renderer->bNeedsGL) {
		for (i=0; i<pt->image.height; i++) {
			memcpy(st->pixels + i * stride, pt->image.pixels + i * stride, stride);
		}
	} 
	/*revert pixel ordering...*/
	else {
		for (i=0; i<pt->image.height; i++) {
			memcpy(st->pixels + i * stride, pt->image.pixels + (pt->image.height - 1 - i) * stride, stride);
		}
	}

	txh->width = pt->image.width;
	txh->height = pt->image.height;
	txh->stride = stride;
	txh->pixelformat = pix_format;
	txh->data = st->pixels;

	txh->compositor->visual_renderer->SetTextureData(txh);
}

void InitPixelTexture(GF_Renderer *sr, GF_Node *node)
{
	PixelTextureStack *st;
	GF_SAFEALLOC(st, PixelTextureStack);
	gf_sr_texture_setup(&st->txh, sr, node);
	st->pixels = NULL;
	st->txh.update_texture_fcnt = UpdatePixelTexture;

	gf_node_set_private(node, st);
	gf_node_set_predestroy_function(node, DestroyPixelTexture);
	st->txh.flags = 0;
	if (((M_PixelTexture*)node)->repeatS) st->txh.flags |= GF_SR_TEXTURE_REPEAT_S;
	if (((M_PixelTexture*)node)->repeatT) st->txh.flags |= GF_SR_TEXTURE_REPEAT_T;
}

GF_TextureHandler *pt_get_texture(GF_Node *node)
{
	PixelTextureStack *st = (PixelTextureStack *) gf_node_get_private(node);
	return &st->txh;
}


GF_EXPORT
GF_TextureHandler *gf_sr_texture_get_handler(GF_Node *n)
{
	if (!n) return NULL;
	switch (gf_node_get_tag(n)) {
	case TAG_MPEG4_ImageTexture: case TAG_X3D_ImageTexture: return it_get_texture(n);
	case TAG_MPEG4_MovieTexture: case TAG_X3D_MovieTexture: return mt_get_texture(n);
	case TAG_MPEG4_PixelTexture: case TAG_X3D_PixelTexture: return pt_get_texture(n);
	default: return NULL;
	}
}
