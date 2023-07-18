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


#include "dx_hw.h"

#if (DIRECTSOUND_VERSION >= 0x0800)
#define USE_WAVE_EX
#endif

#ifdef USE_WAVE_EX
#include <ks.h>
#include <ksmedia.h>
const static GUID  GPAC_KSDATAFORMAT_SUBTYPE_PCM = {0x00000001,0x0000,0x0010,
	{0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71}
};
#endif


typedef HRESULT (WINAPI *DIRECTSOUNDCREATEPROC)(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter);

/*
		DirectSound audio output
*/
#define MAX_NUM_BUFFERS		20

typedef struct
{
	Bool force_config;
	u32 cfg_num_buffers, cfg_duration;

	HWND hWnd;
	LPDIRECTSOUND pDS;
	WAVEFORMATEX format;
	IDirectSoundBuffer *pOutput;

	u32 buffer_size, num_audio_buffer, total_audio_buffer_ms;

	/*notifs*/
	Bool use_notif;
	u32 frame_state[MAX_NUM_BUFFERS];
	DSBPOSITIONNOTIFY notif_events[MAX_NUM_BUFFERS];
	HANDLE events[MAX_NUM_BUFFERS];

	HMODULE hDSoundLib;
	DIRECTSOUNDCREATEPROC DirectSoundCreate;
} DSContext;

#define DSCONTEXT()		DSContext *ctx = (DSContext *)dr->opaque;

void DS_WriteAudio(GF_AudioOutput *dr);
void DS_WriteAudio_Notifs(GF_AudioOutput *dr);

static GF_Err DS_Setup(GF_AudioOutput *dr, void *os_handle, u32 num_buffers, u32 total_duration)
{
	HRESULT hr;

	DSCONTEXT();
	ctx->hWnd = (HWND) os_handle;
	/*check if we have created a HWND (this requires that video is handled by the DX module*/
	if (!ctx->hWnd) ctx->hWnd = DD_GetGlobalHWND();
	/*too bad, use desktop as window*/
	if (!ctx->hWnd) ctx->hWnd = GetDesktopWindow();

	ctx->force_config = (num_buffers && total_duration) ? GF_TRUE : GF_FALSE;
	ctx->cfg_num_buffers = num_buffers;
	ctx->cfg_duration = total_duration;
	if (ctx->cfg_num_buffers <= 1) ctx->cfg_num_buffers = 2;

	if ( FAILED( hr = ctx->DirectSoundCreate( NULL, &ctx->pDS, NULL ) ) ) return GF_IO_ERR;

	if( FAILED( hr = ctx->pDS->lpVtbl->SetCooperativeLevel(ctx->pDS, ctx->hWnd, DSSCL_EXCLUSIVE) ) ) {
		SAFE_DS_RELEASE( ctx->pDS );
		return GF_IO_ERR;
	}
	return GF_OK;
}


void DS_ResetBuffer(DSContext *ctx)
{
	VOID *pLock = NULL;
	DWORD size;

	if( FAILED(ctx->pOutput->lpVtbl->Lock(ctx->pOutput, 0, ctx->buffer_size*ctx->num_audio_buffer, &pLock, &size, NULL, NULL, 0 ) ) )
		return;
	memset(pLock, 0, (size_t) size);
	ctx->pOutput->lpVtbl->Unlock(ctx->pOutput, pLock, size, NULL, 0L);
}

void DS_ReleaseBuffer(GF_AudioOutput *dr)
{
	u32 i;
	DSCONTEXT();

	/*stop playing and notif proc*/
	if (ctx->pOutput) ctx->pOutput->lpVtbl->Stop(ctx->pOutput);
	SAFE_DS_RELEASE(ctx->pOutput);

	/*use notif, shutdown notifier and event watcher*/
	if (ctx->use_notif) {
		for (i=0; i<ctx->num_audio_buffer; i++) CloseHandle(ctx->events[i]);
	}
	ctx->use_notif = GF_FALSE;
}

static void DS_Shutdown(GF_AudioOutput *dr)
{
	DSCONTEXT();
	DS_ReleaseBuffer(dr);
	SAFE_DS_RELEASE(ctx->pDS );
}

