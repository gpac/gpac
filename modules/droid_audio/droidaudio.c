/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Ivica Arsov, Jean Le Feuvre
 *			Copyright (c) Mines-Telecom 2009-
 *					All rights reserved
 *
 *  This file is part of GPAC / Wrapper
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

#include "javaenv.h"

#include <gpac/modules/audio_out.h>
#include <gpac/modules/droidaudio.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#define STREAM_MUSIC 3
#define CHANNEL_CONFIGURATION_MONO 2
#define CHANNEL_CONFIGURATION_STEREO 3
#define ENCODING_PCM_8BIT 3
#define ENCODING_PCM_16BIT 2
#define MODE_STREAM 1
#define CHANNEL_OUT_MONO 4
#define CHANNEL_IN_STEREO 12
#define CHANNEL_IN_MONO 16


/*for channel codes*/
#include <gpac/constants.h>



static const char android_device[] = "Android Default";

static GF_DroidAudio_EventCallback event_callback;
static void *event_callback_ctx;

/* Uncomment the next line if you want to debug */
/* #define DROID_EXTREME_LOGS */

typedef struct
{
	jclass cAudioTrack;
	jobject mTrack;

	jmethodID mAudioTrack;
	jmethodID mSetStereoVolume;
	jmethodID mGetMinBufferSize;
	jmethodID mPlay;
	jmethodID mStop;
	jmethodID mRelease;
	jmethodID mWriteB;
	jmethodID mWriteS;
	jmethodID mFlush;

	Bool configured;

	u32 num_buffers;

	u32 delay, total_length_ms;

	Bool force_config;
	u32 cfg_num_buffers, cfg_duration;

	u32 sampleRateInHz;
	u32 channelConfig; //AudioFormat.CHANNEL_OUT_MONO
	u32 audioFormat; //AudioFormat.ENCODING_PCM_16BIT
	s32 mbufferSizeInBytes;
	u32 volume;
	u32 pan;
	jarray buff;
} DroidContext;

/**
 * Register the event handler
 */
void gf_droidaudio_register_event_handler(GF_DroidAudio_EventCallback cb, void *ctx)
{
	event_callback = cb;
	event_callback_ctx = ctx;
}

