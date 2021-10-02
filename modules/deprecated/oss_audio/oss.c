/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / linux_oss audio render module
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


#if defined(__DARWIN__) || defined(__APPLE__)
#include <soundcard.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>

#else

#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>

#ifdef OSS_FIX_INC
#include <soundcard.h>
#else
#include <sys/soundcard.h>
#endif

#endif

#include <gpac/modules/audio_out.h>

#define OSS_AUDIO_DEVICE	"/dev/dsp"

typedef struct
{
	int audio_dev, sr, nb_ch;
	u32 buf_size, delay, num_buffers, total_duration;
	u32 force_sr;
	char *wav_buf;
} OSSContext;


#define OSSCTX()	OSSContext *ctx = (OSSContext *)dr->opaque;


static GF_Err OSS_Setup(GF_AudioOutput*dr, void *os_handle, u32 num_buffers, u32 total_duration)
{
	int audio;
	const char *opt;
	OSSCTX();

	opt = gf_opts_get_key("core", "force-alsarate");
	if (opt) ctx->force_sr = atoi(opt);

	/*open OSS in non-blocking mode*/
	audio = open(OSS_AUDIO_DEVICE, 0);
	if (audio < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[OSS] Cannot open audio device\n"));
		return GF_NOT_SUPPORTED;
	}

	/*set blocking mode back*/
	//fcntl(audio, F_SETFL, fcntl(audio, F_GETFL) & ~FNDELAY);
	ctx->audio_dev=audio;
	ctx->num_buffers = num_buffers;
	ctx->total_duration = total_duration;
	return GF_OK;
}

static void OSS_Shutdown(GF_AudioOutput*dr)
{
	OSSCTX();
	ioctl(ctx->audio_dev,SNDCTL_DSP_RESET,NULL);
	close(ctx->audio_dev);
	if (ctx->wav_buf) gf_free(ctx->wav_buf);
	ctx->wav_buf = NULL;
}


static GF_Err OSS_Configure(GF_AudioOutput*dr, u32 *SampleRate, u32 *NbChannels, u32 *audioFormat, u32 channel_cfg)
{
	int format, blockalign, nb_bufs, frag_spec;
	long flags;
	OSSCTX();

	if (!ctx) return GF_BAD_PARAM;
	/* reset and reopen audio-device */
	ioctl(ctx->audio_dev,SNDCTL_DSP_RESET,NULL);
	close(ctx->audio_dev);
	if (ctx->wav_buf) gf_free(ctx->wav_buf);
	ctx->wav_buf = NULL;
	ctx->audio_dev=open(OSS_AUDIO_DEVICE,O_WRONLY|O_NONBLOCK);
	if (!ctx->audio_dev) return GF_IO_ERR;

	/* Make the file descriptor use blocking writes with fcntl() so that
	 we don't have to handle sleep() ourselves*/
	flags = fcntl(ctx->audio_dev, F_GETFL);
	flags &= ~O_NONBLOCK;
	if (fcntl(ctx->audio_dev, F_SETFL, flags) < 0 ) return GF_IO_ERR;

	ctx->nb_ch = (int) (*NbChannels);
	if (ioctl(ctx->audio_dev, SNDCTL_DSP_CHANNELS, &ctx->nb_ch)==-1) return GF_IO_ERR;

	blockalign = ctx->nb_ch;

	//only support for PCM 8/16/24/32 packet mode
	switch (*audioFormat) {
	case GF_AUDIO_FMT_U8:
		format = AFMT_S8;
		break;
	default:
		//otherwise force PCM16
		*audioFormat = GF_AUDIO_FMT_S16;
	case GF_AUDIO_FMT_S16:
		blockalign *= 2;
		format = AFMT_S16_LE;
		break;
	}
	if(ioctl(ctx->audio_dev, SNDCTL_DSP_SETFMT,&format)==-1) return GF_IO_ERR;
	ctx->sr = (*SampleRate);
	if(ioctl(ctx->audio_dev, SNDCTL_DSP_SPEED,&ctx->sr)==-1) return GF_IO_ERR;

	nb_bufs = ctx->num_buffers ? : 8;
	ctx->buf_size = (*SampleRate * blockalign * ctx->total_duration) / (1000 * nb_bufs);
	frag_spec = 4;
	while (ctx->buf_size > (1<<(frag_spec+1)))
		frag_spec++;

	ctx->buf_size = 1<<frag_spec;

	ctx->delay = (1000*ctx->buf_size) / (*SampleRate * blockalign);
	frag_spec = ((nb_bufs<<16) & 0xFFFF0000) | frag_spec;

	ctx->delay = (1000*ctx->buf_size*nb_bufs) / (*SampleRate * blockalign);
	if ( ioctl(ctx->audio_dev, SNDCTL_DSP_SETFRAGMENT, &frag_spec) < 0 ) return GF_IO_ERR;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[OSS] setup %d buffers %d bytes each (%d ms buffer delay)", nb_bufs, ctx->buf_size, ctx->delay));
	ctx->wav_buf = gf_realloc(ctx->wav_buf, ctx->buf_size*sizeof(char));
	if(!ctx->wav_buf) return GF_OUT_OF_MEM;
	memset(ctx->wav_buf, 0, ctx->buf_size*sizeof(char));
	return GF_OK;
}