/*we assume what was asked is what we got*/
static GF_Err DS_Configure(GF_AudioOutput *dr, u32 *SampleRate, u32 *NbChannels, u32 *audioFormat, u64 channel_cfg)
{
	u32 i;
	HRESULT hr;
	DSBUFFERDESC dsbBufferDesc;
	IDirectSoundNotify *pNotify;
#ifdef USE_WAVE_EX
	WAVEFORMATEXTENSIBLE format_ex;
#endif
	DSCONTEXT();

	DS_ReleaseBuffer(dr);
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
	ctx->format.nChannels = *NbChannels;
	ctx->format.wBitsPerSample = gf_audio_fmt_bit_depth( *audioFormat);
	ctx->format.nSamplesPerSec = *SampleRate;
	ctx->format.cbSize = sizeof (WAVEFORMATEX);
	ctx->format.wFormatTag = WAVE_FORMAT_PCM;
	ctx->format.nBlockAlign = ctx->format.nChannels * ctx->format.wBitsPerSample / 8;
	ctx->format.nAvgBytesPerSec = ctx->format.nSamplesPerSec * ctx->format.nBlockAlign;

	if (!ctx->format.nBlockAlign)
		return GF_BAD_PARAM;

	if (!ctx->force_config) {
		ctx->buffer_size = ctx->format.nBlockAlign * 1024;
		ctx->num_audio_buffer = 2;
	} else {
		ctx->num_audio_buffer = ctx->cfg_num_buffers;
		ctx->buffer_size = (ctx->format.nAvgBytesPerSec * ctx->cfg_duration) / (1000 * ctx->cfg_num_buffers);
	}

	/*make sure we're aligned*/
	while (ctx->buffer_size % ctx->format.nBlockAlign) ctx->buffer_size++;

	ctx->use_notif = GF_TRUE;
	if (gf_module_get_bool((GF_BaseInterface*)dr, "disable-notif"))
		ctx->use_notif = GF_FALSE;

	memset(&dsbBufferDesc, 0, sizeof(DSBUFFERDESC));
	dsbBufferDesc.dwSize = sizeof (DSBUFFERDESC);
	dsbBufferDesc.dwBufferBytes = ctx->buffer_size * ctx->num_audio_buffer;
	dsbBufferDesc.lpwfxFormat = &ctx->format;
	dsbBufferDesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS | DSBCAPS_CTRLVOLUME;
	if (ctx->use_notif) dsbBufferDesc.dwFlags |= DSBCAPS_CTRLPOSITIONNOTIFY;

#ifdef USE_WAVE_EX
	if (channel_cfg && ctx->format.nChannels>2) {
		memset(&format_ex, 0, sizeof(WAVEFORMATEXTENSIBLE));
		format_ex.Format = ctx->format;
		format_ex.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE);
		format_ex.SubFormat = GPAC_KSDATAFORMAT_SUBTYPE_PCM;
		format_ex.Samples.wValidBitsPerSample = gf_audio_fmt_bit_depth(*audioFormat);
		
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

		dsbBufferDesc.lpwfxFormat = (WAVEFORMATEX *) &format_ex;
	}
#endif


	hr = ctx->pDS->lpVtbl->CreateSoundBuffer(ctx->pDS, &dsbBufferDesc, &ctx->pOutput, NULL );
	if (FAILED(hr)) {
retry:
		if (ctx->use_notif) gf_opts_set_key("directsnd", "disable-notif", "yes");
		ctx->use_notif = GF_FALSE;
		dsbBufferDesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS;
		hr = ctx->pDS->lpVtbl->CreateSoundBuffer(ctx->pDS, &dsbBufferDesc, &ctx->pOutput, NULL );
		if (FAILED(hr)) {
			if (ctx->format.nChannels>2) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_AUDIO, ("[DirectSound] failed to configure output for %d channels (error %08x) - falling back to stereo\n", *NbChannels, hr));
				*NbChannels = 2;
				return DS_Configure(dr, SampleRate, NbChannels, audioFormat, 0);
			}
			return GF_IO_ERR;
		}
	}

	for (i=0; i<ctx->num_audio_buffer; i++) ctx->frame_state[i] = 0;

	if (ctx->use_notif) {
		hr = ctx->pOutput->lpVtbl->QueryInterface(ctx->pOutput, &IID_IDirectSoundNotify , (void **)&pNotify);
		if (hr == S_OK) {
			/*setup the notification positions*/
			for (i=0; i<ctx->num_audio_buffer; i++) {
				ctx->events[i] = CreateEvent( NULL, FALSE, FALSE, NULL );
				ctx->notif_events[i].hEventNotify = ctx->events[i];
				ctx->notif_events[i].dwOffset = ctx->buffer_size * i;
			}

			/*Tell DirectSound when to notify us*/
			hr = pNotify->lpVtbl->SetNotificationPositions(pNotify, ctx->num_audio_buffer, ctx->notif_events);

			if (hr != S_OK) {
				pNotify->lpVtbl->Release(pNotify);
				for (i=0; i<ctx->num_audio_buffer; i++) CloseHandle(ctx->events[i]);
				SAFE_DS_RELEASE(ctx->pOutput);
				goto retry;
			}

			pNotify->lpVtbl->Release(pNotify);
		} else {
			ctx->use_notif = GF_FALSE;
		}
	}
	if (ctx->use_notif) {
		dr->WriteAudio = DS_WriteAudio_Notifs;
	} else {
		dr->WriteAudio = DS_WriteAudio;
	}

	ctx->total_audio_buffer_ms = 1000 * ctx->buffer_size * ctx->num_audio_buffer / ctx->format.nAvgBytesPerSec;

	/*reset*/
	DS_ResetBuffer(ctx);
	/*play*/
	ctx->pOutput->lpVtbl->Play(ctx->pOutput, 0, 0, DSBPLAY_LOOPING);
	return GF_OK;
}