static void dispatch_event(GF_DroidAudio_Event e)
{
	if (event_callback)
		event_callback(event_callback_ctx, e);
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
// Called by the main thread
static GF_Err WAV_Setup(GF_AudioOutput *dr, void *os_handle, u32 num_buffers, u32 total_duration)
{
	DroidContext *ctx = (DroidContext *)dr->opaque;
	JNIEnv *env = gf_droidaudio_jni_get_thread_env();
	int channels;
	int bytes;

	GF_LOG(GF_LOG_INFO, GF_LOG_AUDIO, ("[Android Audio] Setup for %d buffers", num_buffers));

	ctx->force_config = (num_buffers && total_duration) ? 1 : 0;
	ctx->cfg_num_buffers = num_buffers;
	if (ctx->cfg_num_buffers <= 1) ctx->cfg_num_buffers = 2;
	ctx->cfg_duration = total_duration;
	if (!ctx->force_config) ctx->num_buffers = 1;
	ctx->volume = 100;
	ctx->pan = 50;

	if (!ctx->cAudioTrack) {
		ctx->cAudioTrack = (*env)->FindClass(env, "android/media/AudioTrack");
		if (!ctx->cAudioTrack) {
			return GF_NOT_SUPPORTED;
		}

		ctx->cAudioTrack = (*env)->NewGlobalRef(env, ctx->cAudioTrack);

		ctx->mAudioTrack = (*env)->GetMethodID(env, ctx->cAudioTrack, "<init>", "(IIIIII)V");
		ctx->mGetMinBufferSize = (*env)->GetStaticMethodID(env, ctx->cAudioTrack, "getMinBufferSize", "(III)I");
		ctx->mPlay = (*env)->GetMethodID(env, ctx->cAudioTrack, "play", "()V");
		ctx->mStop = (*env)->GetMethodID(env, ctx->cAudioTrack, "stop", "()V");
		ctx->mRelease = (*env)->GetMethodID(env, ctx->cAudioTrack, "release", "()V");
		ctx->mWriteB = (*env)->GetMethodID(env, ctx->cAudioTrack, "write", "([BII)I");
		ctx->mWriteS = (*env)->GetMethodID(env, ctx->cAudioTrack, "write", "([SII)I");
		ctx->mFlush = (*env)->GetMethodID(env, ctx->cAudioTrack, "flush", "()V");
		ctx->mSetStereoVolume = (*env)->GetMethodID(env, ctx->cAudioTrack, "setStereoVolume", "(FF)I");
	}

	return GF_OK;
}

static void WAV_Deconfigure(DroidContext *ctx)
{
	if (!ctx->configured)
		return;

	JNIEnv* env = gf_droidaudio_jni_get_thread_env();
	(*env)->CallNonvirtualVoidMethod(env, ctx->mTrack, ctx->cAudioTrack, ctx->mStop);
	(*env)->CallNonvirtualVoidMethod(env, ctx->mTrack, ctx->cAudioTrack, ctx->mRelease);

	(*env)->PopLocalFrame(env, NULL);

	(*env)->DeleteGlobalRef(env, ctx->buff);
	ctx->buff = NULL;
	(*env)->DeleteGlobalRef(env, ctx->mTrack);
	ctx->mTrack = NULL;
	ctx->configured = GF_FALSE;
}

//----------------------------------------------------------------------
// Called by the audio thread
static void WAV_Shutdown(GF_AudioOutput *dr)
{
	GF_LOG(GF_LOG_INFO, GF_LOG_AUDIO, ("[Android Audio] Shutdown"));
	DroidContext *ctx = (DroidContext *)dr->opaque;
	JNIEnv* env = gf_droidaudio_jni_attach_current_thread();
	WAV_Deconfigure(ctx);
	(*env)->DeleteGlobalRef(env, ctx->cAudioTrack);
	ctx->cAudioTrack = NULL;
	gf_droidaudio_jni_detach_current_thread();
}


/*we assume what was asked is what we got*/
/* Called by the audio thread */
static GF_Err WAV_ConfigureOutput(GF_AudioOutput *dr, u32 *SampleRate, u32 *NbChannels, u32 *nbBitsPerSample, u32 channel_cfg)
{
	JNIEnv* env = gf_droidaudio_jni_attach_current_thread();
	u32 i;
	DroidContext *ctx = (DroidContext *)dr->opaque;

	GF_LOG(GF_LOG_INFO, GF_LOG_AUDIO, ("[Android Audio] Configure Output for %u channels...", *NbChannels));

	if (!ctx) return GF_BAD_PARAM;

	if (ctx->configured)
		WAV_Deconfigure(ctx);

	ctx->sampleRateInHz = *SampleRate;
	ctx->channelConfig = (*NbChannels == 1) ? CHANNEL_CONFIGURATION_MONO : CHANNEL_CONFIGURATION_STEREO; //AudioFormat.CHANNEL_CONFIGURATION_MONO
	ctx->audioFormat = (*nbBitsPerSample == 8)? ENCODING_PCM_8BIT : ENCODING_PCM_16BIT; //AudioFormat.ENCODING_PCM_16BIT
	GF_LOG(GF_LOG_INFO, GF_LOG_AUDIO, ("[Android Audio] SampleRate : %d",ctx->sampleRateInHz));
	GF_LOG(GF_LOG_INFO, GF_LOG_AUDIO, ("[Android Audio] BitPerSample : %d", *nbBitsPerSample));

	(*env)->PushLocalFrame(env, 2);

	ctx->num_buffers = 1;
	ctx->mbufferSizeInBytes = (*env)->CallStaticIntMethod(env, ctx->cAudioTrack, ctx->mGetMinBufferSize,
	                          ctx->sampleRateInHz, ctx->channelConfig, ctx->audioFormat);

	//ctx->mbufferSizeInBytes *= 3;

	GF_LOG(GF_LOG_INFO, GF_LOG_AUDIO, ("[Android Audio] Buffer Size : %d", ctx->mbufferSizeInBytes));

	i = 1;
	if ( ctx->channelConfig == CHANNEL_CONFIGURATION_STEREO )
		i *= 2;
	if ( ctx->audioFormat == ENCODING_PCM_16BIT )
		i *= 2;

	ctx->total_length_ms =  1000 * ctx->num_buffers * ctx->mbufferSizeInBytes / i / ctx->sampleRateInHz;

	GF_LOG(GF_LOG_INFO, GF_LOG_AUDIO, ("[Android Audio] Buffer Length ms : %d", ctx->total_length_ms));

	/*initial delay is full buffer size*/
	ctx->delay = ctx->total_length_ms;

	jobject track = (*env)->NewObject(env, ctx->cAudioTrack, ctx->mAudioTrack, STREAM_MUSIC, ctx->sampleRateInHz,
					  ctx->channelConfig, ctx->audioFormat, ctx->mbufferSizeInBytes, MODE_STREAM); //AudioTrack.MODE_STREAM
	if (track) {
		ctx->mTrack = (*env)->NewGlobalRef(env, track);
		(*env)->CallNonvirtualVoidMethod(env, ctx->mTrack, ctx->cAudioTrack, ctx->mPlay);
//	  (*env)->CallNonvirtualVoidMethod(env, ctx->mTrack, ctx->cAudioTrack, ctx->mStop);
	}  else {
		GF_LOG(GF_LOG_INFO, GF_LOG_AUDIO, ("[Android Audio] ctx->mTrack = %p", ctx->mTrack));
		return GF_NOT_SUPPORTED;
	}

	if ( ctx->audioFormat == ENCODING_PCM_8BIT )
		ctx->buff = (*env)->NewByteArray(env, ctx->mbufferSizeInBytes);
	else
		ctx->buff = (*env)->NewShortArray(env, ctx->mbufferSizeInBytes/2);
	if ( ctx->buff ) {
		ctx->buff = (*env)->NewGlobalRef(env, ctx->buff);
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_AUDIO, ("[Android Audio] ctx->buff = %p", ctx->buff));
		return GF_NOT_SUPPORTED;
	}

	ctx->configured = GF_TRUE;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_AUDIO, ("[Android Audio] ConfigureOutput DONE."));
	return GF_OK;
}

