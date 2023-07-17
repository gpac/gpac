/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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
#include <windows.h>

#define MAX_AUDIO_BUFFER	30

typedef struct
{
	HWAVEOUT hwo;
	WAVEHDR wav_hdr[MAX_AUDIO_BUFFER];
	WAVEFORMATEX fmt;

	u32 num_buffers;
	u32 buffer_size;

	Bool exit_request;
	HANDLE event;

	u32 vol, pan;
	u32 delay, total_length_ms;

	Bool force_config;
	u32 cfg_num_buffers, cfg_duration;

	char *wav_buf;
} WAVContext;



#if !defined(DISABLE_WAVE_EX) && !defined(_WIN32_WCE)
/*IF YOU CAN'T COMPILE WAVEOUT TRY TO COMMENT THIS - note waveOut & multichannel is likely not to work*/
#define USE_WAVE_EXT
#endif


#ifdef USE_WAVE_EXT

/*for channel codes*/
#include <gpac/constants.h>

#ifndef WAVE_FORMAT_EXTENSIBLE
#define  WAVE_FORMAT_EXTENSIBLE   0xFFFE
#endif

#ifndef SPEAKER_FRONT_LEFT
#   define SPEAKER_FRONT_LEFT             0x1
#   define SPEAKER_FRONT_RIGHT            0x2
#   define SPEAKER_FRONT_CENTER           0x4
#   define SPEAKER_LOW_FREQUENCY          0x8
#   define SPEAKER_BACK_LEFT              0x10
#   define SPEAKER_BACK_RIGHT             0x20
#   define SPEAKER_FRONT_LEFT_OF_CENTER   0x40
#   define SPEAKER_FRONT_RIGHT_OF_CENTER  0x80
#   define SPEAKER_BACK_CENTER            0x100
#   define SPEAKER_SIDE_LEFT              0x200
#   define SPEAKER_SIDE_RIGHT             0x400
#   define SPEAKER_TOP_CENTER             0x800
#   define SPEAKER_TOP_FRONT_LEFT         0x1000
#   define SPEAKER_TOP_FRONT_CENTER       0x2000
#   define SPEAKER_TOP_FRONT_RIGHT        0x4000
#   define SPEAKER_TOP_BACK_LEFT          0x8000
#   define SPEAKER_TOP_BACK_CENTER        0x10000
#   define SPEAKER_TOP_BACK_RIGHT         0x20000
#   define SPEAKER_RESERVED               0x80000000
#endif

#ifndef _WAVEFORMATEXTENSIBLE_
typedef struct {
	WAVEFORMATEX    Format;
	union {
		WORD wValidBitsPerSample;       /* bits of precision  */
		WORD wSamplesPerBlock;          /* valid if wBitsPerSample==0 */
		WORD wReserved;                 /* If neither applies, set to zero. */
	} Samples;
	DWORD           dwChannelMask;      /* which channels are */
	/* present in stream  */
	GUID            SubFormat;
} WAVEFORMATEXTENSIBLE, *PWAVEFORMATEXTENSIBLE;
#endif

const static GUID  GPAC_KSDATAFORMAT_SUBTYPE_PCM = {0x00000001,0x0000,0x0010, {0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71} };

#pragma message("Using multichannel audio extensions")

#endif



#define WAVCTX()	WAVContext *ctx = (WAVContext *)dr->opaque;

static GF_Err WAV_Setup(GF_AudioOutput *dr, void *os_handle, u32 num_buffers, u32 total_duration)
{
	WAVCTX();

	ctx->force_config = (num_buffers && total_duration) ? GF_TRUE : GF_FALSE;
	ctx->cfg_num_buffers = num_buffers;
	if (ctx->cfg_num_buffers <= 1) ctx->cfg_num_buffers = 2;
	ctx->cfg_duration = total_duration;
	if (!ctx->force_config) ctx->num_buffers = 6;

	return GF_OK;
}

static void CALLBACK WaveProc(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
	GF_AudioOutput *dr = (GF_AudioOutput *) dwInstance;
	WAVCTX();

	if (uMsg != WOM_DONE) return;
	if (ctx->exit_request) return;
	SetEvent(ctx->event);
}


