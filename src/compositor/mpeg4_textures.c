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

#include <gpac/network.h>
#include <gpac/nodes_mpeg4.h>
#include <gpac/nodes_x3d.h>

#ifndef GPAC_DISABLE_VRML

#ifdef __cplusplus
extern "C" {
#endif

/*for cache texture decode and hash*/
#include <gpac/avparse.h>
#include <gpac/crypt.h>
#include "nodes_stacks.h"


typedef struct
{
	GF_TextureHandler txh;

	GF_TimeNode time_handle;
	Bool fetch_first_frame, first_frame_fetched, is_x3d;
	Double start_time;
} MovieTextureStack;

static void movietexture_destroy(GF_Node *node, void *rs, Bool is_destroy)
{
	if (is_destroy) {
		MovieTextureStack *st = (MovieTextureStack *) gf_node_get_private(node);
		gf_sc_texture_destroy(&st->txh);
		if (st->time_handle.is_registered) gf_sc_unregister_time_node(st->txh.compositor, &st->time_handle);
		gf_free(st);
	}
}
static Fixed movietexture_get_speed(MovieTextureStack *stack, M_MovieTexture *mt)
{
	return gf_mo_get_speed(stack->txh.stream, mt->speed);
}
static Bool movietexture_get_loop(MovieTextureStack *stack, M_MovieTexture *mt)
{
	return gf_mo_get_loop(stack->txh.stream, mt->loop);
}
static void movietexture_activate(MovieTextureStack *stack, M_MovieTexture *mt, Double scene_time)
{
	mt->isActive = 1;
	gf_node_event_out((GF_Node*)mt, 8/*"isActive"*/);
	if (!stack->txh.is_open) {
		scene_time -= mt->startTime;
		gf_sc_texture_play_from_to(&stack->txh, &mt->url, scene_time, -1, gf_mo_get_loop(stack->txh.stream, mt->loop), 0);
	} else if (stack->first_frame_fetched) {
		gf_mo_resume(stack->txh.stream);
	}
	gf_mo_set_speed(stack->txh.stream, mt->speed);
}
static void movietexture_deactivate(MovieTextureStack *stack, M_MovieTexture *mt)
{
	mt->isActive = 0;
	gf_node_event_out((GF_Node*)mt, 8/*"isActive"*/);
	stack->time_handle.needs_unregister = 1;

	if (stack->txh.is_open) {
		gf_sc_texture_stop_no_unregister(&stack->txh);
	}
}
static void movietexture_update(GF_TextureHandler *txh)
{
	M_MovieTexture *txnode = (M_MovieTexture *) txh->owner;
	MovieTextureStack *st = (MovieTextureStack *) gf_node_get_private(txh->owner);

	/*setup texture if needed*/
	if (!txh->is_open) return;
	if (!txnode->isActive && st->first_frame_fetched) return;

	/*when fetching the first frame disable resync*/
	gf_sc_texture_update_frame(txh, 0);

	if (txh->stream_finished) {
		if (movietexture_get_loop(st, txnode)) {
			gf_sc_texture_restart(txh);
		}
		/*if active deactivate*/
		else if (txnode->isActive && gf_mo_should_deactivate(st->txh.stream) ) {
			movietexture_deactivate(st, txnode);
		}
	}
	/*first frame is fetched*/
	if (!st->first_frame_fetched && (txh->needs_refresh) ) {
		st->first_frame_fetched = 1;
		txnode->duration_changed = gf_mo_get_duration(txh->stream);
		gf_node_event_out(txh->owner, 7/*"duration_changed"*/);
		/*stop stream if needed*/
		if (!txnode->isActive && txh->is_open) {
			gf_mo_pause(txh->stream);
			/*make sure the refresh flag is not cleared*/
			txh->needs_refresh = 1;
			gf_sc_invalidate(txh->compositor, NULL);
		}
	}
	if (txh->needs_refresh) {
		/*mark all subtrees using this image as dirty*/
		gf_node_dirty_parents(txh->owner);
	}
}

static void movietexture_update_time(GF_TimeNode *st)
{
	Double time;
	M_MovieTexture *mt = (M_MovieTexture *)st->udta;
	MovieTextureStack *stack = (MovieTextureStack *)gf_node_get_private(st->udta);

	/*not active, store start time and speed*/
	if ( ! mt->isActive) {
		stack->start_time = mt->startTime;
	}
	time = gf_node_get_scene_time(st->udta);

	if (time < stack->start_time ||
	        /*special case if we're getting active AFTER stoptime */
	        (!mt->isActive && (mt->stopTime > stack->start_time) && (time>=mt->stopTime))
//		|| (!stack->start_time && !stack->is_x3d && !mt->loop)
	   ) {
		/*opens stream only at first access to fetch first frame*/
		if (stack->fetch_first_frame) {
			stack->fetch_first_frame = 0;
			if (!stack->txh.is_open)
				gf_sc_texture_play(&stack->txh, &mt->url);
			else
				gf_mo_resume(stack->txh.stream);
		}
		return;
	}

	if (movietexture_get_speed(stack, mt) && mt->isActive) {
		/*if stoptime is reached (>startTime) deactivate*/
		if ((mt->stopTime > stack->start_time) && (time >= mt->stopTime) ) {
			movietexture_deactivate(stack, mt);
			return;
		}
	}

	/*we're (about to be) active: VRML:
	"A time-dependent node is inactive until its startTime is reached. When time now becomes greater than or
	equal to startTime, an isActive TRUE event is generated and the time-dependent node becomes active 	*/

	if (! mt->isActive) movietexture_activate(stack, mt, time);
	stack->txh.stream_finished = GF_FALSE;
}

void compositor_init_movietexture(GF_Compositor *compositor, GF_Node *node)
{
	MovieTextureStack *st;
	GF_SAFEALLOC(st, MovieTextureStack);
	if (!st) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate movie texture stack\n"));
		return;
	}
	gf_sc_texture_setup(&st->txh, compositor, node);
	st->txh.update_texture_fcnt = movietexture_update;
	st->time_handle.UpdateTimeNode = movietexture_update_time;
	st->time_handle.udta = node;
	st->fetch_first_frame = 1;
	st->txh.flags = 0;
	if (((M_MovieTexture*)node)->repeatS) st->txh.flags |= GF_SR_TEXTURE_REPEAT_S;
	if (((M_MovieTexture*)node)->repeatT) st->txh.flags |= GF_SR_TEXTURE_REPEAT_T;

#ifndef GPAC_DISABLE_X3D
	st->is_x3d = (gf_node_get_tag(node)==TAG_X3D_MovieTexture) ? 1 : 0;
#endif

	gf_node_set_private(node, st);
	gf_node_set_callback_function(node, movietexture_destroy);

	gf_sc_register_time_node(compositor, &st->time_handle);
}