static Bool DS_RestoreBuffer(LPDIRECTSOUNDBUFFER pDSBuffer)
{
	DWORD dwStatus;
	pDSBuffer->lpVtbl->GetStatus(pDSBuffer, &dwStatus );
	if( dwStatus & DSBSTATUS_BUFFERLOST ) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DirectSound] buffer lost\n"));
		pDSBuffer->lpVtbl->Restore(pDSBuffer);
		pDSBuffer->lpVtbl->GetStatus(pDSBuffer, &dwStatus);
		if( dwStatus & DSBSTATUS_BUFFERLOST ) return GF_TRUE;
	}
	return GF_FALSE;
}



void DS_FillBuffer(GF_AudioOutput *dr, u32 buffer)
{
	HRESULT hr;
	VOID *pLock;
	u32 pos;
	DWORD size;
	DSCONTEXT();

	/*restoring*/
	if (DS_RestoreBuffer(ctx->pOutput)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DirectSound] restoring sound buffer\n"));
		return;
	}

	/*lock and fill from current pos*/
	pos = buffer * ctx->buffer_size;
	pLock = NULL;
	if( FAILED( hr = ctx->pOutput->lpVtbl->Lock(ctx->pOutput, pos, ctx->buffer_size,
	                 &pLock,  &size, NULL, NULL, 0L ) ) ) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DirectSound] Error locking sound buffer\n"));
		return;
	}

	assert(size == ctx->buffer_size);

	dr->FillBuffer(dr->audio_renderer, pLock, size);

	/*update current pos*/
	if( FAILED( hr = ctx->pOutput->lpVtbl->Unlock(ctx->pOutput, pLock, size, NULL, 0)) ) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DirectSound] Error unlocking sound buffer\n"));
	}
	ctx->frame_state[buffer] = 1;
}


void DS_WriteAudio_Notifs(GF_AudioOutput *dr)
{
	s32 i, inframe, nextframe;
	DSCONTEXT();

	inframe = WaitForMultipleObjects(ctx->num_audio_buffer, ctx->events, 0, 1000);
	if (inframe==WAIT_TIMEOUT) return;
	inframe -= WAIT_OBJECT_0;
	/*reset state*/
	ctx->frame_state[ (inframe + ctx->num_audio_buffer - 1) % ctx->num_audio_buffer] = 0;

	nextframe = (inframe + 1) % ctx->num_audio_buffer;
	for (i=nextframe; (i % ctx->num_audio_buffer) != (u32) inframe; i++) {
		u32 buf = i % ctx->num_audio_buffer;
		if (ctx->frame_state[buf]) continue;
		DS_FillBuffer(dr, buf);
	}
}

void DS_WriteAudio(GF_AudioOutput *dr)
{
	u32 retry;
	DWORD in_play, cur_play;
	DSCONTEXT();
	if (!ctx->pOutput) return;

	/*wait for end of current play buffer*/
	if (ctx->pOutput->lpVtbl->GetCurrentPosition(ctx->pOutput, &in_play, NULL) != DS_OK ) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DirectSound] error getting sound buffer positions\n"));
		return;
	}
	in_play = (in_play / ctx->buffer_size);
	retry = 6;
	while (retry) {
		if (ctx->pOutput->lpVtbl->GetCurrentPosition(ctx->pOutput, &cur_play, NULL) != DS_OK ) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DirectSound] error getting sound buffer positions\n"));
			return;
		}
		cur_play = (cur_play / ctx->buffer_size);
		if (cur_play == in_play) {
			gf_sleep(20);
			retry--;
		} else {
			/**/
			ctx->frame_state[in_play] = 0;
			DS_FillBuffer(dr, in_play);
			return;
		}
	}
}

static GF_Err DS_QueryOutputSampleRate(GF_AudioOutput *dr, u32 *desired_samplerate, u32 *NbChannels, u32 *nbBitsPerSample)
{
	/*all sample rates supported for now ... */
	return GF_OK;
}