/* Called by the audio thread */
static void WAV_WriteAudio(GF_AudioOutput *dr)
{
	DroidContext *ctx = (DroidContext *)dr->opaque;
	if (!ctx)
		return;
	JNIEnv* env = gf_droidaudio_jni_get_thread_env();
	u32 written;
	void* pBuffer;
	if (!env)
		return;
#ifdef DROID_EXTREME_LOGS
	GF_LOG(GF_LOG_DEBUG, GF_LOG_AUDIO, ("[Android Audio] WAV_WriteAudio() : entering"));
#endif /* DROID_EXTREME_LOGS */
	dispatch_event(GF_DROIDAUDIO_ENTERING_CRITICAL);
	pBuffer = (*env)->GetPrimitiveArrayCritical(env, ctx->buff, NULL);
	if (pBuffer)
	{
		written = dr->FillBuffer(dr->audio_renderer, pBuffer, ctx->mbufferSizeInBytes);
		(*env)->ReleasePrimitiveArrayCritical(env, ctx->buff, pBuffer, 0);
		dispatch_event(GF_DROIDAUDIO_EXITING_CRITICAL);
		if (written)
		{
			if ( ctx->audioFormat == ENCODING_PCM_8BIT )
				(*env)->CallNonvirtualIntMethod(env, ctx->mTrack, ctx->cAudioTrack, ctx->mWriteB, ctx->buff, 0, ctx->mbufferSizeInBytes);
			else
				(*env)->CallNonvirtualIntMethod(env, ctx->mTrack, ctx->cAudioTrack, ctx->mWriteS, ctx->buff, 0, ctx->mbufferSizeInBytes/2);
		}
	}
	else
	{
		dispatch_event(GF_DROIDAUDIO_EXITING_CRITICAL);
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUDIO, ("[Android Audio] Failed to get pointer to array bytes = %p", pBuffer));
	}
#ifdef DROID_EXTREME_LOGS
	GF_LOG(GF_LOG_DEBUG, GF_LOG_AUDIO, ("[Android Audio] WAV_WriteAudio() : done"));
#endif /* DROID_EXTREME_LOGS */
}