GF_TextureHandler *mt_get_texture(GF_Node *node)
{
	MovieTextureStack *st = (MovieTextureStack *) gf_node_get_private(node);
	return &st->txh;
}

void compositor_movietexture_modified(GF_Node *node)
{
	M_MovieTexture *mt = (M_MovieTexture *)node;
	MovieTextureStack *st = (MovieTextureStack *) gf_node_get_private(node);
	if (!st) return;

	/*if open and changed, stop and play*/
	if (gf_sc_texture_check_url_change(&st->txh, &mt->url)) {
		if (st->txh.is_open) gf_sc_texture_stop(&st->txh);
		if (mt->isActive) gf_sc_texture_play(&st->txh, &mt->url);
	}
	/*update state if we're active*/
	else if (mt->isActive) {
		movietexture_update_time(&st->time_handle);
		if (!mt->isActive) return;
	}
	/*reregister if needed*/
	st->time_handle.needs_unregister = 0;
	if (!st->time_handle.is_registered) gf_sc_register_time_node(st->txh.compositor, &st->time_handle);
}

static void imagetexture_destroy(GF_Node *node, void *rs, Bool is_destroy)
{
	if (is_destroy) {
		GF_TextureHandler *txh = (GF_TextureHandler *) gf_node_get_private(node);

		/*cleanup cache if needed*/
		if (gf_node_get_tag(node)==TAG_MPEG4_CacheTexture) {
			char section[64];
			const char *opt, *file;
			Bool delete_file = GF_TRUE;
			M_CacheTexture *ct = (M_CacheTexture*)node;

			sprintf(section, "@cache=%p", ct);
			file = gf_opts_get_key(section, "cacheFile");
			opt = gf_opts_get_key(section, "expireAfterNTP");

			if (opt) {
				u32 sec, frac, exp;
				sscanf(opt, "%u", &exp);
				gf_net_get_ntp(&sec, &frac);
				if (!exp || (exp>sec)) delete_file=GF_FALSE;
			}
			if (delete_file) {
				if (file) gf_delete_file((char*)file);
				gf_opts_del_section(section);
			}

			if (txh->data) gf_free(txh->data);
			txh->data = NULL;
		}
		gf_sc_texture_destroy(txh);
		gf_free(txh);
	}
}