static void OSS_WriteAudio(GF_AudioOutput*dr)
{
	u32 written;
	OSSCTX();
	written = dr->FillBuffer(dr->audio_renderer, ctx->wav_buf, ctx->buf_size);
	/*this will also perform sleep*/
	if (written) {
		u32 reallyWritten = write(ctx->audio_dev, ctx->wav_buf, written);
		if (reallyWritten != written) {
			GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[OSS] Failed to write all audio to device, has written %u, should have %u", reallyWritten, written));
		}
	}
}

static void OSS_SetVolume(GF_AudioOutput*dr, u32 Volume) {}
static void OSS_SetPan(GF_AudioOutput*dr, u32 Pan) {}
static void OSS_SetPriority(GF_AudioOutput*dr, u32 Priority) {}
static u32 OSS_GetAudioDelay(GF_AudioOutput*dr)
{
	OSSCTX()
	return ctx->delay;
}

/*
 * to get the best matching samplerate the oss-device can be set up
 * with the desired sr. if not supported the returned value contains the
 * best matching sr.
 *
 * todo: supported samplerate could depend on nb_channels and format
 */
static GF_Err OSS_QueryOutputSampleRate(GF_AudioOutput*dr, u32 *desired_sr, u32 *NbChannels, u32 *nbBitsPerSample)
{
#ifdef FORCE_SR_LIMIT
	*NbChannels = 2;
	if (!( *desired_sr % 11025)) return GF_OK;
	if (*desired_sr<22050) *desired_sr = 22050;
	else *desired_sr = 44100;
	return GF_OK;
#else
	/* reset and reopen audio-device */
	int i;
	OSSCTX();
	if (ctx->force_sr) {
		*desired_sr = ctx->force_sr;
		return GF_OK;
	}
	i=*desired_sr;
	if(ioctl(ctx->audio_dev, SNDCTL_DSP_SPEED,&i)==-1) return GF_IO_ERR;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[OSS] uses samplerate %d for desired sr %d\n", i, *desired_sr));
	*desired_sr = i;
	i = *NbChannels;
	if(ioctl(ctx->audio_dev,SNDCTL_DSP_CHANNELS, &i)==-1) return GF_IO_ERR;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[OSS] uses %d channels for %d desired ones\n", i, *NbChannels));
	*NbChannels = i;
	if(ioctl(ctx->audio_dev, SNDCTL_DSP_SPEED,&ctx->sr)==-1) return GF_OK;
	if(ioctl(ctx->audio_dev,SNDCTL_DSP_CHANNELS, &ctx->nb_ch)==-1) return GF_OK;
	return GF_OK;
#endif
}

void *NewOSSRender()
{
	OSSContext *ctx;
	GF_AudioOutput*driv;
	ctx = gf_malloc(sizeof(OSSContext));
	if(!ctx)
		return NULL;
	memset(ctx, 0, sizeof(OSSContext));
	driv = gf_malloc(sizeof(GF_AudioOutput));
	if(!driv)
	{
		gf_free(ctx);
		ctx=NULL;
		return NULL;
	}
	memset(driv, 0, sizeof(GF_AudioOutput));
	driv->opaque = ctx;
	driv->SelfThreaded = 0;
	driv->Setup = OSS_Setup;
	driv->Shutdown = OSS_Shutdown;
	driv->Configure = OSS_Configure;
	driv->GetAudioDelay = OSS_GetAudioDelay;
	driv->SetVolume = OSS_SetVolume;
	driv->SetPan = OSS_SetPan;
	driv->SetPriority = OSS_SetPriority;
	driv->QueryOutputSampleRate = OSS_QueryOutputSampleRate;
	driv->WriteAudio = OSS_WriteAudio;

	GF_REGISTER_MODULE_INTERFACE(driv, GF_AUDIO_OUTPUT_INTERFACE, "OSS Audio Output", "gpac distribution");
	return driv;
}

void DeleteOSSRender(void *ifce)
{
	GF_AudioOutput*dr = (GF_AudioOutput*) ifce;
	OSSContext *ctx = (OSSContext *)dr->opaque;
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
		return NewOSSRender();
	return NULL;
}

GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	if (ifce->InterfaceType==GF_AUDIO_OUTPUT_INTERFACE)
		DeleteOSSRender((GF_AudioOutput*)ifce);
}

GPAC_MODULE_STATIC_DECLARATION( oss )