static void close_waveform(GF_AudioOutput *dr)
{
	WAVCTX();

	if (!ctx->event) return;

	/*brute-force version, actually much safer on winCE*/
#ifdef _WIN32_WCE
	ctx->exit_request = GF_TRUE;
	SetEvent(ctx->event);
	waveOutReset(ctx->hwo);
	waveOutClose(ctx->hwo);
	if (ctx->wav_buf) gf_free(ctx->wav_buf);
	ctx->wav_buf = NULL;
	CloseHandle(ctx->event);
	ctx->event = NULL;
	ctx->exit_request = GF_FALSE;
#else
	ctx->exit_request = GF_TRUE;
	SetEvent(ctx->event);
	if (ctx->hwo) {
		u32 i;
		Bool not_done;
		MMRESULT res;
		/*wait for all buffers to complete, otherwise this locks waveOutReset*/
		while (1) {
			not_done = GF_FALSE;
			for (i=0 ; i< ctx->num_buffers; i++) {
				if (! (ctx->wav_hdr[i].dwFlags & WHDR_DONE)) {
					not_done = GF_TRUE;
					break;
				}
			}
			if (!not_done) break;
			gf_sleep(60);
		}
		/*waveOutReset gives unpredictable results on PocketPC, so just close right away*/
		while (1) {
			res = waveOutClose(ctx->hwo);
			if (res == MMSYSERR_NOERROR) break;
		}
		ctx->hwo = NULL;
	}
	if (ctx->wav_buf) gf_free(ctx->wav_buf);
	ctx->wav_buf = NULL;
	CloseHandle(ctx->event);
	ctx->event = NULL;
	ctx->exit_request = GF_FALSE;
#endif
}

static void WAV_Shutdown(GF_AudioOutput *dr)
{
	close_waveform(dr);
}


