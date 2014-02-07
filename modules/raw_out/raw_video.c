/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre 
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / DirectX audio and video render module
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



/*driver interfaces*/
#include <gpac/modules/video_out.h>
#include <gpac/modules/audio_out.h>
#include <gpac/user.h>
#include <gpac/list.h>
#include <gpac/constants.h>

#include <gpac/setup.h>

typedef struct
{
	char *pixels;
	u32 width, height;
	u32 pixel_format, bpp;
	Bool passthrough;

	u32 sample_rate, nb_channels, chan_cfg;
} RawContext;

#define RAWCTX	RawContext *rc = (RawContext *)dr->opaque

static GF_Err raw_resize(GF_VideoOutput *dr, u32 w, u32 h)
{
	RAWCTX;
	if (rc->pixels) gf_free(rc->pixels);
	rc->width = w;
	rc->height = h;
	rc->pixels = gf_malloc(sizeof(char) * rc->bpp * w * h);
	if (!rc->pixels) return GF_OUT_OF_MEM;
	return GF_OK;
}

static GF_Err RAW_BlitPassthrough(GF_VideoOutput *dr, GF_VideoSurface *video_src, GF_Window *src_wnd, GF_Window *dst_wnd, u32 overlay_type)
{
	return GF_OK;
}

GF_Err RAW_Setup(GF_VideoOutput *dr, void *os_handle, void *os_display, u32 init_flags)
{
	const char *opt;
	RAWCTX;

	opt = gf_modules_get_option((GF_BaseInterface *)dr, "RAWVideo", "RawOutput");
	if (opt && !strcmp(opt, "null")) {
		rc->passthrough = 1;
		dr->Blit = RAW_BlitPassthrough;
		dr->hw_caps |= GF_VIDEO_HW_HAS_RGB | GF_VIDEO_HW_HAS_RGBA | GF_VIDEO_HW_HAS_STRETCH | GF_VIDEO_HW_HAS_YUV | GF_VIDEO_HW_OPENGL | GF_VIDEO_HW_HAS_YUV_OVERLAY;
	}

	if (init_flags & GF_TERM_WINDOW_TRANSPARENT) {
		rc->bpp = 4;
		rc->pixel_format = GF_PIXEL_ARGB;
	} else {
		rc->bpp = 3;
		rc->pixel_format = GF_PIXEL_RGB_24;
	}
	raw_resize(dr, 100, 100);
	return GF_OK;
}


static void RAW_Shutdown(GF_VideoOutput *dr)
{
	RAWCTX;
	if (rc->pixels) gf_free(rc->pixels);
	rc->pixels = NULL;
}


static GF_Err RAW_Flush(GF_VideoOutput *dr, GF_Window *dest)
{
#if 0
	RAWCTX;
	char szName[1024];
	sprintf(szName, "test%d.png", gf_sys_clock());
	gf_img_png_enc_file(rc->pixels, rc->width, rc->height, rc->width*rc->bpp, rc->pixel_format, szName);
#endif
	return GF_OK;
}

static GF_Err RAW_LockBackBuffer(GF_VideoOutput *dr, GF_VideoSurface *vi, Bool do_lock)
{
	RAWCTX;
	if (do_lock) {
		if (!vi) return GF_BAD_PARAM;
		memset(vi, 0, sizeof(GF_VideoSurface));
		vi->height = rc->height;
		vi->width = rc->width;
		vi->video_buffer = rc->pixels;
		vi->pitch_x = rc->bpp;
		vi->pitch_y = rc->bpp * vi->width;
		vi->pixel_format = rc->pixel_format;
	}
	return GF_OK;
}

static GF_Err RAW_ProcessEvent(GF_VideoOutput *dr, GF_Event *evt)
{
	if (evt) {
		switch (evt->type) {
		case GF_EVENT_VIDEO_SETUP:
			if (evt->setup.opengl_mode) return GF_NOT_SUPPORTED;
			return raw_resize(dr, evt->setup.width, evt->setup.height);
		}
	}
	return GF_OK;
}

GF_VideoOutput *NewRawVideoOutput()
{
	RawContext *pCtx;
	GF_VideoOutput *driv = (GF_VideoOutput *) gf_malloc(sizeof(GF_VideoOutput));
	memset(driv, 0, sizeof(GF_VideoOutput));
	GF_REGISTER_MODULE_INTERFACE(driv, GF_VIDEO_OUTPUT_INTERFACE, "Raw Video Output", "gpac distribution")

	pCtx = gf_malloc(sizeof(RawContext));
	memset(pCtx, 0, sizeof(RawContext));

	driv->opaque = pCtx;

	driv->Flush = RAW_Flush;
	driv->LockBackBuffer = RAW_LockBackBuffer;
	driv->Setup = RAW_Setup;
	driv->Shutdown = RAW_Shutdown;
	driv->ProcessEvent = RAW_ProcessEvent;
	return (void *)driv;
}

