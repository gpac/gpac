/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Pierre Souchay , Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2008-2019
 *					All rights reserved
 *
 *
 *  PulseAudio output module : output audio thru the PulseAudio daemon
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


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <poll.h>
#include <pulse/pulseaudio.h>
#include <pulse/simple.h>
#include <gpac/modules/audio_out.h>

typedef struct
{
	pa_simple *playback_handle;
	pa_sample_spec sample_spec;
	const char *output_name;
	const char *output_description;
	u32 errors;
	u32 consecutive_zero_reads;
} PulseAudioContext;

static void
free_pulseaudio_resources (GF_AudioOutput * dr)
{
	PulseAudioContext *ctx;
	if (dr == NULL)
		return;
	ctx = (PulseAudioContext *) dr->opaque;
	if (ctx == NULL)
		return;
	if (ctx->playback_handle != NULL)
	{
		pa_simple_free (ctx->playback_handle);
	}
	ctx->playback_handle = NULL;
}

static GF_Err
PulseAudio_Setup (GF_AudioOutput * dr, void *os_handle,
                  u32 num_buffers, u32 total_duration)
{
	const char *opt;
	PulseAudioContext *ctx = (PulseAudioContext *) dr->opaque;
	if (ctx == NULL)
		return GF_BAD_PARAM;
	opt = gf_module_get_key(dr, "name");
	ctx->output_name = opt ? ctx->output_name : "GPAC";

	opt = gf_module_get_key(dr, "description");
	ctx->output_description = opt  ? opt  : "GPAC Output";
	return GF_OK;
}

static void
PulseAudio_Shutdown (GF_AudioOutput * dr)
{
	int pa_error = 0;
	PulseAudioContext *ctx = (PulseAudioContext *) dr->opaque;
	if (ctx == NULL)
		return;
	if (ctx->playback_handle)
	{
		GF_LOG (GF_LOG_DEBUG, GF_LOG_MMIO,
		        ("[PulseAudio] Closing PulseAudio output\n"));
		pa_simple_drain (ctx->playback_handle, &pa_error);
		if (pa_error)
		{
			GF_LOG (GF_LOG_ERROR, GF_LOG_MMIO,
			        ("[PulseAudio] Error while closing PulseAudio output: %s\n",
			         pa_strerror (pa_error)));

		}
	}
}

static GF_Err
PulseAudio_Configure(GF_AudioOutput *dr, u32 *SampleRate, u32 *NbChannels, u32 *audioFormat, u64 channel_cfg)
{
	int pa_error = 0;
	PulseAudioContext *ctx = (PulseAudioContext *) dr->opaque;
	if (ctx->playback_handle != NULL)
	{
		GF_LOG (GF_LOG_DEBUG, GF_LOG_MMIO,
		        ("[PulseAudio] PulseAudio output already configured !\n"));
		/* Should not happen */
		pa_simple_flush (ctx->playback_handle, &pa_error);
		pa_simple_free (ctx->playback_handle);
		ctx->playback_handle = NULL;
	}

	//only support for PCM 16
	*audioFormat = GF_AUDIO_FMT_S16;

	ctx->consecutive_zero_reads = 0;
	ctx->sample_spec.format = PA_SAMPLE_S16NE;
	ctx->sample_spec.channels = *NbChannels;
	ctx->sample_spec.rate = *SampleRate;
	ctx->playback_handle = pa_simple_new (NULL,
	                                      ctx->output_name,
	                                      PA_STREAM_PLAYBACK,
	                                      NULL,
	                                      ctx->output_description,
	                                      &(ctx->sample_spec),
	                                      NULL, NULL, &pa_error);
	if (ctx->playback_handle == NULL || pa_error != 0)
	{
		GF_LOG (GF_LOG_ERROR, GF_LOG_MMIO,
		        ("[PulseAudio] Error while allocating PulseAudio output: %s.\n",
		         pa_strerror (pa_error)));
		return GF_IO_ERR;
	}
	GF_LOG (GF_LOG_DEBUG, GF_LOG_MMIO, ("[PulseAudio] Initialized - sampling rate %d - %d channels\n", ctx->sample_spec.rate, ctx->sample_spec.channels));
	return GF_OK;
}

#define BUFF_SIZE 8192