/*we assume what was asked is what we got*/
static GF_Err WAV_Configure(GF_AudioOutput *dr, u32 *SampleRate, u32 *NbChannels, u32 *audioFormat, u64 channel_cfg)
{
	u32 i, retry;
	HRESULT	hr;
	WAVEFORMATEX *fmt;
#ifdef USE_WAVE_EXT
	WAVEFORMATEXTENSIBLE format_ex;
#endif

	WAVCTX();

	if (!ctx) return GF_BAD_PARAM;

	/*reset*/
	close_waveform(dr);

#ifndef USE_WAVE_EXT
	if (*NbChannels>2) *NbChannels=2;
#endif

	//only support for PCM 8/16/24/32 packet mode
	switch (*audioFormat) {
	case GF_AUDIO_FMT_U8:
	case GF_AUDIO_FMT_S16:
	case GF_AUDIO_FMT_S24:
	case GF_AUDIO_FMT_S32:
		break;
	default:
		//otherwise force PCM16
		*audioFormat = GF_AUDIO_FMT_S16;
		break;
	}

	memset (&ctx->fmt, 0, sizeof(ctx->fmt));
	ctx->fmt.cbSize = sizeof(WAVEFORMATEX);
	ctx->fmt.wFormatTag = WAVE_FORMAT_PCM;
	ctx->fmt.nChannels = *NbChannels;
	ctx->fmt.wBitsPerSample = gf_audio_fmt_bit_depth(*audioFormat);
	ctx->fmt.nSamplesPerSec = *SampleRate;
	ctx->fmt.nBlockAlign = ctx->fmt.wBitsPerSample * ctx->fmt.nChannels / 8;
	ctx->fmt.nAvgBytesPerSec = *SampleRate * ctx->fmt.nBlockAlign;
	fmt = &ctx->fmt;

#ifdef USE_WAVE_EXT
	if (channel_cfg && ctx->fmt.nChannels>2) {
		memset(&format_ex, 0, sizeof(WAVEFORMATEXTENSIBLE));
		format_ex.Format = ctx->fmt;
		format_ex.Format.cbSize = 22;
		format_ex.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
		format_ex.SubFormat = GPAC_KSDATAFORMAT_SUBTYPE_PCM;
		format_ex.Samples.wValidBitsPerSample = ctx->fmt.wBitsPerSample;
		format_ex.dwChannelMask = 0;
		if (channel_cfg & GF_AUDIO_CH_FRONT_LEFT) format_ex.dwChannelMask |= SPEAKER_FRONT_LEFT;
		if (channel_cfg & GF_AUDIO_CH_FRONT_RIGHT) format_ex.dwChannelMask |= SPEAKER_FRONT_RIGHT;
		if (channel_cfg & GF_AUDIO_CH_FRONT_CENTER) format_ex.dwChannelMask |= SPEAKER_FRONT_CENTER;
		if (channel_cfg & GF_AUDIO_CH_LFE) format_ex.dwChannelMask |= SPEAKER_LOW_FREQUENCY;
		if (channel_cfg & GF_AUDIO_CH_SURROUND_LEFT) format_ex.dwChannelMask |= SPEAKER_BACK_LEFT;
		if (channel_cfg & GF_AUDIO_CH_SURROUND_RIGHT) format_ex.dwChannelMask |= SPEAKER_BACK_RIGHT;
		if (channel_cfg & GF_AUDIO_CH_REAR_CENTER) format_ex.dwChannelMask |= SPEAKER_BACK_CENTER;
		if (channel_cfg & GF_AUDIO_CH_SIDE_SURROUND_LEFT) format_ex.dwChannelMask |= SPEAKER_SIDE_LEFT;
		if (channel_cfg & GF_AUDIO_CH_SIDE_SURROUND_RIGHT) format_ex.dwChannelMask |= SPEAKER_SIDE_RIGHT;
		fmt = (WAVEFORMATEX *) &format_ex;
	}
#endif

	/* Open a waveform device for output using window callback. */
	retry = 10;
	while (retry) {
		hr = waveOutOpen((LPHWAVEOUT)&ctx->hwo, WAVE_MAPPER, &ctx->fmt, (DWORD_PTR) WaveProc, (DWORD_PTR) dr,
		                 CALLBACK_FUNCTION | WAVE_ALLOWSYNC | WAVE_FORMAT_DIRECT
		                );

		if (hr == MMSYSERR_NOERROR) break;
		/*couldn't open audio*/
		if (hr != MMSYSERR_ALLOCATED) return GF_IO_ERR;
		retry--;
	}
	if (hr != MMSYSERR_NOERROR) return GF_IO_ERR;
	ctx->fmt.nBlockAlign = fmt->nBlockAlign;
	ctx->fmt.nAvgBytesPerSec = fmt->nAvgBytesPerSec;

	if (!ctx->force_config) {
		/*one wave buffer size*/
		ctx->buffer_size = 1024 * ctx->fmt.nBlockAlign;
		ctx->num_buffers = 2;
	} else {
		ctx->num_buffers = ctx->cfg_num_buffers;
		ctx->buffer_size = (ctx->fmt.nAvgBytesPerSec * ctx->cfg_duration) / (1000 * ctx->cfg_num_buffers);
	}

	ctx->event = CreateEvent( NULL, FALSE, FALSE, NULL);

	/*make sure we're aligned*/
	while (ctx->buffer_size % ctx->fmt.nBlockAlign) ctx->buffer_size++;

	ctx->wav_buf = (char*)gf_malloc(ctx->buffer_size*ctx->num_buffers*sizeof(char));
	memset(ctx->wav_buf, 0, ctx->buffer_size*ctx->num_buffers*sizeof(char));

	/*setup wave headers*/
	for (i=0 ; i < ctx->num_buffers; i++) {
		memset(& ctx->wav_hdr[i], 0, sizeof(WAVEHDR));
		ctx->wav_hdr[i].dwBufferLength = ctx->buffer_size;
		ctx->wav_hdr[i].lpData = & ctx->wav_buf[i*ctx->buffer_size];
		ctx->wav_hdr[i].dwFlags = WHDR_DONE;
		waveOutPrepareHeader(ctx->hwo, &ctx->wav_hdr[i], sizeof(WAVEHDR));
		waveOutWrite(ctx->hwo, &ctx->wav_hdr[i], sizeof(WAVEHDR));
		Sleep(1);
	}
	ctx->total_length_ms = 1000 * ctx->num_buffers * ctx->buffer_size / ctx->fmt.nAvgBytesPerSec;
	/*initial delay is full buffer size*/
	ctx->delay = ctx->total_length_ms;
	return GF_OK;
}

static void WAV_WriteAudio(GF_AudioOutput *dr)
{
	LPWAVEHDR hdr;
	u32 i;
	WAVCTX();

	if (!ctx->hwo) return;

	WaitForSingleObject(ctx->event, INFINITE);

	if (ctx->exit_request) return;

	for (i=0; i<ctx->num_buffers; i++) {
		/*get buffer*/
		hdr = &ctx->wav_hdr[i];

		if (hdr->dwFlags & WHDR_DONE) {
			waveOutUnprepareHeader(ctx->hwo, hdr, sizeof(WAVEHDR));

			hdr->dwBufferLength = ctx->buffer_size;
			/*update delay*/
			ctx->delay = 1000 * i * ctx->buffer_size / ctx->fmt.nAvgBytesPerSec;
			/*fill it*/
			hdr->dwBufferLength = dr->FillBuffer(dr->audio_renderer, hdr->lpData, ctx->buffer_size);
			hdr->dwFlags = 0;
			waveOutPrepareHeader(ctx->hwo, hdr, sizeof(WAVEHDR));
			/*write it*/
			waveOutWrite(ctx->hwo, hdr, sizeof(WAVEHDR));
		}
	}
}