static void imagetexture_update(GF_TextureHandler *txh)
{
	if (gf_node_get_tag(txh->owner)!=TAG_MPEG4_CacheTexture) {
		MFURL url = ((M_ImageTexture *) txh->owner)->url;

		/*setup texture if needed*/
		if (!txh->is_open && url.count) {
			gf_sc_texture_play(txh, &url);
		}
		gf_sc_texture_update_frame(txh, 0);

		if (
		    /*URL is present but not opened - redraw till fetch*/
		    /* (txh->stream && !txh->tx_io) && */
		    /*image has been updated*/
		    txh->needs_refresh) {
			/*mark all subtrees using this image as dirty*/
			gf_node_dirty_parents(txh->owner);
			gf_sc_invalidate(txh->compositor, NULL);
		}
		return;
	}
	/*cache texture case*/
	else {
		M_CacheTexture *ct = (M_CacheTexture *) txh->owner;

		/*decode cacheTexture data */
		if ((ct->data || ct->image.buffer) && !txh->data) {
#ifndef GPAC_DISABLE_AV_PARSERS
			u32 out_size;
			GF_Err e;

			/*BT/XMT playback: load to memory*/
			if (ct->image.buffer) {
				char *par = (char *) gf_scene_get_service_url( gf_node_get_graph(txh->owner ) );
				char *src_url = gf_url_concatenate(par, ct->image.buffer);

				if (ct->data) gf_free(ct->data);
				ct->data = NULL;
				ct->data_len = 0;

				e = gf_file_load_data(src_url ? src_url : ct->image.buffer, &ct->data, &ct->data_len);
				if (e) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to load CacheTexture data from file %s: %s\n", src_url ? src_url : ct->image.buffer, gf_error_to_string(e) ) );
				}
				if (ct->image.buffer) gf_free(ct->image.buffer);
				ct->image.buffer = NULL;
				if (src_url) gf_free(src_url);
			}

			/*BIFS decoded playback*/
			switch (ct->objectTypeIndication) {
			case GF_CODECID_JPEG:
				out_size = 0;
				e = gf_img_jpeg_dec(ct->data, ct->data_len, &txh->width, &txh->height, &txh->pixelformat, NULL, &out_size, 3);
				if (e==GF_BUFFER_TOO_SMALL) {
					u32 BPP;
					txh->data = gf_malloc(sizeof(char) * out_size);
					if (txh->pixelformat==GF_PIXEL_GREYSCALE) BPP = 1;
					else BPP = 3;

					e = gf_img_jpeg_dec(ct->data, ct->data_len, &txh->width, &txh->height, &txh->pixelformat, txh->data, &out_size, BPP);
					if (e==GF_OK) {
						gf_sc_texture_allocate(txh);
						gf_sc_texture_set_data(txh);
						txh->needs_refresh = 1;
						txh->stride = out_size / txh->height;
					}
				}
				break;
			case GF_CODECID_PNG:
				out_size = 0;
				e = gf_img_png_dec(ct->data, ct->data_len, &txh->width, &txh->height, &txh->pixelformat, NULL, &out_size);
				if (e==GF_BUFFER_TOO_SMALL) {
					txh->data = gf_malloc(sizeof(char) * out_size);
					e = gf_img_png_dec(ct->data, ct->data_len, &txh->width, &txh->height, &txh->pixelformat, txh->data, &out_size);
					if (e==GF_OK) {
						gf_sc_texture_allocate(txh);
						gf_sc_texture_set_data(txh);
						txh->needs_refresh = 1;
						txh->stride = out_size / txh->height;
					}
				}
				break;
			}

#endif // GPAC_DISABLE_AV_PARSERS

			/*cacheURL is specified, store the image*/
			if (ct->cacheURL.buffer) {
				u32 i;
				u8 hash[20];
				FILE *cached_texture;
				char szExtractName[GF_MAX_PATH], section[64], *opt, *src_url;
				opt = (char *) gf_opts_get_key("core", "cache");
				if (opt) {
					strcpy(szExtractName, opt);
				} else {
					strcpy(szExtractName, gf_get_default_cache_directory());
				}
				strcat(szExtractName, "/");
				src_url = (char *) gf_scene_get_service_url( gf_node_get_graph(txh->owner ) );

				gf_sha1_csum((u8 *)src_url, (u32) strlen(src_url), hash);
				for (i=0; i<20; i++) {
					char t[3];
					t[2] = 0;
					sprintf(t, "%02X", hash[i]);
					strcat(szExtractName, t);
				}
				strcat(szExtractName, "_");

				strcat(szExtractName, ct->cacheURL.buffer);
				cached_texture = gf_fopen(szExtractName, "wb");
				if (cached_texture) {
					gf_fwrite(ct->data, 1, ct->data_len, cached_texture);
					gf_fclose(cached_texture);
				}

				/*and write cache info*/
				if (ct->expirationDate!=0) {
					sprintf(section, "@cache=%p", ct);
					gf_opts_set_key(section, "serviceURL", src_url);
					gf_opts_set_key(section, "cacheFile", szExtractName);
					gf_opts_set_key(section, "cacheName", ct->cacheURL.buffer);

					if (ct->expirationDate>0) {
						char exp[50];
						u32 sec, frac;
						gf_net_get_ntp(&sec, &frac);
						sec += ct->expirationDate;
						sprintf(exp, "%u", sec);
						gf_opts_set_key(section, "expireAfterNTP", exp);
					} else {
						gf_opts_set_key(section, "expireAfterNTP", "0");
					}
				}
			}

			/*done with image, destroy buffer*/
			if (ct->data) gf_free(ct->data);
			ct->data = NULL;
			ct->data_len = 0;
		}
	}
}

