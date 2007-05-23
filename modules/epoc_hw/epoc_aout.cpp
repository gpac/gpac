/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / wave audio render module
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


#include <gpac/modules/audio_out.h>
/*symbian audio stuff*/
#include <mdaaudiooutputstream.h>
#include <mda/common/audio.h>

#define EPOC_MAX_BUFFERS	8

enum {
	EPOC_AUDIO_INIT = 0,
	EPOC_AUDIO_OPEN,
	EPOC_AUDIO_PLAY,
	EPOC_AUDIO_ERROR,
};

class EPOCAudio : public MMdaAudioOutputStreamCallback
{
public:
	EPOCAudio();
	virtual ~EPOCAudio();

	virtual void MaoscOpenComplete( TInt aError );
	virtual void MaoscBufferCopied( TInt aError, const TDesC8& a_Buffer);
	virtual void MaoscPlayComplete( TInt );

	GF_Err Open(u32 sample_rate, Bool stereo);
	void Close(Bool and_wait);

	CMdaAudioOutputStream* m_stream;
	TMdaAudioDataSettings m_settings;
	u32 state;

	/*desired config*/
	u32 cfg_num_buffers, cfg_total_duration, init_vol, init_pan;
	/*actual config*/
	u32 num_buffers, total_duration, buffer_len;

	/*audio buffers*/
	char *buffers[EPOC_MAX_BUFFERS];
	TPtrC8 sent_buffers[EPOC_MAX_BUFFERS];
	u32 buffer_size;
	u32 current_buffer, nb_buffers_queued;
};

EPOCAudio::EPOCAudio()
{
	u32 i;
	m_stream = NULL;
	for (i=0; i<EPOC_MAX_BUFFERS; i++) buffers[i] = NULL;
	state = EPOC_AUDIO_INIT;
	num_buffers = 0;
	init_vol = 100;
}

EPOCAudio::~EPOCAudio()
{
	if (m_stream) {
		delete m_stream;
	}
}

GF_Err EPOCAudio::Open(u32 sample_rate, Bool stereo)
{
	TInt res = 0;
	u32 count;
	TMdaAudioDataSettings::TAudioCaps epoc_sr;

	
	switch (sample_rate) {
	case 8000: epoc_sr = TMdaAudioDataSettings::ESampleRate8000Hz; break;
	case 11025: epoc_sr = TMdaAudioDataSettings::ESampleRate11025Hz; break;
	case 12000: epoc_sr = TMdaAudioDataSettings::ESampleRate12000Hz; break;
	case 16000: epoc_sr = TMdaAudioDataSettings::ESampleRate16000Hz; break;
	case 22050: epoc_sr = TMdaAudioDataSettings::ESampleRate22050Hz; break;
	case 24000: epoc_sr = TMdaAudioDataSettings::ESampleRate24000Hz; break;
	case 32000: epoc_sr = TMdaAudioDataSettings::ESampleRate32000Hz; break;
	case 44100: epoc_sr = TMdaAudioDataSettings::ESampleRate44100Hz; break;
	case 48000: epoc_sr = TMdaAudioDataSettings::ESampleRate48000Hz; break;
	default:
		return GF_NOT_SUPPORTED;
	}
	
	state = EPOC_AUDIO_INIT;

	gf_sleep(10);
	TRAP(res, m_stream = CMdaAudioOutputStream::NewL(*this) );
	if ((res!=KErrNone) || !m_stream) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[EPOCAudio] Cannot create output audio stream\n"));
		return GF_IO_ERR;
	}
	m_stream->Open(&m_settings);

	/*wait for ack - if not getting it in 50*40 = 2sec, abort*/
	count = 50;
	while (count) {
		if (state == EPOC_AUDIO_OPEN) break;
		else if (state == EPOC_AUDIO_ERROR) {
			return GF_IO_ERR;
		}
		gf_sleep(40);

		TInt error;
		CActiveScheduler::RunIfReady(error, CActive::EPriorityIdle);
		count--;
	}
	if (state != EPOC_AUDIO_OPEN) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[EPOCAudio] Failed to open sound device - is it present?\n"));
		return GF_NOT_SUPPORTED;
	}

	TRAP(res, m_stream->SetAudioPropertiesL(epoc_sr, stereo ? TMdaAudioDataSettings::EChannelsStereo : TMdaAudioDataSettings::EChannelsMono) );
	m_stream->SetPriority(EPriorityAbsoluteHigh, EMdaPriorityPreferenceTime );
	m_stream->SetVolume(init_vol * m_stream->MaxVolume() / 100);

	current_buffer = nb_buffers_queued = 0;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[EPOCAudio] output audio stream ready - sample rate %d - %d channels\n", sample_rate, stereo ? 2 : 1));
	return GF_OK;
}

