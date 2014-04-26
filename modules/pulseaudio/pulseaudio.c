/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Pierre Souchay 2008
 *
 *  History:
 *
 *  2008/03/30 - v1.1 (Pierre Souchay)
 *    first revision
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

static const char *MODULE_NAME = "PulseAudio";

static const char *OUTPUT_NAME = "OutputName";

static const char *OUTPUT_DESCRIPTION = "OutputDescription";

static const char *DEFAULT_OUTPUT_NAME = "GPAC";

static const char *DEFAULT_OUTPUT_DESCRIPTION = "GPAC Output";

static GF_Err
PulseAudio_Setup (GF_AudioOutput * dr, void *os_handle,
                  u32 num_buffers, u32 total_duration)
{
	const char *opt;
	PulseAudioContext *ctx = (PulseAudioContext *) dr->opaque;
	if (ctx == NULL)
		return GF_BAD_PARAM;
	opt = gf_modules_get_option ((GF_BaseInterface *) dr, MODULE_NAME,
	                             OUTPUT_NAME);
	ctx->output_name = DEFAULT_OUTPUT_NAME;
	if (opt != NULL)
	{
		ctx->output_name = opt;
	}
	else
	{
		gf_modules_set_option ((GF_BaseInterface *) dr, MODULE_NAME,
		                       OUTPUT_NAME, DEFAULT_OUTPUT_NAME);
	}
	opt = gf_modules_get_option ((GF_BaseInterface *) dr, MODULE_NAME,
	                             OUTPUT_DESCRIPTION);
	ctx->output_description = DEFAULT_OUTPUT_DESCRIPTION;
	if (opt != NULL)
	{
		ctx->output_description = opt;
	}
	else
	{
		gf_modules_set_option ((GF_BaseInterface *) dr, MODULE_NAME,
		                       OUTPUT_DESCRIPTION, DEFAULT_OUTPUT_DESCRIPTION);
	}
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
PulseAudio_ConfigureOutput (GF_AudioOutput * dr, u32 * SampleRate,
                            u32 * NbChannels, u32 * nbBitsPerSample,
                            u32 channel_cfg)
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
		if (ctx->consecutive_zero_reads > 5) {
			gf_sleep(5);
		} else if (ctx->consecutive_zero_reads < 25) {
			gf_sleep(10);
		} else {
			gf_sleep(33);
		}
		return;
	}
	ctx->consecutive_zero_reads = 0;
	written = pa_simple_write (ctx->playback_handle, data, written, &pa_error);
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

static void
PulseAudio_SetVolume (GF_AudioOutput * dr, u32 Volume)
{
	GF_LOG (GF_LOG_WARNING, GF_LOG_MMIO,
	        ("[PulseAudio] Set volume to %lu: not yet implemented.\n", Volume));
}

static void
PulseAudio_SetPan (GF_AudioOutput * dr, u32 Pan)
{
}

static void
PulseAudio_SetPriority (GF_AudioOutput * dr, u32 Priority)
{
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

static GF_Err
PulseAudio_QueryOutputSampleRate (GF_AudioOutput * dr, u32 * desired_sr,
                                  u32 * NbChannels, u32 * nbBitsPerSample)
{
	/**
	 * PulseAudio can do the resampling by itself and play any number of channels
	 */
	return GF_OK;
}

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
	driv->ConfigureOutput = PulseAudio_ConfigureOutput;
	driv->GetAudioDelay = PulseAudio_GetAudioDelay;
	driv->SetVolume = PulseAudio_SetVolume;
	driv->SetPan = PulseAudio_SetPan;
	driv->SetPriority = PulseAudio_SetPriority;
	driv->QueryOutputSampleRate = PulseAudio_QueryOutputSampleRate;
	driv->WriteAudio = PulseAudio_WriteAudio;
	GF_REGISTER_MODULE_INTERFACE (driv, GF_AUDIO_OUTPUT_INTERFACE, MODULE_NAME, "gpac distribution");
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

GPAC_MODULE_STATIC_DELARATION( pulseaudio )