void compositor_init_imagetexture(GF_Compositor *compositor, GF_Node *node)
{
	GF_TextureHandler *txh;
	GF_SAFEALLOC(txh, GF_TextureHandler);
	if (!txh) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate image texture stack\n"));
		return;
	}
	gf_sc_texture_setup(txh, compositor, node);
	txh->update_texture_fcnt = imagetexture_update;
	gf_node_set_private(node, txh);
	gf_node_set_callback_function(node, imagetexture_destroy);
	txh->flags = 0;

	if (gf_node_get_tag(txh->owner)!=TAG_MPEG4_CacheTexture) {
		if (((M_ImageTexture*)node)->repeatS) txh->flags |= GF_SR_TEXTURE_REPEAT_S;
		if (((M_ImageTexture*)node)->repeatT) txh->flags |= GF_SR_TEXTURE_REPEAT_T;
	} else {
		const char *url;
		u32 i, count;
		M_CacheTexture*ct = (M_CacheTexture*)node;
		if (!ct->image.buffer) return;

		if (ct->repeatS) txh->flags |= GF_SR_TEXTURE_REPEAT_S;
		if (ct->repeatT) txh->flags |= GF_SR_TEXTURE_REPEAT_T;

		/*locate existing cache*/
		url = gf_scene_get_service_url( gf_node_get_graph(node) );
		count = gf_opts_get_section_count();
		for (i=0; i<count; i++) {
			const char *opt;
			const char *name = gf_opts_get_section_name(i);
			if (strncmp(name, "@cache=", 7)) continue;
			opt = gf_opts_get_key(name, "serviceURL");
			if (!opt || stricmp(opt, url)) continue;
			opt = gf_opts_get_key(name, "cacheName");
			if (opt && ct->cacheURL.buffer && !stricmp(opt, ct->cacheURL.buffer)) {
				opt = gf_opts_get_key(name, "cacheFile");
				if (opt) gf_delete_file((char*)opt);
				gf_opts_del_section(name);
				break;
			}
		}

	}
}

