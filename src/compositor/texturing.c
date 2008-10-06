/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
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
#ifdef GPAC_TRISCOPE_MODE
#include "../src/compositor/triscope_renoir/triscope_renoir.h"
#endif


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
	gf_mx_p(compositor->mx);

	if (txh->tx_io) gf_sc_texture_release(txh);
	if (txh->is_open) gf_sc_texture_stop(txh);
#ifdef GPAC_TRISCOPE_MODE
	/*Destroy the renoir object associated to this texture*/
	if (txh->RenoirObject) DestroyRenoirObject (txh->RenoirObject, (GF_RenoirHandler *) txh->compositor->RenoirHandler);
#endif
	gf_list_del_item(txh->compositor->textures, txh);

	gf_mx_v(compositor->mx);
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

	/*store url*/
	gf_sg_vrml_field_copy(&txh->current_url, url, GF_SG_VRML_MFURL);

	/*get media object*/
	txh->stream = gf_mo_register(txh->owner, url, lock_scene_timeline);
	/*bad/Empty URL*/
	if (!txh->stream) return GF_NOT_SUPPORTED;
	/*request play*/
	gf_mo_play(txh->stream, start_offset, end_offset, can_loop);

	txh->last_frame_time = (u32) (-1);
	gf_sc_invalidate(txh->compositor, NULL);
	txh->is_open = 1;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_sc_texture_play(GF_TextureHandler *txh, MFURL *url)
{
	Double offset = 0;
	Bool loop = 0;
	if (txh->compositor->term && (txh->compositor->term->play_state!=GF_STATE_PLAYING)) {
		offset = gf_node_get_scene_time(txh->owner);
		loop = /*gf_mo_get_loop(gf_mo_register(txh->owner, url, 0), 0)*/ 1;
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
	gf_mo_stop(txh->stream);
	gf_sg_vrml_mf_reset(&txh->current_url, GF_SG_VRML_MFURL);
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

GF_EXPORT
void gf_sc_texture_update_frame(GF_TextureHandler *txh, Bool disable_resync)
{
	u32 size, ts;

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
		/*if we had a texture this means the object has changed - delete texture and force next frame 
		composition (this will take care of OD reuse)*/
		if (txh->tx_io) {
			gf_sc_texture_release(txh);
			txh->data = NULL;
			txh->needs_refresh = 1;
			gf_sc_invalidate(txh->compositor, NULL);
			return;
		}
	}
	txh->data = gf_mo_fetch_data(txh->stream, !disable_resync, &txh->stream_finished, &ts, &size);

	/*if no frame or muted don't draw*/
	if (!txh->data || !size) return;

	/*if setup and same frame return*/
	if (txh->tx_io && (txh->stream_finished || (txh->last_frame_time==ts)) ) {
		gf_mo_release_data(txh->stream, 0xFFFFFFFF, 0);
		txh->needs_release = 0;
		return;
	}
	txh->needs_release = 1; 
	txh->last_frame_time = ts;
	if (gf_mo_is_muted(txh->stream)) return;

	if (!txh->tx_io) {
		gf_sc_texture_allocate(txh);
		if (!txh->tx_io) return;

		gf_mo_get_visual_info(txh->stream, &txh->width, &txh->height, &txh->stride, &txh->pixel_ar, &txh->pixelformat);

		txh->transparent = 0;
		switch (txh->pixelformat) {
		case GF_PIXEL_ALPHAGREY:
		case GF_PIXEL_ARGB:
		case GF_PIXEL_RGBA:
		case GF_PIXEL_YUVA:
			txh->transparent = 1;
			break;
		}

		gf_mo_set_flag(txh->stream, GF_MO_IS_INIT, 1);
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
	case TAG_MPEG4_ImageTexture: case TAG_X3D_ImageTexture: return it_get_texture(n);
	case TAG_MPEG4_MovieTexture: case TAG_X3D_MovieTexture: return mt_get_texture(n);
	case TAG_MPEG4_PixelTexture: case TAG_X3D_PixelTexture: return pt_get_texture(n);

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
