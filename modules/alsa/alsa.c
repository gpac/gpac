/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2019
 *					All rights reserved
 *
 *  This file is part of GPAC / alsa audio output module
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
#include <alsa/asoundlib.h>
#include <gpac/modules/audio_out.h>


typedef struct
{
	snd_pcm_t *playback_handle;
	u32 nb_ch, buf_size, delay, num_buffers, total_duration, block_align;
	u32 force_sr;
	const char *dev_name;
	char *wav_buf;
} ALSAContext;


static GF_Err ALSA_Setup(GF_AudioOutput*dr, void *os_handle, u32 num_buffers, u32 total_duration)
{
	int err;
	ALSAContext *ctx = (ALSAContext*)dr->opaque;


	ctx->force_sr = gf_module_get_int((GF_BaseInterface *)dr, "force-rate");
	ctx->dev_name = gf_module_get_key((GF_BaseInterface *)dr, "devname");
	if (!ctx->dev_name) ctx->dev_name = "hw:0,0";

	/*test device*/
	err = snd_pcm_open(&ctx->playback_handle, ctx->dev_name, SND_PCM_STREAM_PLAYBACK, 0);
	if (err < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[ALSA] Cannot open audio device %s: %s\n", ctx->dev_name, snd_strerror (err)) );
		return GF_IO_ERR;
	}
	ctx->num_buffers = num_buffers ? num_buffers : 2;
	ctx->total_duration = total_duration ? total_duration : 100;
	return GF_OK;
}

static void ALSA_Shutdown(GF_AudioOutput*dr)
{
	ALSAContext *ctx = (ALSAContext*)dr->opaque;
	if (ctx->playback_handle) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[ALSA] Closing alsa output\n") );
		snd_pcm_close(ctx->playback_handle);
		ctx->playback_handle = NULL;
	}
	if (ctx->wav_buf) gf_free(ctx->wav_buf);
	ctx->wav_buf = NULL;
}