GF_TextureHandler *it_get_texture(GF_Node *node)
{
	return gf_node_get_private(node);
}
void compositor_imagetexture_modified(GF_Node *node)
{
	MFURL url;
	SFURL sfurl;
	GF_TextureHandler *txh = (GF_TextureHandler *) gf_node_get_private(node);
	if (!txh) return;

	if (gf_node_get_tag(node)!=TAG_MPEG4_CacheTexture) {
		url = ((M_ImageTexture *) node)->url;
	} else {
		url.count = 1;
		sfurl.OD_ID=GF_MEDIA_EXTERNAL_ID;
		sfurl.url = ((M_CacheTexture *) node)->image.buffer;
		url.vals = &sfurl;
	}

	/*if open and changed, stop and play*/
	if (txh->is_open) {
		if (! gf_sc_texture_check_url_change(txh, &url)) return;
		gf_sc_texture_stop(txh);
		gf_sc_texture_play(txh, &url);
		return;
	}
	/*if not open and changed play*/
	if (url.count)
		gf_sc_texture_play(txh, &url);
}



typedef struct
{
	GF_TextureHandler txh;
	char *pixels;
} PixelTextureStack;

static void pixeltexture_destroy(GF_Node *node, void *rs, Bool is_destroy)
{
	if (is_destroy) {
		PixelTextureStack *st = (PixelTextureStack *) gf_node_get_private(node);
		if (st->pixels) gf_free(st->pixels);
		gf_sc_texture_destroy(&st->txh);
		gf_free(st);
	}
}
static void pixeltexture_update(GF_TextureHandler *txh)
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
		pix_format = GF_PIXEL_RGB;
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

	if (!txh->tx_io) {
		gf_sc_texture_allocate(txh);
		if (!txh->tx_io) return;
	}

	if (st->pixels) gf_free(st->pixels);
	st->pixels = (char*)gf_malloc(sizeof(char) * stride * pt->image.height);
	/*FIXME FOR OPENGL !!*/
#if 0
	for (i=0; i<pt->image.height; i++) {
		memcpy(st->pixels + i * stride, pt->image.pixels + i * stride, stride);
	}
#else
	/*revert pixel ordering...*/
	for (i=0; i<pt->image.height; i++) {
		memcpy(st->pixels + i * stride, pt->image.pixels + (pt->image.height - 1 - i) * stride, stride);
	}
#endif

	txh->width = pt->image.width;
	txh->height = pt->image.height;
	txh->stride = stride;
	txh->pixelformat = pix_format;
	txh->data = st->pixels;

	gf_sc_texture_set_data(txh);
}