static void WAV_Play(GF_AudioOutput *dr, u32 PlayType)
{
#if 0
	u32 i;
	WAVCTX();

	switch (PlayType) {
	case 0:
		waveOutPause(ctx->hwo);
		break;
	case 2:
		for (i=0; i<ctx->num_buffers; i++) {
			LPWAVEHDR hdr = &ctx->wav_hdr[i];
			memset(&hdr->lpData, 0, sizeof(char)*ctx->buffer_size);
		}
	case 1:
		waveOutRestart(ctx->hwo);
		break;
	}
#endif
}

#ifndef _WIN32_WCE
static void set_vol_pan(WAVContext *ctx)
{
	WORD rV, lV;
	/*in wave, volume & pan are specified as a DWORD. LOW word is LEFT channel, HIGH is right - iPaq doesn't support that*/
	lV = (WORD) (ctx->vol * ctx->pan / 100);
	rV = (WORD) (ctx->vol * (100 - ctx->pan) / 100);

	waveOutSetVolume(ctx->hwo, MAKELONG(lV, rV) );
}

static void WAV_SetVolume(GF_AudioOutput *dr, u32 Volume)
{
	WAVCTX();
	if (Volume > 100) Volume = 100;
	ctx->vol = Volume * 0xFFFF / 100;
	set_vol_pan(ctx);
}

static void WAV_SetPan(GF_AudioOutput *dr, u32 Pan)
{
	WAVCTX();
	if (Pan > 100) Pan = 100;
	ctx->pan = Pan;
	set_vol_pan(ctx);
}

#else
/*too much trouble with iPaq ...*/
static void WAV_SetVolume(GF_AudioOutput *dr, u32 Volume) { }
static void WAV_SetPan(GF_AudioOutput *dr, u32 Pan) { }

#endif


static GF_Err WAV_QueryOutputSampleRate(GF_AudioOutput *dr, u32 *desired_samplerate, u32 *NbChannels, u32 *nbBitsPerSample)
{
	/*iPaq's output frequencies available*/
#ifdef _WIN32_WCE
	switch (*desired_samplerate) {
	case 11025:
	case 22050:
		*desired_samplerate = 22050;
		break;
	case 8000:
	case 16000:
	case 32000:
		*desired_samplerate = 44100;
		break;
	case 24000:
	case 48000:
		*desired_samplerate = 44100;
		break;
	case 44100:
		*desired_samplerate = 44100;
		break;
	default:
		break;
	}
	return GF_OK;
#else
	return GF_OK;
#endif
}

static u32 WAV_GetAudioDelay(GF_AudioOutput *dr)
{
	WAVCTX();
	return ctx->delay;
}

static u32 WAV_GetTotalBufferTime(GF_AudioOutput *dr)
{
	WAVCTX();
	return ctx->total_length_ms;
}


void *NewWAVRender()
{
	GF_AudioOutput *driv;
	WAVContext *ctx = (WAVContext*)gf_malloc(sizeof(WAVContext));
	memset(ctx, 0, sizeof(WAVContext));
	ctx->num_buffers = 10;
	ctx->pan = 50;
	ctx->vol = 100;
	driv = (GF_AudioOutput*)gf_malloc(sizeof(GF_AudioOutput));
	memset(driv, 0, sizeof(GF_AudioOutput));
	GF_REGISTER_MODULE_INTERFACE(driv, GF_AUDIO_OUTPUT_INTERFACE, "mme", "gpac distribution")

	driv->opaque = ctx;

	driv->SelfThreaded = GF_FALSE;
	driv->Setup = WAV_Setup;
	driv->Shutdown = WAV_Shutdown;
	driv->Configure= WAV_Configure;
	driv->GetAudioDelay = WAV_GetAudioDelay;
	driv->GetTotalBufferTime = WAV_GetTotalBufferTime;
	driv->SetVolume = WAV_SetVolume;
	driv->SetPan = WAV_SetPan;
	driv->Play = WAV_Play;
	driv->QueryOutputSampleRate = WAV_QueryOutputSampleRate;
	driv->WriteAudio = WAV_WriteAudio;

	return driv;
}

void DeleteWAVRender(void *ifce)
{
	GF_AudioOutput *dr = (GF_AudioOutput *) ifce;
	WAVContext *ctx = (WAVContext *)dr->opaque;
	gf_free(ctx);
	gf_free(dr);
}

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
		return (GF_BaseInterface*)NewWAVRender();
	return NULL;
}

GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_AUDIO_OUTPUT_INTERFACE:
		DeleteWAVRender((GF_AudioOutput *) ifce);
		break;
	}
}

GPAC_MODULE_STATIC_DECLARATION( wave_out )