static GF_Err ALSA_Configure(GF_AudioOutput*dr, u32 *SampleRate, u32 *NbChannels, u32 *audioFormat, u64 channel_cfg)
{
	snd_pcm_hw_params_t *hw_params = NULL;
	int err;
	int nb_bufs, sr, period_time;
	ALSAContext *ctx = (ALSAContext*)dr->opaque;

	if (!ctx) return GF_BAD_PARAM;

	/*close device*/
	if (ctx->playback_handle) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[ALSA] Closing audio device %s\n", ctx->dev_name ));
		snd_pcm_close(ctx->playback_handle);
		ctx->playback_handle = NULL;
	}
	if (ctx->wav_buf) gf_free(ctx->wav_buf);
	ctx->wav_buf = NULL;

	err = snd_pcm_open(&ctx->playback_handle, ctx->dev_name, SND_PCM_STREAM_PLAYBACK, 0/*SND_PCM_NONBLOCK*/);
	if (err < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[ALSA] Cannot open audio device %s: %s\n", ctx->dev_name, snd_strerror (err)) );
		return GF_IO_ERR;
	}

	err = snd_pcm_hw_params_malloc(&hw_params);
	if (err < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[ALSA] Cannot allocate hardware params: %s\n", snd_strerror (err)) );
		goto err_exit;
	}
	err = snd_pcm_hw_params_any(ctx->playback_handle, hw_params);
	if (err < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[ALSA] Cannot initialize hardware params: %s\n", snd_strerror (err)) );
		goto err_exit;
	}
	err = snd_pcm_hw_params_set_access(ctx->playback_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (err < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[ALSA] Cannot set access type: %s\n", snd_strerror (err)) );
		goto err_exit;
	}
	/*set output format*/
	ctx->nb_ch = (int) (*NbChannels);
	ctx->block_align = ctx->nb_ch;

	//only support for PCM 8/16/24/32 packet mode
	switch (*audioFormat) {
	case GF_AUDIO_FMT_U8:
		err = snd_pcm_hw_params_set_format(ctx->playback_handle, hw_params, SND_PCM_FORMAT_U8);
		break;
	default:
		*audioFormat = GF_AUDIO_FMT_S16;
	case GF_AUDIO_FMT_S16:
		ctx->block_align *= 2;
		err = snd_pcm_hw_params_set_format(ctx->playback_handle, hw_params, SND_PCM_FORMAT_S16_LE);
		break;
	}

	if (err < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[ALSA] Cannot set sample format: %s\n", snd_strerror (err)) );
		goto err_exit;
	}

	/*set output sample rate*/
	if (ctx->force_sr) *SampleRate = ctx->force_sr;
	sr = *SampleRate;
	err = snd_pcm_hw_params_set_rate_near(ctx->playback_handle, hw_params, SampleRate, 0);
	if (err < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[ALSA] Cannot set sample rate: %s\n", snd_strerror (err)) );
		goto err_exit;
	}
	if (sr != *SampleRate) {
		GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[ALSA] Sample rate %d not supported, using %d instead\n", sr, *SampleRate ) );
		sr = *SampleRate;
	}
	/*set output channels*/
	err = snd_pcm_hw_params_set_channels_near(ctx->playback_handle, hw_params, NbChannels);
	if (err < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[ALSA] Cannot set channel count: %s\n", snd_strerror (err)) );
		goto err_exit;
	}
	if (ctx->nb_ch != *NbChannels) {
		GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[ALSA] %d channels not supported - using %d instead\n", ctx->nb_ch, *NbChannels ) );
		ctx->block_align /= ctx->nb_ch;
		ctx->nb_ch = *NbChannels;
		ctx->block_align *= ctx->nb_ch;
	}
	/* Set number of buffers*/
	nb_bufs = ctx->num_buffers;
	err = snd_pcm_hw_params_set_periods_near(ctx->playback_handle, hw_params, &nb_bufs, 0);
	if (err < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[ALSA] Cannot set number of HW buffers (%d): %s\n", nb_bufs, snd_strerror(err) ));
		goto err_exit;
	}
	/* Set total buffer size*/
	ctx->buf_size = (sr * ctx->total_duration)/1000 / nb_bufs;
	err = snd_pcm_hw_params_set_period_size_near(ctx->playback_handle, hw_params, (snd_pcm_uframes_t *)&ctx->buf_size, 0);
	if (err < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[ALSA] Cannot set HW buffer size (%d): %s\n", ctx->buf_size, snd_strerror(err) ));
		goto err_exit;
	}

	err = snd_pcm_hw_params_get_buffer_size(hw_params, (snd_pcm_uframes_t *)&ctx->buf_size);
	if (err < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[ALSA] Cannot get HW buffer size (%d): %s\n", ctx->buf_size, snd_strerror(err) ));
		goto err_exit;
	}
	ctx->buf_size *= ctx->block_align;
	/*get period time*/
	snd_pcm_hw_params_get_period_time(hw_params, &period_time, 0);

	err = snd_pcm_hw_params(ctx->playback_handle, hw_params);
	if (err < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[ALSA] Cannot set parameters: %s\n", snd_strerror (err)) );
		goto err_exit;
	}
	snd_pcm_hw_params_free (hw_params);
	hw_params = NULL;

	ctx->delay = (ctx->buf_size*1000) / (sr*ctx->block_align);

	/*allocate a single buffer*/
	ctx->wav_buf = gf_malloc(ctx->buf_size*sizeof(char));
	if(!ctx->wav_buf) return GF_OUT_OF_MEM;
	memset(ctx->wav_buf, 0, ctx->buf_size*sizeof(char));
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[ALSA] Setup %d ch @ %d hz - %d periods of %d us - total buffer size %d - overall delay %d ms\n", ctx->nb_ch, sr, nb_bufs, period_time, ctx->buf_size, ctx->delay));

	return GF_OK;

err_exit:
	if (hw_params) snd_pcm_hw_params_free(hw_params);
	snd_pcm_close(ctx->playback_handle);
	ctx->playback_handle = NULL;
	return GF_IO_ERR;
}

static void ALSA_WriteAudio(GF_AudioOutput*dr)
{
	u32 written;
	snd_pcm_sframes_t nb_frames;
	int err;
	ALSAContext *ctx = (ALSAContext*)dr->opaque;

	/*wait ctx delay for device interrupt*/
	err = snd_pcm_wait(ctx->playback_handle, 1);
	if (err<0) {
		if (err == -EPIPE) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_MMIO, ("[ALSA] Broken connection to sound card - restoring!\n"));
			snd_pcm_prepare(ctx->playback_handle);
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[ALSA] error %s while waiting!\n", snd_strerror(err) ));
			return;
		}
	}

	nb_frames = snd_pcm_avail_update(ctx->playback_handle);
	if (nb_frames < 0) {
		if (nb_frames == -EPIPE) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[ALSA] an xrun occured!\n"));
			snd_pcm_prepare(ctx->playback_handle);
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[ALSA] unknown ALSA avail update return value (%d)\n", nb_frames));
		}
		return;
	}
	if (!nb_frames) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[ALSA] no frame to write\n" ));
		return;
	}

	//assert(nb_frames*ctx->block_align<=ctx->buf_size);
	written = dr->FillBuffer(dr->audio_renderer, ctx->wav_buf, (u32) (ctx->block_align*nb_frames) );
	if (!written) return;
	written /= ctx->block_align;

	err = snd_pcm_writei(ctx->playback_handle, ctx->wav_buf, written);
	if (err == -EPIPE ) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[ALSA] an xrun occured!\n"));
		snd_pcm_prepare(ctx->playback_handle);
		err = snd_pcm_writei(ctx->playback_handle, ctx->wav_buf, nb_frames);
	}
	if (err < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[ALSA] Write failure: %s\n", snd_strerror(err)));
	}
}