/* Called by the main thread */
static void WAV_Play(GF_AudioOutput *dr, u32 PlayType)
{
	DroidContext *ctx = (DroidContext *)dr->opaque;
	JNIEnv* env = gf_droidaudio_jni_get_thread_env();

	GF_LOG(GF_LOG_DEBUG, GF_LOG_AUDIO, ("[Android Audio] Play: %d", PlayType));

	int detach = 0;
	if (!env) {
 		detach = 1;
		env = gf_droidaudio_jni_attach_current_thread();
	}

	switch ( PlayType )
	{
	case 0:
		// Stop playing
		(*env)->CallNonvirtualVoidMethod(env, ctx->mTrack, ctx->cAudioTrack, ctx->mStop);
		// Clear the internal buffers
		(*env)->CallNonvirtualVoidMethod(env, ctx->mTrack, ctx->cAudioTrack, ctx->mFlush);
		break;
	case 2:
		(*env)->CallNonvirtualVoidMethod(env, ctx->mTrack, ctx->cAudioTrack, ctx->mFlush);
	case 1:
		(*env)->CallNonvirtualVoidMethod(env, ctx->mTrack, ctx->cAudioTrack, ctx->mPlay);
		break;
	default:
		GF_LOG(GF_LOG_INFO, GF_LOG_AUDIO, ("[Android Audio] Unknown Play method=%d", PlayType));
	}

	if (detach)
		gf_droidaudio_jni_detach_current_thread();
}

static void WAV_UpdateVolume(DroidContext *ctx) {
	float lV, rV;
	JNIEnv* env = gf_droidaudio_jni_get_thread_env();
	if (!ctx)
		return;
	if (ctx->pan > 100)
		ctx->pan = 100;
	lV =rV = ctx->volume / 100.0;
	if (ctx->pan > 50) {
		float m = (100 - ctx->pan) / 50.0;
		lV*=m;
	} else if (ctx->pan < 50) {
		float m = ctx->pan / 50.0;
		rV*=m;
	}
	if (env && ctx->mSetStereoVolume && ctx->mTrack && ctx->cAudioTrack) {
		int success;
		if (0!= (success=((*env)->CallNonvirtualIntMethod(env, ctx->mTrack, ctx->cAudioTrack, ctx->mSetStereoVolume, lV, rV))))
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUDIO, ("SetVolume(%f,%f) returned Error code %d", lV, rV, success ));
	} else {
		GF_LOG(GF_LOG_INFO, GF_LOG_AUDIO, ("SetVolume(%f,%f)", lV, rV));
	}
}

static void WAV_SetVolume(GF_AudioOutput *dr, u32 Volume) {
	DroidContext *ctx = (DroidContext *)dr->opaque;
	ctx->volume = Volume;
	WAV_UpdateVolume(ctx);
}

static void WAV_SetPan(GF_AudioOutput *dr, u32 Pan)
{
	DroidContext *ctx = (DroidContext *)dr->opaque;
	WAV_UpdateVolume(ctx);
}