void EPOCAudio::Close(Bool and_wait)
{
	u32 i;
	if (m_stream) {
		if (state==EPOC_AUDIO_PLAY) {
#if 0
			m_stream->Stop();

			while (0 && and_wait) {

				if (state != EPOC_AUDIO_PLAY) break;
				gf_sleep(1);

				TInt error;
				CActiveScheduler::RunIfReady(error, CActive::EPriorityIdle);
			}
#endif
		}
		delete m_stream;
		m_stream = NULL;
	}
	for (i=0; i<num_buffers; i++) {
		if (buffers[i]) free(buffers[i]);
		buffers[i] = NULL;
	}
	num_buffers = 0;
	state = EPOC_AUDIO_INIT;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[EPOCAudio] output audio stream closed\n"));
}

void EPOCAudio::MaoscOpenComplete(TInt aError)
{
	if (aError) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[EPOCAudio] Failed to open sound device - error %d\n", aError));
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[EPOCAudio] Sound device opened\n", aError));
		state = EPOC_AUDIO_OPEN;
	}
}

void EPOCAudio::MaoscBufferCopied(TInt aError, const TDesC8& a_Buffer)
{
	assert(nb_buffers_queued);
	nb_buffers_queued--;
	state = nb_buffers_queued ? EPOC_AUDIO_PLAY : EPOC_AUDIO_OPEN;
}

void EPOCAudio::MaoscPlayComplete(TInt aError)
{
	if (aError) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[EPOCAudio] Playback stoped due to error %d\n", aError));
		state = EPOC_AUDIO_ERROR;
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[EPOCAudio] Playback stoped due to user request\n"));
		state = EPOC_AUDIO_OPEN;
	}
}


static GF_Err EAUD_Setup(GF_AudioOutput *dr, void *os_handle, u32 num_buffers, u32 total_duration)
{
	EPOCAudio *ctx = (EPOCAudio *)dr->opaque;
	ctx->cfg_num_buffers = MAX(num_buffers, EPOC_MAX_BUFFERS);
	ctx->cfg_total_duration = total_duration;
	return GF_OK;
}

static void EAUD_Shutdown(GF_AudioOutput *dr)
{
	EPOCAudio *ctx = (EPOCAudio *)dr->opaque;
	ctx->Close(1);
}