static void
PulseAudio_WriteAudio (GF_AudioOutput * dr)
{
	char data[BUFF_SIZE];
	int written = 0;
	int pa_error = 0;
	PulseAudioContext *ctx = (PulseAudioContext *) dr->opaque;
	if (ctx == NULL || ctx->playback_handle == NULL)
	{
		if (ctx == NULL || ctx->errors == 0)
		{
			if (ctx != NULL)
				ctx->errors++;
			GF_LOG (GF_LOG_ERROR, GF_LOG_MMIO,
			        ("[PulseAudio] unable to connect to a PulseAudio daemon!\n"))
		}
		return;
	}
	written = dr->FillBuffer (dr->audio_renderer, data, BUFF_SIZE / 4);
	if (written <= 0)
	{
		ctx->consecutive_zero_reads++;
		if (ctx->consecutive_zero_reads < 5) {
			gf_sleep(5);
		} else if (ctx->consecutive_zero_reads < 25) {
			gf_sleep(10);
		} else {
			gf_sleep(33);
		}
		return;
	}
	ctx->consecutive_zero_reads = 0;
	/*written = */pa_simple_write (ctx->playback_handle, data, written, &pa_error);
	if (pa_error != 0)
	{
		if (ctx->errors < 1)
			GF_LOG (GF_LOG_ERROR, GF_LOG_MMIO,
			        ("[PulseAudio] Write failure: %s\n", pa_strerror (pa_error)));
		ctx->errors++;
	}
	else
	{
		ctx->errors = 0;
	}
}

static u32
PulseAudio_GetAudioDelay (GF_AudioOutput * dr)
{
	pa_usec_t delay = 0;
	int pa_error = 0;
	u32 ms_delay = 0;
	PulseAudioContext *ctx = (PulseAudioContext *) dr->opaque;
	if (ctx == NULL || ctx->playback_handle == NULL)
	{
		GF_LOG (GF_LOG_ERROR, GF_LOG_MMIO,
		        ("[PulseAudio] missing connection to pulseaudio daemon!\n"))
		return 0;
	}
	delay = pa_simple_get_latency (ctx->playback_handle, &pa_error);
	if (pa_error)
	{
		GF_LOG (GF_LOG_ERROR, GF_LOG_MMIO,
		        ("[PulseAudio] Error while retrieving pulseaudio delay: %s.\n",
		         pa_strerror (pa_error)));
		return 0;
	}
	ms_delay = (u32) (delay / 1000);
	GF_LOG (GF_LOG_DEBUG, GF_LOG_MMIO, ("[PulseAudio] Audio delay: %llu us.\n",
	                                    delay));
	return ms_delay;
}

static GF_GPACArg PulseAudioArgs[] = {
	GF_DEF_ARG("name", NULL, "name for PulseAudio", "GPAC", NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("description", NULL, "description for PulseAudio", "GPAC output", NULL, GF_ARG_STRING, 0),
	{0},
};

void *
NewPulseAudioOutput ()
{
	PulseAudioContext *ctx;
	GF_AudioOutput *driv;
	GF_SAFEALLOC (ctx, PulseAudioContext);
	if (!ctx)
		return NULL;

	GF_SAFEALLOC (driv, GF_AudioOutput);
	if (!driv)
	{
		gf_free(ctx);
		return NULL;
	}
	driv->opaque = ctx;
	ctx->playback_handle = NULL;
	ctx->errors = 0;
	driv->SelfThreaded = 0;
	driv->Setup = PulseAudio_Setup;
	driv->Shutdown = PulseAudio_Shutdown;
	driv->Configure = PulseAudio_Configure;
	driv->GetAudioDelay = PulseAudio_GetAudioDelay;
	driv->WriteAudio = PulseAudio_WriteAudio;
	driv->description = "Audio output using PulseAudio";
	driv->args = PulseAudioArgs;

	GF_REGISTER_MODULE_INTERFACE (driv, GF_AUDIO_OUTPUT_INTERFACE, "pulse", "gpac distribution");
	return driv;
}

void
DeletePulseAudioOutput (void *ifce)
{
	GF_AudioOutput *dr = (GF_AudioOutput *) ifce;
	free_pulseaudio_resources (dr);
	if (dr != NULL) {
		if (dr->opaque)
			gf_free(dr->opaque);
		dr->opaque = NULL;
		gf_free(dr);
	}
}


/*
 * ********************************************************************
 * interface
 */
GPAC_MODULE_EXPORT
const u32 *QueryInterfaces(u32 InterfaceType)
{
	static u32 si [] = {
		GF_AUDIO_OUTPUT_INTERFACE,
		0
	};
	return si;
}

GPAC_MODULE_EXPORT
GF_BaseInterface *LoadInterface (u32 InterfaceType)
{
	if (InterfaceType == GF_AUDIO_OUTPUT_INTERFACE)
		return NewPulseAudioOutput ();
	return NULL;
}

GPAC_MODULE_EXPORT
void ShutdownInterface (GF_BaseInterface * ifce)
{
	if (ifce->InterfaceType == GF_AUDIO_OUTPUT_INTERFACE)
		DeletePulseAudioOutput ((GF_AudioOutput *) ifce);
}

GPAC_MODULE_STATIC_DECLARATION( pulseaudio )