/* Called by the audio thread */
static GF_Err WAV_QueryOutputSampleRate(GF_AudioOutput *dr, u32 *desired_samplerate, u32 *NbChannels, u32 *nbBitsPerSample)
{
	DroidContext *ctx = (DroidContext *)dr->opaque;
	JNIEnv* env = gf_droidaudio_jni_get_thread_env();
	u32 sampleRateInHz, channelConfig, audioFormat;

#ifdef TEST_QUERY_SAMPLE
	sampleRateInHz = *desired_samplerate;
	channelConfig = (*NbChannels == 1) ? CHANNEL_CONFIGURATION_MONO : CHANNEL_CONFIGURATION_STEREO;
	audioFormat = (*nbBitsPerSample == 8)? ENCODING_PCM_8BIT : ENCODING_PCM_16BIT;

	GF_LOG(GF_LOG_INFO, GF_LOG_AUDIO, ("[Android Audio] Query: SampleRate ChannelConfig AudioFormat: %d %d %d",
					   sampleRateInHz,
					   (channelConfig == CHANNEL_CONFIGURATION_MONO)? 1 : 2,
					   (ctx->audioFormat == ENCODING_PCM_8BIT)? 8 : 16));

	switch (*desired_samplerate) {
	case 11025:
		*desired_samplerate = 11025;
		if ( (*env)->CallStaticIntMethod(env, ctx->cAudioTrack, ctx->ctx->mGetMinBufferSize,
		                                 *desired_samplerate, channelConfig, audioFormat) > 0 )
			return GF_OK;
	case 22050:
		*desired_samplerate = 22050;
		if ( (*env)->CallStaticIntMethod(env, ctx->cAudioTrack, ctx->ctx->mGetMinBufferSize,
		                                 *desired_samplerate, channelConfig, audioFormat) > 0 )
			return GF_OK;
		break;
	case 8000:
		*desired_samplerate = 8000;
		if ( (*env)->CallStaticIntMethod(env, ctx->cAudioTrack, ctx->mGetMinBufferSize,
		                                 *desired_samplerate, channelConfig, audioFormat) > 0 )
			return GF_OK;
	case 16000:
		*desired_samplerate = 16000;
		if ( (*env)->CallStaticIntMethod(env, ctx->cAudioTrack, ctx->mGetMinBufferSize,
		                                 *desired_samplerate, channelConfig, audioFormat) > 0 )
			return GF_OK;
	case 32000:
		*desired_samplerate = 32000;
		if ( (*env)->CallStaticIntMethod(env, ctx->cAudioTrack, ctx->mGetMinBufferSize,
		                                 *desired_samplerate, channelConfig, audioFormat) > 0 )
			return GF_OK;
		break;
	case 24000:
		*desired_samplerate = 24000;
		if ( (*env)->CallStaticIntMethod(env, ctx->cAudioTrack, ctx->mGetMinBufferSize,
		                                 *desired_samplerate, channelConfig, audioFormat) > 0 )
			return GF_OK;
	case 48000:
		*desired_samplerate = 48000;
		if ( (*env)->CallStaticIntMethod(env, ctx->cAudioTrack, ctx->mGetMinBufferSize,
		                                 *desired_samplerate, channelConfig, audioFormat) > 0 )
			return GF_OK;
		break;
	case 44100:
		*desired_samplerate = 44100;
		if ( (*env)->CallStaticIntMethod(env, ctx->cAudioTrack, ctx->mGetMinBufferSize,
		                                 *desired_samplerate, channelConfig, audioFormat) > 0 )
			return GF_OK;
		break;
	default:
		break;
	}
#endif

	return GF_OK;
}

static u32 WAV_GetAudioDelay(GF_AudioOutput *dr)
{
	DroidContext *ctx = (DroidContext *)dr->opaque;

	return ctx->delay;
}

static u32 WAV_GetTotalBufferTime(GF_AudioOutput *dr)
{
	DroidContext *ctx = (DroidContext *)dr->opaque;

	return ctx->total_length_ms;
}

//----------------------------------------------------------------------
void *NewWAVRender()
{
	DroidContext *ctx;
	GF_AudioOutput *driv;
	ctx = gf_malloc(sizeof(DroidContext));
	memset(ctx, 0, sizeof(DroidContext));
	ctx->num_buffers = 1;
	ctx->pan = 50;
	ctx->volume = 100;
	driv = gf_malloc(sizeof(GF_AudioOutput));
	memset(driv, 0, sizeof(GF_AudioOutput));
	GF_REGISTER_MODULE_INTERFACE(driv, GF_AUDIO_OUTPUT_INTERFACE, "Android Audio Output", "gpac distribution")

	driv->opaque = ctx;

	driv->SelfThreaded = 0;
	driv->Setup = WAV_Setup;
	driv->Shutdown = WAV_Shutdown;
	driv->ConfigureOutput = WAV_ConfigureOutput;
	driv->GetAudioDelay = WAV_GetAudioDelay;
	driv->GetTotalBufferTime = WAV_GetTotalBufferTime;
	driv->SetVolume = WAV_SetVolume;
	driv->SetPan = WAV_SetPan;
	driv->Play = WAV_Play;
	driv->QueryOutputSampleRate = WAV_QueryOutputSampleRate;
	driv->WriteAudio = WAV_WriteAudio;
	return driv;
}
//----------------------------------------------------------------------
void DeleteWAVRender(void *ifce)
{
	GF_AudioOutput *dr = (GF_AudioOutput *) ifce;
	if (!ifce)
		return;
	gf_free(dr);
}
//----------------------------------------------------------------------

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
	if (InterfaceType == GF_AUDIO_OUTPUT_INTERFACE) return NewWAVRender();
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

GPAC_MODULE_STATIC_DECLARATION( droid_audio )
