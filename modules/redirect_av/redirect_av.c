/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean le Feuvre
 *			Copyright (c) 2010-20XX Telecom ParisTech
 *			All rights reserved
 *
 *  This file is part of GPAC / AVI Recorder demo module
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


#include <gpac/modules/term_ext.h>
#include <gpac/internal/terminal_dev.h>
#include <gpac/internal/compositor_dev.h>

/*sample raw AVI writing*/
#include <gpac/internal/avilib.h>


typedef struct
{
	GF_Terminal *term;

	Bool is_open;
	GF_AudioListener audio_listen;
	GF_VideoListener video_listen;

	avi_t *avi_out; 

	char *frame;
	u32 size;
} GF_AVRedirect;

static void avr_on_audio_frame(void *udta, char *buffer, u32 buffer_size, u32 time, u32 delay_ms)
{
	GF_AVRedirect *avr = (GF_AVRedirect *)udta;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[AVRedirect] Audio frame size %d - delay %d\n", buffer_size, delay_ms));
	AVI_write_audio(avr->avi_out, buffer, buffer_size);
}

static void avr_on_audio_reconfig(void *udta, u32 samplerate, u32 bits_per_sample, u32 nb_channel, u32 channel_cfg)
{
	GF_AVRedirect *avr = (GF_AVRedirect *)udta;
	GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[AVRedirect] Audio reconfig sr %d bps %d nb_ch %d\n", samplerate, bits_per_sample, nb_channel));
	AVI_set_audio(avr->avi_out, nb_channel, samplerate, bits_per_sample, WAVE_FORMAT_PCM, 0);
}

static void avr_on_video_frame(void *udta, u32 time)
{
	u32 i, j;
	GF_Err e;
	GF_VideoSurface fb;
	GF_AVRedirect *avr = (GF_AVRedirect *)udta;

	e = gf_sc_get_screen_buffer(avr->term->compositor, &fb, 0);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[AVRedirect] Error grabing frame buffer %s\n", gf_error_to_string(e)));
		return;
	}
	/*convert frame*/
	for (i=0; i<fb.height; i++) {
		char *dst = avr->frame + i * fb.width * 3;
		char *src = fb.video_buffer + (fb.height-i-1) * fb.pitch_y;
		for (j=0; j<fb.width; j++) {
			dst[0] = src[0];
			dst[1] = src[1];
			dst[2] = src[2];
			src+=4;
			dst += 3;
		}
	}

	if (AVI_write_frame(avr->avi_out, avr->frame, avr->size, 1) <0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[AVRedirect] Error writing video frame\n"));
	}
	gf_sc_release_screen_buffer(avr->term->compositor, &fb);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[AVRedirect] Writing video frame\n"));
}

static void avr_on_video_reconfig(void *udta, u32 width, u32 height)
{
	char comp[5];
	GF_AVRedirect *avr = (GF_AVRedirect *)udta;
	GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[AVRedirect] Video reconfig width %d height %d\n", width, height));


	comp[0] = comp[1] = comp[2] = comp[3] = comp[4] = 0;
	AVI_set_video(avr->avi_out, width, height, 30, comp);

	if (avr->frame) free(avr->frame);
	avr->size = 3*width*height;
	avr->frame = malloc(sizeof(char)*avr->size);
}


static void avr_open(GF_AVRedirect *avr)
{
	if (avr->is_open) return;

	avr->avi_out = AVI_open_output_file("dump.avi");
	if (!avr->avi_out) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[AVRedirect] Error opening output AVI file\n"));
		return;
	}
	gf_sc_add_audio_listener(avr->term->compositor, &avr->audio_listen);
	gf_sc_add_video_listener(avr->term->compositor, &avr->video_listen);
	avr->is_open = 1;

	GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[AVRedirect] Open output AVI file %s\n", "dump.avi"));
}

static void avr_close(GF_AVRedirect *avr)
{
	if (!avr->is_open) return;
	gf_sc_remove_audio_listener(avr->term->compositor, &avr->audio_listen);
	gf_sc_remove_video_listener(avr->term->compositor, &avr->video_listen);
	avr->is_open = 0;
	AVI_close(avr->avi_out);
	GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[AVRedirect] Closing output AVI file\n"));
}

static Bool avr_process(GF_TermExt *termext, u32 action, void *param)
{
	const char *opt;
	GF_AVRedirect *avr = termext->udta;

	switch (action) {
	case GF_TERM_EXT_START:
		avr->term = (GF_Terminal *)param;
		opt = gf_modules_get_option((GF_BaseInterface*)termext, "AVRedirect", "Enabled");
		if (!opt || strcmp(opt, "yes")) return 0;
		termext->caps |= GF_TERM_EXTENSION_FILTER_EVENT;

		avr->audio_listen.udta = avr;
		avr->audio_listen.on_audio_frame = avr_on_audio_frame;
		avr->audio_listen.on_audio_reconfig = avr_on_audio_reconfig;
		avr->video_listen.udta = avr;
		avr->video_listen.on_video_frame = avr_on_video_frame;
		avr->video_listen.on_video_reconfig = avr_on_video_reconfig;
		return 1;

	case GF_TERM_EXT_STOP:
		avr->term = NULL;
		break;

	case GF_TERM_EXT_EVENT:
		{
			GF_Event *evt = (GF_Event *)param;
			switch (evt->type) {
			case GF_EVENT_CONNECT:
				if (evt->connect.is_connected) {
					avr_open(avr);
				} else {
					avr_close(avr);
				}
				break;
			}
		}
		break;

	/*if not threaded, perform our tasks here*/
	case GF_TERM_EXT_PROCESS:
		break;
	}
	return 0;
}


GF_TermExt *avr_new()
{
	GF_TermExt *dr;
	GF_AVRedirect *uir;
	dr = gf_malloc(sizeof(GF_TermExt));
	memset(dr, 0, sizeof(GF_TermExt));
	GF_REGISTER_MODULE_INTERFACE(dr, GF_TERM_EXT_INTERFACE, "GPAC Output Recorder", "gpac distribution");

	GF_SAFEALLOC(uir, GF_AVRedirect);
	dr->process = avr_process;
	dr->udta = uir;
	return dr;
}


void avr_delete(GF_BaseInterface *ifce)
{
	GF_TermExt *dr = (GF_TermExt *) ifce;
	GF_AVRedirect *uir = dr->udta;
	gf_free(uir);
	gf_free(dr);
}

GF_EXPORT
const u32 *QueryInterfaces() 
{
	static u32 si [] = {
		GF_TERM_EXT_INTERFACE,
		0
	};
	return si;
}

GF_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	if (InterfaceType == GF_TERM_EXT_INTERFACE) return (GF_BaseInterface *)avr_new();
	return NULL;
}

GF_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_TERM_EXT_INTERFACE:
		avr_delete(ifce);
		break;
	}
}
