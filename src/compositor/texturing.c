/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre 
 *			Copyright (c) Telecom ParisTech 2000-2012
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

#include "texturing.h"
#include <gpac/internal/terminal_dev.h>
#include <gpac/options.h>

#include "nodes_stacks.h"

static void update_texture_void(GF_TextureHandler *txh)
{
}

GF_EXPORT
void gf_sc_texture_setup(GF_TextureHandler *txh, GF_Compositor *compositor, GF_Node *owner)
{
	memset(txh, 0, sizeof(GF_TextureHandler));
	txh->owner = owner;
	txh->compositor = compositor;
	/*insert texture in reverse order, so that textures in sub documents/scenes are updated before parent ones*/
	if (gf_list_find(compositor->textures, txh)<0) 
		gf_list_insert(compositor->textures, txh, 0);
	if (!txh->update_texture_fcnt) txh->update_texture_fcnt = update_texture_void;
}


GF_EXPORT
void gf_sc_texture_destroy(GF_TextureHandler *txh)
{
	GF_Compositor *compositor = txh->compositor;
	Bool lock = gf_mx_try_lock(compositor->mx);

	gf_sc_texture_release(txh);
	if (txh->is_open) gf_sc_texture_stop(txh);
	gf_list_del_item(txh->compositor->textures, txh);

	if (lock) gf_mx_v(compositor->mx);
}

GF_EXPORT
Bool gf_sc_texture_check_url_change(GF_TextureHandler *txh, MFURL *url)
{
	if (!txh->stream) return url->count;
	return gf_mo_url_changed(txh->stream, url);
}

GF_EXPORT
GF_Err gf_sc_texture_play_from_to(GF_TextureHandler *txh, MFURL *url, Double start_offset, Double end_offset, Bool can_loop, Bool lock_scene_timeline)
{
	if (txh->is_open) return GF_BAD_PARAM;

	/*if existing texture in cache destroy it - we don't destroy it on stop to handle MovieTexture*/
	if (txh->tx_io) gf_sc_texture_release(txh);

	/*get media object*/
	txh->stream = gf_mo_register(txh->owner, url, lock_scene_timeline, 0);
	/*bad/Empty URL*/
	if (!txh->stream) return GF_NOT_SUPPORTED;
	/*request play*/
	gf_mo_play(txh->stream, start_offset, end_offset, can_loop);

	txh->last_frame_time = (u32) (-1);
	//gf_sc_invalidate(txh->compositor, NULL);
	txh->is_open = 1;

	/*request play*/
	txh->raw_memory = gf_mo_is_raw_memory(txh->stream);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_sc_texture_play(GF_TextureHandler *txh, MFURL *url)
{
	Double offset = 0;
	Bool loop = 0;
	if (txh->compositor->term && (txh->compositor->term->play_state!=GF_STATE_PLAYING)) {
		offset = gf_node_get_scene_time(txh->owner);
		loop = /*gf_mo_get_loop(gf_mo_register(txh->owner, url, 0, 0), 0)*/ 1;
	}
	return gf_sc_texture_play_from_to(txh, url, offset, -1, loop, 0);
}


GF_EXPORT
void gf_sc_texture_stop(GF_TextureHandler *txh)
{
	if (!txh->is_open) return;
	/*release texture WITHOUT droping frame*/
	if (txh->needs_release) {
		gf_mo_release_data(txh->stream, 0xFFFFFFFF, -1);
		txh->needs_release = 0;
	}
	gf_sc_invalidate(txh->compositor, NULL);
	if (gf_mo_stop(txh->stream)) {
		txh->data = NULL;
	}
	txh->is_open = 0;

	/*and deassociate object*/
	gf_mo_unregister(txh->owner, txh->stream);
	txh->stream = NULL;
}

GF_EXPORT
void gf_sc_texture_restart(GF_TextureHandler *txh)
{
	if (!txh->is_open) return;
	gf_sc_texture_release_stream(txh);
	txh->stream_finished = 0;
	gf_mo_restart(txh->stream);
}


static void setup_texture_object(GF_TextureHandler *txh, Bool private_media)
{
	if (!txh->tx_io) {
        gf_sc_texture_allocate(txh);
		if (!txh->tx_io) return;

		gf_mo_get_visual_info(txh->stream, &txh->width, &txh->height, &txh->stride, &txh->pixel_ar, &txh->pixelformat, &txh->is_flipped);
        
        if (private_media) {
			txh->transparent = 1;
			txh->pixelformat = GF_PIXEL_ARGB;
			txh->flags |= GF_SR_TEXTURE_PRIVATE_MEDIA;
		} else {
			txh->transparent = 0;
			switch (txh->pixelformat) {
			case GF_PIXEL_ALPHAGREY:
			case GF_PIXEL_ARGB:
			case GF_PIXEL_RGBA:
			case GF_PIXEL_YUVA:
			case GF_PIXEL_RGBDS:
				txh->transparent = 1;
				break;
			}
		}
		gf_mo_set_flag(txh->stream, GF_MO_IS_INIT, 1);
	}
}


GF_EXPORT
void gf_sc_texture_update_frame(GF_TextureHandler *txh, Bool disable_resync)
{
	Bool needs_reload = 0;
	u32 size, ts, ms_until_next;
	s32 ms_until_pres;

	/*already refreshed*/
	if (txh->needs_refresh) return;

	if (!txh->stream) {
		txh->data = NULL;
		return;
	}

	/*should never happen!!*/
	if (txh->needs_release) gf_mo_release_data(txh->stream, 0xFFFFFFFF, 0);

	/*check init flag*/
	if (!(gf_mo_get_flags(txh->stream) & GF_MO_IS_INIT)) {
		needs_reload = 1;
		txh->data = NULL;
		if (txh->tx_io) {
			gf_sc_texture_release(txh);
		}
	}
	txh->data = gf_mo_fetch_data(txh->stream, !disable_resync, &txh->stream_finished, &ts, &size, &ms_until_pres, &ms_until_next);

	if (!(gf_mo_get_flags(txh->stream) & GF_MO_IS_INIT)) {
		needs_reload = 1;
	}

	if (needs_reload) {
		/*if we had a texture this means the object has changed - delete texture and resetup. Do not skip
		texture update as this may lead to an empty rendering pass (blank frame for this object), especially in DASH*/
		if (txh->tx_io) {
			gf_sc_texture_release(txh);
			txh->needs_refresh = 1;
		}
		if (gf_mo_is_private_media(txh->stream)) {
			setup_texture_object(txh, 1);
			gf_node_dirty_set(txh->owner, 0, 0);
		}
	}

	/*if no frame or muted don't draw*/
	if (!txh->data || !size) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("No output frame available\n"));
		/*TODO - check if this is needed */
		if (txh->flags & GF_SR_TEXTURE_PRIVATE_MEDIA) {
			//txh->needs_refresh = 1;
			gf_sc_invalidate(txh->compositor, NULL);
		}
		return;
	}

	/*if setup and same frame return*/
	if (txh->tx_io && (txh->stream_finished || (txh->last_frame_time==ts)) ) {
		gf_mo_release_data(txh->stream, 0xFFFFFFFF, 0);
		txh->needs_release = 0;
		if (!txh->stream_finished) {
			if (txh->compositor->next_frame_delay > ms_until_next)
				txh->compositor->next_frame_delay = ms_until_next;
		}
		return;
	}
	txh->needs_release = 1; 
	txh->last_frame_time = ts;
	if (txh->raw_memory) {
		gf_mo_get_raw_image_planes(txh->stream, (u8 **) &txh->data, (u8 **) &txh->pU, (u8 **) &txh->pV);
	}
	if (gf_mo_is_muted(txh->stream)) return;


	if (txh->nb_frames) {
		s32 push_delay = txh->upload_time / txh->nb_frames;
		if (push_delay > ms_until_pres) ms_until_pres = 0;
		else ms_until_pres -= push_delay;
	}

	if (txh->compositor->frame_delay < ms_until_pres)
		txh->compositor->frame_delay = ms_until_pres;

	txh->compositor->next_frame_delay = 1;

	if (!txh->tx_io) {
		setup_texture_object(txh, 0);
	}

	/*try to push texture on graphics but don't complain if failure*/
	gf_sc_texture_set_data(txh);

	txh->needs_refresh = 1;
	gf_sc_invalidate(txh->compositor, NULL);
}