void DeleteVideoOutput(void *ifce)
{
	RawContext *rc;
	GF_VideoOutput *driv = (GF_VideoOutput *) ifce;
	rc = (RawContext *)driv->opaque;
	gf_free(rc);
	gf_free(driv);
}


static GF_Err RAW_AudioSetup(GF_AudioOutput *dr, void *os_handle, u32 num_buffers, u32 total_duration)
{
	return GF_OK;
}

static void RAW_AudioShutdown(GF_AudioOutput *dr)
{
}

/*we assume what was asked is what we got*/
static GF_Err RAW_ConfigureOutput(GF_AudioOutput *dr, u32 *SampleRate, u32 *NbChannels, u32 *nbBitsPerSample, u32 channel_cfg)
{
	RAWCTX;
	rc->sample_rate = *SampleRate;
	rc->nb_channels = *NbChannels;
	rc->chan_cfg = channel_cfg;
	return GF_OK;
}

static void RAW_WriteAudio(GF_AudioOutput *dr)
{
	char buf[4096];
	dr->FillBuffer(dr->audio_renderer, buf, 4096);
}

static void RAW_Play(GF_AudioOutput *dr, u32 PlayType)
{
}

static void RAW_SetVolume(GF_AudioOutput *dr, u32 Volume)
{
}

static void RAW_SetPan(GF_AudioOutput *dr, u32 Pan)
{
}

static GF_Err RAW_QueryOutputSampleRate(GF_AudioOutput *dr, u32 *desired_samplerate, u32 *NbChannels, u32 *nbBitsPerSample)
{
	return GF_OK;
}

static u32 RAW_GetAudioDelay(GF_AudioOutput *dr)
{
	return 0;
}

static u32 RAW_GetTotalBufferTime(GF_AudioOutput *dr)
{
	return 0;
}

void *NewRawAudioOutput()
{
	RawContext *ctx;
	GF_AudioOutput *driv;
	ctx = gf_malloc(sizeof(RawContext));
	memset(ctx, 0, sizeof(RawContext));
	driv = gf_malloc(sizeof(GF_AudioOutput));
	memset(driv, 0, sizeof(GF_AudioOutput));
	GF_REGISTER_MODULE_INTERFACE(driv, GF_AUDIO_OUTPUT_INTERFACE, "Raw Audio Output", "gpac distribution")

	driv->opaque = ctx;

	driv->SelfThreaded = 0;
	driv->Setup = RAW_AudioSetup;
	driv->Shutdown = RAW_AudioShutdown;
	driv->ConfigureOutput = RAW_ConfigureOutput;
	driv->GetAudioDelay = RAW_GetAudioDelay;
	driv->GetTotalBufferTime = RAW_GetTotalBufferTime;
	driv->SetVolume = RAW_SetVolume;
	driv->SetPan = RAW_SetPan;
	driv->Play = RAW_Play;
	driv->QueryOutputSampleRate = RAW_QueryOutputSampleRate;
	driv->WriteAudio = RAW_WriteAudio;
	return driv;
}

void DeleteAudioOutput(void *ifce)
{
	GF_AudioOutput *dr = (GF_AudioOutput *) ifce;
	RawContext *ctx = (RawContext*)dr->opaque;
	gf_free(ctx);
	gf_free(dr);
}


#ifndef GPAC_STANDALONE_RENDER_2D

/*interface query*/
GPAC_MODULE_EXPORT
const u32 *QueryInterfaces() 
{
	static u32 si [] = {
		GF_VIDEO_OUTPUT_INTERFACE,
		GF_AUDIO_OUTPUT_INTERFACE,
		0
	};
	return si; 
}

/*interface create*/
GPAC_MODULE_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	if (InterfaceType == GF_VIDEO_OUTPUT_INTERFACE) return (GF_BaseInterface *) NewRawVideoOutput();
	if (InterfaceType == GF_AUDIO_OUTPUT_INTERFACE) return (GF_BaseInterface *) NewRawAudioOutput();
	return NULL;
}

/*interface destroy*/
GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_VIDEO_OUTPUT_INTERFACE:
		DeleteVideoOutput((GF_VideoOutput *)ifce);
		break;
	case GF_AUDIO_OUTPUT_INTERFACE:
		DeleteAudioOutput((GF_AudioOutput *)ifce);
		break;
	}
}


GPAC_MODULE_STATIC_DELARATION( raw_out )

#endif