static void DS_Play(GF_AudioOutput *dr, u32 PlayType)
{
	DSCONTEXT();
	switch (PlayType) {
	case 0:
		ctx->pOutput->lpVtbl->Stop(ctx->pOutput);
		break;
	case 2:
		DS_ResetBuffer(ctx);
	case 1:
		ctx->pOutput->lpVtbl->Play(ctx->pOutput, 0, 0, DSBPLAY_LOOPING);
		break;
	}
}

static void DS_SetVolume(GF_AudioOutput *dr, u32 Volume)
{
	LONG Vol;
	DSCONTEXT();
	if (!ctx->pOutput) return;
	if (Volume > 100) Vol = DSBVOLUME_MAX;
	else if(Volume == 0) Vol = DSBVOLUME_MIN;
	else Vol = DSBVOLUME_MIN/2 + Volume * (DSBVOLUME_MAX-DSBVOLUME_MIN/2) / 100;
	ctx->pOutput->lpVtbl->SetVolume(ctx->pOutput, Vol);
}

static void DS_SetPan(GF_AudioOutput *dr, u32 Pan)
{
	LONG dspan;
	DSCONTEXT();
	if (!ctx->pOutput) return;

	if (Pan > 100) Pan = 100;
	if (Pan > 50) {
		dspan = DSBPAN_RIGHT * (Pan - 50) / 50;
	} else if (Pan < 50) {
		dspan = DSBPAN_LEFT * (50 - Pan) / 50;
	} else {
		dspan = 0;
	}
	ctx->pOutput->lpVtbl->SetPan(ctx->pOutput, dspan);
}


static void DS_SetPriority(GF_AudioOutput *dr, u32 Priority)
{
}

static u32 DS_GetAudioDelay(GF_AudioOutput *dr)
{
	DSCONTEXT();
	return ctx->total_audio_buffer_ms;
}

static u32 DS_GetTotalBufferTime(GF_AudioOutput *dr)
{
	DSCONTEXT();
	return ctx->total_audio_buffer_ms;
}

static GF_GPACArg DSAudioArgs[] = {
	GF_DEF_ARG("disable-notif", NULL, "disable buffer position notifications", "false", NULL, GF_ARG_BOOL, 0),
	{0},
};

void *NewAudioOutput()
{
	HRESULT hr;
	DSContext *ctx;
	GF_AudioOutput *driv;

	if( FAILED( hr = CoInitialize(NULL) ) ) return NULL;


	ctx = (DSContext*)gf_malloc(sizeof(DSContext));
	memset(ctx, 0, sizeof(DSContext));
#ifdef UNICODE
	ctx->hDSoundLib = LoadLibrary(L"dsound.dll");
#else
	ctx->hDSoundLib = LoadLibrary("dsound.dll");
#endif
	if (ctx->hDSoundLib) {
		ctx->DirectSoundCreate = (DIRECTSOUNDCREATEPROC) GetProcAddress(ctx->hDSoundLib, "DirectSoundCreate");
	}
	if (!ctx->DirectSoundCreate) {
		if (ctx->hDSoundLib) FreeLibrary(ctx->hDSoundLib);
		gf_free(ctx);
		return NULL;
	}


	driv = (GF_AudioOutput*)gf_malloc(sizeof(GF_AudioOutput));
	memset(driv, 0, sizeof(GF_AudioOutput));
	GF_REGISTER_MODULE_INTERFACE(driv, GF_AUDIO_OUTPUT_INTERFACE, "directsnd", "gpac distribution");

	driv->opaque = ctx;

	driv->Setup = DS_Setup;
	driv->Shutdown = DS_Shutdown;
	driv->Configure = DS_Configure;
	driv->SetVolume = DS_SetVolume;
	driv->SetPan = DS_SetPan;
	driv->Play = DS_Play;
	driv->SetPriority = DS_SetPriority;
	driv->GetAudioDelay = DS_GetAudioDelay;
	driv->GetTotalBufferTime = DS_GetTotalBufferTime;
	driv->WriteAudio = DS_WriteAudio;
	driv->QueryOutputSampleRate = DS_QueryOutputSampleRate;
	/*never threaded*/
	driv->SelfThreaded = GF_FALSE;
	driv->args = DSAudioArgs;
	driv->description = "DirectSound Audio output";
	return driv;
}

void DeleteDxAudioOutput(void *ifce)
{
	GF_AudioOutput *dr = (GF_AudioOutput *)ifce;
	DSCONTEXT();

	if (ctx->hDSoundLib)
		FreeLibrary(ctx->hDSoundLib);
	gf_free(ctx);
	gf_free(ifce);
	CoUninitialize();
}