/*we assume what was asked is what we got*/
static GF_Err EAUD_ConfigureOutput(GF_AudioOutput *dr, u32 *SampleRate, u32 *NbChannels, u32 *nbBitsPerSample, u32 channel_cfg)
{
	u32 snd_align, bps, i;
	GF_Err e;
	EPOCAudio *ctx = (EPOCAudio *)dr->opaque;

	ctx->Close(1);

	*nbBitsPerSample = 16;
	if (*NbChannels > 2) *NbChannels = 2;

	e = ctx->Open(*SampleRate, (*NbChannels ==2) ? 1 : 0);
	if (e) return e;
	
	snd_align = *NbChannels * 2;
	bps = snd_align * *SampleRate;
	
	if (ctx->cfg_total_duration) {
		ctx->num_buffers = ctx->cfg_num_buffers;
		ctx->buffer_size = (bps*ctx->cfg_total_duration/1000) / ctx->num_buffers;
	} else {
		ctx->num_buffers = 4;
		/*use 25 ms buffers*/
		ctx->buffer_size = bps / 40;
	}
	ctx->buffer_size /= snd_align;
	ctx->buffer_size *= snd_align;

	ctx->buffer_len = ctx->buffer_size * 1000 / bps;
	ctx->total_duration = ctx->buffer_len * ctx->num_buffers;

	for (i=0; i<ctx->num_buffers; i++) {
		ctx->buffers[i] = (char *)malloc(sizeof(char)*ctx->buffer_size);
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[EPOCAudio] Output audio stream configured - %d buffers of %d ms each\n", ctx->num_buffers, ctx->buffer_len));

	return GF_OK;
}

static void EAUD_WriteAudio(GF_AudioOutput *dr)
{
	EPOCAudio *ctx = (EPOCAudio *)dr->opaque;

	/*no buffers available...*/
	if (ctx->nb_buffers_queued == ctx->num_buffers) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[EPOCAudio] Audio queue full - yielding to app\n"));
		User::After(0);
		TInt error;
		CActiveScheduler::RunIfReady(error, CActive::EPriorityIdle);
		return;
	}

	while (ctx->nb_buffers_queued < ctx->num_buffers) {
		u32 written = dr->FillBuffer(dr->audio_renderer, ctx->buffers[ctx->current_buffer], ctx->buffer_size);
		//GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[EPOCAudio] Filling audio buffer %d / %d\n", ctx->current_buffer, ctx->num_buffers));
		if (!written) return;

		ctx->sent_buffers[ctx->current_buffer].Set((const TText8 *) ctx->buffers[ctx->current_buffer], written);

		ctx->nb_buffers_queued++;
		ctx->m_stream->WriteL(ctx->sent_buffers[ctx->current_buffer]);
		ctx->current_buffer = (ctx->current_buffer + 1) % ctx->num_buffers;
	}
}

static void EAUD_Play(GF_AudioOutput *dr, u32 PlayType)
{
}

static void EAUD_SetVolume(GF_AudioOutput *dr, u32 Volume)
{
	EPOCAudio *ctx = (EPOCAudio *)dr->opaque;
	ctx->init_vol = Volume;
	if (ctx->m_stream)
		ctx->m_stream->SetVolume(ctx->init_vol * ctx->m_stream->MaxVolume() / 100);
}

static void EAUD_SetPan(GF_AudioOutput *dr, u32 Pan)
{
}


static GF_Err EAUD_QueryOutputSampleRate(GF_AudioOutput *dr, u32 *desired_samplerate, u32 *NbChannels, u32 *nbBitsPerSample)
{
	*nbBitsPerSample = 16;
	if (*NbChannels > 2) *NbChannels = 2;
	return GF_OK;
}

static u32 EAUD_GetAudioDelay(GF_AudioOutput *dr)
{
	EPOCAudio *ctx = (EPOCAudio *)dr->opaque;
	return ctx->current_buffer*ctx->buffer_len;
}

static u32 EAUD_GetTotalBufferTime(GF_AudioOutput *dr)
{
	EPOCAudio *ctx = (EPOCAudio *)dr->opaque;
	return ctx->total_duration;
}


#ifdef __cplusplus
extern "C" {
#endif

void *EPOC_aout_new()
{
	GF_AudioOutput *driv;
	driv = (GF_AudioOutput *) malloc(sizeof(GF_AudioOutput));
	memset(driv, 0, sizeof(GF_AudioOutput));
	GF_REGISTER_MODULE_INTERFACE(driv, GF_AUDIO_OUTPUT_INTERFACE, "EPOC Audio Output", "gpac distribution")

	driv->Setup = EAUD_Setup;
	driv->Shutdown = EAUD_Shutdown;
	driv->ConfigureOutput = EAUD_ConfigureOutput;
	driv->GetAudioDelay = EAUD_GetAudioDelay;
	driv->GetTotalBufferTime = EAUD_GetTotalBufferTime;
	driv->SetVolume = EAUD_SetVolume;
	driv->SetPan = EAUD_SetPan;
	driv->Play = EAUD_Play;
	driv->QueryOutputSampleRate = EAUD_QueryOutputSampleRate;
	driv->WriteAudio = EAUD_WriteAudio;

	driv->SelfThreaded = 0;
	driv->opaque = new EPOCAudio();

	return driv;
}

void EPOC_aout_del(void *ifce)
{
	GF_AudioOutput *dr = (GF_AudioOutput *) ifce;
	EPOCAudio *ctx = (EPOCAudio*)dr->opaque;
	delete ctx;
	free(dr);
}

#ifdef __cplusplus
}
#endif