GF_EXPORT
void gf_sc_texture_release_stream(GF_TextureHandler *txh)
{
	if (txh->needs_release) {
		assert(txh->stream);
		gf_mo_release_data(txh->stream, 0xFFFFFFFF, 0);
		txh->needs_release = 0;
	}
	txh->needs_refresh = 0;
}


GF_EXPORT
GF_TextureHandler *gf_sc_texture_get_handler(GF_Node *n)
{
	if (!n) return NULL;
	switch (gf_node_get_tag(n)) {
#ifndef GPAC_DISABLE_VRML
	case TAG_MPEG4_ImageTexture: 
	case TAG_MPEG4_CacheTexture: 
		return it_get_texture(n);
	case TAG_MPEG4_MovieTexture: return mt_get_texture(n);
	case TAG_MPEG4_PixelTexture: return pt_get_texture(n);

	case TAG_MPEG4_CompositeTexture2D: 
	case TAG_MPEG4_CompositeTexture3D: 
		return compositor_get_composite_texture(n);
	case TAG_MPEG4_LinearGradient: 
	case TAG_MPEG4_RadialGradient: 
		return compositor_mpeg4_get_gradient_texture(n);

	case TAG_MPEG4_MatteTexture:
	{
		GF_TextureHandler *hdl = gf_sc_texture_get_handler( ((M_MatteTexture*)n)->surfaceB );
		if (hdl) hdl->matteTexture = n;
		return hdl;
	}
#endif /*GPAC_DISABLE_VRML*/

#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_ImageTexture: return it_get_texture(n);
	case TAG_X3D_MovieTexture: return mt_get_texture(n);
	case TAG_X3D_PixelTexture: return pt_get_texture(n);
#endif


#ifndef GPAC_DISABLE_SVG
	case TAG_SVG_linearGradient: 
	case TAG_SVG_radialGradient: 
		return compositor_svg_get_gradient_texture(n);
	case TAG_SVG_image:
	case TAG_SVG_video:
		return compositor_svg_get_image_texture(n);
#endif

	default: return NULL;
	}
}