void compositor_init_pixeltexture(GF_Compositor *compositor, GF_Node *node)
{
	PixelTextureStack *st;
	GF_SAFEALLOC(st, PixelTextureStack);
	if (!st) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate pixel texture stack\n"));
		return;
	}
	gf_sc_texture_setup(&st->txh, compositor, node);
	st->pixels = NULL;
	st->txh.update_texture_fcnt = pixeltexture_update;

	gf_node_set_private(node, st);
	gf_node_set_callback_function(node, pixeltexture_destroy);
	st->txh.flags = 0;
	if (((M_PixelTexture*)node)->repeatS) st->txh.flags |= GF_SR_TEXTURE_REPEAT_S;
	if (((M_PixelTexture*)node)->repeatT) st->txh.flags |= GF_SR_TEXTURE_REPEAT_T;
}

GF_TextureHandler *pt_get_texture(GF_Node *node)
{
	PixelTextureStack *st = (PixelTextureStack *) gf_node_get_private(node);
	return &st->txh;
}

static void matte_update(GF_TextureHandler *txh)
{
	/*nothing to do*/
}

void compositor_init_mattetexture(GF_Compositor *compositor, GF_Node *node)
{
	GF_TextureHandler *txh;
	GF_SAFEALLOC(txh, GF_TextureHandler);
	if (!txh) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate matte texture stack\n"));
		return;
	}
	gf_sc_texture_setup(txh, compositor, node);
	txh->flags = GF_SR_TEXTURE_MATTE;
	txh->update_texture_fcnt = matte_update;
	gf_node_set_private(node, txh);
	gf_node_set_callback_function(node, imagetexture_destroy);
}

void gf_sc_mo_destroyed(GF_Node *n)
{
	void *st = gf_node_get_private(n);
	if (!st) return;

	switch (gf_node_get_tag(n)) {
	case TAG_MPEG4_Background2D:
		((Background2DStack *)st)->txh.stream = NULL;
		break;
	case TAG_MPEG4_Background:
	case TAG_X3D_Background:
#ifndef GPAC_DISABLE_3D
		((BackgroundStack *)st)->txh_back.stream = NULL;
		((BackgroundStack *)st)->txh_front.stream = NULL;
		((BackgroundStack *)st)->txh_left.stream = NULL;
		((BackgroundStack *)st)->txh_right.stream = NULL;
		((BackgroundStack *)st)->txh_top.stream = NULL;
		((BackgroundStack *)st)->txh_bottom.stream = NULL;
#endif
		break;
	case TAG_MPEG4_ImageTexture:
	case TAG_X3D_ImageTexture:
		((GF_TextureHandler *)st)->stream = NULL;
		break;
	case TAG_MPEG4_MovieTexture:
	case TAG_X3D_MovieTexture:
		((MovieTextureStack *)st)->txh.stream = NULL;
		break;
	case TAG_MPEG4_MediaSensor:
		((MediaSensorStack *)st)->stream = NULL;
		break;
	case TAG_MPEG4_MediaControl:
		((MediaControlStack *)st)->stream = NULL;
		break;
	case TAG_MPEG4_AudioSource:
		((AudioSourceStack *)st)->input.stream = NULL;
		break;
	case TAG_MPEG4_AudioClip:
	case TAG_X3D_AudioClip:
		((AudioClipStack *)st)->input.stream = NULL;
		break;
#ifndef GPAC_DISABLE_SVG
	case TAG_SVG_video:
	case TAG_SVG_image:
		((SVG_video_stack *)st)->txh.stream = NULL;
		break;
	case TAG_SVG_audio:
		((SVG_audio_stack *)st)->input.stream = NULL;
		break;
#endif
	default:
		break;
	}
}

#ifdef __cplusplus
}
#endif
#endif /*GPAC_DISABLE_VRML*/
