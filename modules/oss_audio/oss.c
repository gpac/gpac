/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
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


#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>

#ifdef OSS_FIX_INC
#include <soundcard.h>
#else
#include <sys/soundcard.h>
#endif

#include <gpac/modules/audio_out.h>

#define OSS_AUDIO_DEVICE	"/dev/dsp"

typedef struct 
{
	int audio_dev;
	u32 buf_size, delay, num_buffers, total_duration;
	char *wav_buf;
} OSSContext;


#define OSSCTX()	OSSContext *ctx = (OSSContext *)dr->opaque;


static GF_Err OSS_Setup(GF_AudioOutput*dr, void *os_handle, u32 num_buffers, u32 total_duration)
{
	int audio;
	OSSCTX();
	/*open OSS in non-blocking mode*/
	if((audio=open(OSS_AUDIO_DEVICE,O_WRONLY|O_NDELAY))==-1) 
	  return GF_NOT_SUPPORTED;

	/*set blocking mode back*/
	fcntl(audio, F_SETFL, fcntl(audio, F_GETFL) & ~FNDELAY);
	ctx->audio_dev=audio;
	ctx->num_buffers = num_buffers;
	ctx->total_duration = total_duration;
	return GF_OK;
}

static void OSS_Shutdown(GF_AudioOutput*dr)
{
	OSSCTX();
	ioctl(ctx->audio_dev,SNDCTL_DSP_RESET);
	close(ctx->audio_dev);
	if (ctx->wav_buf) free(ctx->wav_buf);
	ctx->wav_buf = NULL;
}


static GF_Err OSS_ConfigureOutput(GF_AudioOutput*dr, u32 *SampleRate, u32 *NbChannels, u32 *nbBitsPerSample, u32 channel_cfg)
{
	int i, format, blockalign, frag_size, nb_bufs, frag_spec;
	long flags;
	OSSCTX();

	if (!ctx) return GF_BAD_PARAM;
	/* reset and reopen audio-device */
	ioctl(ctx->audio_dev,SNDCTL_DSP_RESET);
	close(ctx->audio_dev);
	if (ctx->wav_buf) free(ctx->wav_buf);
	ctx->wav_buf = NULL;
	ctx->audio_dev=open(OSS_AUDIO_DEVICE,O_WRONLY);
	blockalign = i =(*NbChannels);

	/* Make the file descriptor use blocking writes with fcntl() so that 
	 we don't have to handle sleep() ourselves*/
	flags = fcntl(ctx->audio_dev, F_GETFL);
	flags &= ~O_NONBLOCK;
	if (fcntl(ctx->audio_dev, F_SETFL, flags) < 0 ) return GF_IO_ERR;

	if (ioctl(ctx->audio_dev, SNDCTL_DSP_CHANNELS,&i)==-1) return GF_IO_ERR;	
	if ((*nbBitsPerSample) == 16) {
	  blockalign *= 2;
	  format = AFMT_S16_LE;
	} else {
	  format = AFMT_S8;
	}
	if(ioctl(ctx->audio_dev, SNDCTL_DSP_SETFMT,&format)==-1) return GF_IO_ERR;
	i=(*SampleRate);
	if(ioctl(ctx->audio_dev, SNDCTL_DSP_SPEED,&i)==-1) return GF_IO_ERR;
	if (ctx->num_buffers && ctx->total_duration) {
		frag_size = (*SampleRate) * ctx->total_duration * blockalign;
		frag_size /= (1000 * ctx->num_buffers);
		nb_bufs = ctx->num_buffers;
	} else {
		frag_size = 1024*blockalign;
		nb_bufs = 2;
	}
	frag_spec = 0;
	while ( (0x01<<frag_spec) < frag_size) frag_spec++;
	if (frag_spec>10) frag_spec--;

	ctx->buf_size = 0x01 << frag_spec;
	ctx->delay = (1000*ctx->buf_size) / (*SampleRate * blockalign);
	frag_spec = ((nb_bufs<<16) & 0xFFFF0000) | frag_spec;
	
	ctx->delay = (1000*ctx->buf_size) / (*SampleRate * blockalign);
	if ( ioctl(ctx->audio_dev, SNDCTL_DSP_SETFRAGMENT, &frag_spec) < 0 ) return GF_IO_ERR;

	//fprintf(stdout, "OSS setup %d buffers %d bytes each (%d ms buffer delay)", nb_bufs, ctx->buf_size, ctx->delay);
	ctx->wav_buf = realloc(ctx->wav_buf, ctx->buf_size*sizeof(char));
	if(!ctx->wav_buf) return GF_OUT_OF_MEM;
	memset(ctx->wav_buf, 0, ctx->buf_size*sizeof(char));
	return GF_OK;
}

static void OSS_WriteAudio(GF_AudioOutput*dr)
{
	OSSCTX();
	dr->FillBuffer(dr->audio_renderer, ctx->wav_buf, ctx->buf_size);
	/*this will also perform sleep*/
	write(ctx->audio_dev, ctx->wav_buf, ctx->buf_size);
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
static u32 OSS_QueryOutputSampleRate(GF_AudioOutput*dr, u32 desired_sr, u32 NbChannels, u32 nbBitsPerSample)
{
	int i;
	OSSCTX();
	return desired_sr;
	/* reset and reopen audio-device */
	ioctl(ctx->audio_dev,SNDCTL_DSP_RESET);
	close(ctx->audio_dev);
	ctx->audio_dev=open(OSS_AUDIO_DEVICE,O_WRONLY);
	i=desired_sr;
	if(ioctl(ctx->audio_dev, SNDCTL_DSP_SPEED,&i)==-1) return 44100;
	return i;
}

void *NewOSSRender()
{
	OSSContext *ctx;
	GF_AudioOutput*driv;
	ctx = malloc(sizeof(OSSContext));
	if(!ctx)
		return NULL;
	memset(ctx, 0, sizeof(OSSContext));
	driv = malloc(sizeof(GF_AudioOutput));
	if(!driv)
	{
		free(ctx);
		ctx=NULL;
		return NULL;
	}
	memset(driv, 0, sizeof(GF_AudioOutput));
	driv->opaque = ctx;
	driv->SelfThreaded = 0;
	driv->Setup = OSS_Setup;
	driv->Shutdown = OSS_Shutdown;
	driv->ConfigureOutput = OSS_ConfigureOutput;
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
	free(ctx);
	free(dr);
}


/*
 * ********************************************************************
 * interface
 */
Bool QueryInterface(u32 InterfaceType)
{
	if (InterfaceType == GF_AUDIO_OUTPUT_INTERFACE) 
		return 1;
	return 0;
}

GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	if (InterfaceType == GF_AUDIO_OUTPUT_INTERFACE) 
		return NewOSSRender();
	return NULL;
}

void ShutdownInterface(GF_BaseInterface *ifce)
{
	if (ifce->InterfaceType==GF_AUDIO_OUTPUT_INTERFACE)
		DeleteOSSRender((GF_AudioOutput*)ifce);
}