static u32 ALSA_GetAudioDelay(GF_AudioOutput*dr)
{
	ALSAContext *ctx = (ALSAContext*)dr->opaque;
	return ctx->delay;
}

static GF_Err ALSA_QueryOutputSampleRate(GF_AudioOutput*dr, u32 *desired_sr, u32 *NbChannels, u32 *nbBitsPerSample)
{
	ALSAContext *ctx = (ALSAContext*)dr->opaque;
	int err;
	snd_pcm_hw_params_t *hw_params = NULL;

	err = snd_pcm_hw_params_malloc(&hw_params);
	if (err < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[ALSA] Cannot allocate hardware params: %s\n", snd_strerror (err)) );
		goto err_exit;
	}
	err = snd_pcm_hw_params_any(ctx->playback_handle, hw_params);
	if (err < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[ALSA] Cannot initialize hardware params: %s\n", snd_strerror (err)) );
		goto err_exit;
	}

	err = snd_pcm_hw_params_set_rate_near(ctx->playback_handle, hw_params, desired_sr, 0);
	if (err < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[ALSA] Cannot check available sample rates: %s\n", snd_strerror (err)) );
		goto err_exit;
	}

	err = snd_pcm_hw_params_set_channels_near(ctx->playback_handle, hw_params, NbChannels);
	if (err < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[ALSA] Cannot check available channels: %s\n", snd_strerror (err)) );
		goto err_exit;
	}
	snd_pcm_hw_params_free (hw_params);
	hw_params = NULL;
	return GF_OK;
err_exit:
	snd_pcm_hw_params_free (hw_params);
	hw_params = NULL;
	return GF_IO_ERR;
}

static GF_GPACArg ALSAArgs[] = {
	GF_DEF_ARG("devname", NULL, "alsa device name", "hw:0,0", NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("force-rate", NULL, "force alsa sample rate", "0", NULL, GF_ARG_INT, 0),
	{0},
};

void *NewALSAOutput()
{
	ALSAContext *ctx;
	GF_AudioOutput*driv;
	GF_SAFEALLOC(ctx, ALSAContext);
	if(!ctx) return NULL;

	GF_SAFEALLOC(driv, GF_AudioOutput);
	if(!driv) {
		gf_free(ctx);
		return NULL;
	}
	driv->opaque = ctx;
	driv->SelfThreaded = 0;
	driv->Setup = ALSA_Setup;
	driv->Shutdown = ALSA_Shutdown;
	driv->Configure = ALSA_Configure;
	driv->GetAudioDelay = ALSA_GetAudioDelay;
	driv->QueryOutputSampleRate = ALSA_QueryOutputSampleRate;
	driv->WriteAudio = ALSA_WriteAudio;
	driv->args = ALSAArgs;
	driv->description = "Audio output using ASLA";

	GF_REGISTER_MODULE_INTERFACE(driv, GF_AUDIO_OUTPUT_INTERFACE, "alsa", "gpac distribution");
	return driv;
}

void DeleteALSAOutput(void *ifce)
{
	GF_AudioOutput*dr = (GF_AudioOutput*) ifce;
	ALSAContext *ctx = (ALSAContext *)dr->opaque;
	gf_free(ctx);
	gf_free(dr);
}


/*
 * ********************************************************************
 * interface
 */
GPAC_MODULE_EXPORT
const u32 *QueryInterfaces()
{
	static u32 si [] = {
		GF_AUDIO_OUTPUT_INTERFACE,
		0
	};
	return si;
}

GPAC_MODULE_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	if (InterfaceType == GF_AUDIO_OUTPUT_INTERFACE)
		return NewALSAOutput();
	return NULL;
}

GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	if (ifce->InterfaceType==GF_AUDIO_OUTPUT_INTERFACE)
		DeleteALSAOutput((GF_AudioOutput*)ifce);
}

GPAC_MODULE_STATIC_DECLARATION( alsa )
