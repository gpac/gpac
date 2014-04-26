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

#ifdef GPAC_STATIC_MODULES

JavaVM* GetJavaVM();
JNIEnv* GetEnv();

#endif



static const char android_device[] = "Android Default";

static jclass cAudioTrack = NULL;
static jobject mtrack = NULL;

static jmethodID mAudioTrack;
static jmethodID setStereoVolume;
static jmethodID mGetMinBufferSize;
static jmethodID mPlay;
static jmethodID mStop;
static jmethodID mRelease;
static jmethodID mWriteB;
static jmethodID mWriteS;
static jmethodID mFlush;

#include <android/log.h>
#define TAG "GPAC Android Audio"
#define LOGV(X, Y)  __android_log_print(ANDROID_LOG_VERBOSE, TAG, X, Y)
#define LOGV3(X, Y, Z, K)  __android_log_print(ANDROID_LOG_VERBOSE, TAG, X, Y, Z, K)
#define LOGD(X, Y)  __android_log_print(ANDROID_LOG_DEBUG, TAG, X, Y)
#define LOGD2(X, Y, Z)  __android_log_print(ANDROID_LOG_DEBUG, TAG, X, Y, Z)
#define LOGE(X, Y)  __android_log_print(ANDROID_LOG_ERROR, TAG, X, Y)
#define LOGE3(X, Y, Z, W)  __android_log_print(ANDROID_LOG_ERROR, TAG, X, Y, Z, W)
#define LOGW(X, Y)  __android_log_print(ANDROID_LOG_WARN, TAG, X, Y)
#define LOGI(X, Y)  __android_log_print(ANDROID_LOG_INFO, TAG, X, Y)


/* Uncomment the next line if you want to debug */
/* #define DROID_EXTREME_LOGS */

typedef struct
{
	JNIEnv* env;

	jobject mtrack;

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

//----------------------------------------------------------------------
//----------------------------------------------------------------------
// Called by the main thread
static GF_Err WAV_Setup(GF_AudioOutput *dr, void *os_handle, u32 num_buffers, u32 total_duration)
{
	DroidContext *ctx = (DroidContext *)dr->opaque;
	JNIEnv* env = GetEnv();
	int channels;
	int bytes;
	LOGV("[Android Audio] Setup for %d buffers", num_buffers);

	ctx->force_config = (num_buffers && total_duration) ? 1 : 0;
	ctx->cfg_num_buffers = num_buffers;
	if (ctx->cfg_num_buffers <= 1) ctx->cfg_num_buffers = 2;
	ctx->cfg_duration = total_duration;
	if (!ctx->force_config) ctx->num_buffers = 1;
	ctx->volume = 100;
	ctx->pan = 50;

	if (!cAudioTrack) {
		cAudioTrack = (*env)->FindClass(env, "android/media/AudioTrack");
		if (!cAudioTrack) {
			return GF_NOT_SUPPORTED;
		}

		cAudioTrack = (*env)->NewGlobalRef(env, cAudioTrack);

		mAudioTrack = (*env)->GetMethodID(env, cAudioTrack, "<init>", "(IIIIII)V");
		mGetMinBufferSize = (*env)->GetStaticMethodID(env, cAudioTrack, "getMinBufferSize", "(III)I");
		mPlay = (*env)->GetMethodID(env, cAudioTrack, "play", "()V");
		mStop = (*env)->GetMethodID(env, cAudioTrack, "stop", "()V");
		mRelease = (*env)->GetMethodID(env, cAudioTrack, "release", "()V");
		mWriteB = (*env)->GetMethodID(env, cAudioTrack, "write", "([BII)I");
		mWriteS = (*env)->GetMethodID(env, cAudioTrack, "write", "([SII)I");
		mFlush = (*env)->GetMethodID(env, cAudioTrack, "flush", "()V");
		setStereoVolume = (*env)->GetMethodID(env, cAudioTrack, "setStereoVolume", "(FF)I");
	}

	return GF_OK;
}
//----------------------------------------------------------------------
// Called by the audio thread
static void WAV_Shutdown(GF_AudioOutput *dr)
{
	DroidContext *ctx = (DroidContext *)dr->opaque;
	JNIEnv* env = NULL;
	jint res = 0;

	LOGV("[Android Audio] Shutdown START.", 0);

	res = (*GetJavaVM())->GetEnv(GetJavaVM(), (void**)&env, JNI_VERSION_1_2);
	if ( res == JNI_EDETACHED ) {
		(*GetJavaVM())->AttachCurrentThread(GetJavaVM(), &env, NULL);
	}

	(*env)->CallNonvirtualVoidMethod(env, mtrack, cAudioTrack, mStop);
	(*env)->CallNonvirtualVoidMethod(env, mtrack, cAudioTrack, mRelease);

	(*env)->PopLocalFrame(env, NULL);

	(*env)->DeleteGlobalRef(env, ctx->buff);
	(*env)->DeleteGlobalRef(env, mtrack);
	(*env)->DeleteGlobalRef(env, cAudioTrack);

	//if ( res == JNI_EDETACHED ) {
	(*GetJavaVM())->DetachCurrentThread(GetJavaVM());
	//}

	LOGV("[Android Audio] Shutdown DONE.", 0);
}


/*we assume what was asked is what we got*/
/* Called by the audio thread */
static GF_Err WAV_ConfigureOutput(GF_AudioOutput *dr, u32 *SampleRate, u32 *NbChannels, u32 *nbBitsPerSample, u32 channel_cfg)
{
	JNIEnv* env = NULL;
	u32 i;
	DroidContext *ctx = (DroidContext *)dr->opaque;

	LOGI("[Android Audio] Configure Output for %u channels...", *NbChannels);

	if (!ctx) return GF_BAD_PARAM;

	ctx->sampleRateInHz = *SampleRate;
	ctx->channelConfig = (*NbChannels == 1) ? CHANNEL_CONFIGURATION_MONO : CHANNEL_CONFIGURATION_STEREO; //AudioFormat.CHANNEL_CONFIGURATION_MONO
	ctx->audioFormat = (*nbBitsPerSample == 8)? ENCODING_PCM_8BIT : ENCODING_PCM_16BIT; //AudioFormat.ENCODING_PCM_16BIT

	// Get the java environment in the new thread
	(*GetJavaVM())->AttachCurrentThread(GetJavaVM(), &env, NULL);
	ctx->env = env;
	LOGV("[Android Audio] SampleRate : %d",ctx->sampleRateInHz);
	LOGV("[Android Audio] BitPerSample : %d", *nbBitsPerSample);

	(*env)->PushLocalFrame(env, 2);

	ctx->num_buffers = 1;
	ctx->mbufferSizeInBytes = (*env)->CallStaticIntMethod(env, cAudioTrack, mGetMinBufferSize,
	                          ctx->sampleRateInHz, ctx->channelConfig, ctx->audioFormat);

	//ctx->mbufferSizeInBytes *= 3;

	LOGV("[Android Audio] Buffer Size : %d", ctx->mbufferSizeInBytes);

	i = 1;
	if ( ctx->channelConfig == CHANNEL_CONFIGURATION_STEREO )
		i *= 2;
	if ( ctx->audioFormat == ENCODING_PCM_16BIT )
		i *= 2;

	ctx->total_length_ms =  1000 * ctx->num_buffers * ctx->mbufferSizeInBytes / i / ctx->sampleRateInHz;

	LOGV("[Android Audio] Buffer Length ms : %d", ctx->total_length_ms);

	/*initial delay is full buffer size*/
	ctx->delay = ctx->total_length_ms;

	mtrack = (*env)->NewObject(env, cAudioTrack, mAudioTrack, STREAM_MUSIC, ctx->sampleRateInHz,
	                           ctx->channelConfig, ctx->audioFormat, ctx->mbufferSizeInBytes, MODE_STREAM); //AudioTrack.MODE_STREAM
	if (mtrack) {
		mtrack = (*env)->NewGlobalRef(env, mtrack);
		ctx->mtrack = mtrack;
		(*env)->CallNonvirtualVoidMethod(env, mtrack, cAudioTrack, mPlay);
//	  (*env)->CallNonvirtualVoidMethod(env, mtrack, cAudioTrack, mStop);
	}  else {
		LOGV("[Android Audio] mtrack = %p", mtrack);
		return GF_NOT_SUPPORTED;
	}

	if ( ctx->audioFormat == ENCODING_PCM_8BIT )
		ctx->buff = (*env)->NewByteArray(env, ctx->mbufferSizeInBytes);
	else
		ctx->buff = (*env)->NewShortArray(env, ctx->mbufferSizeInBytes/2);
	if ( ctx->buff ) {
		ctx->buff = (*env)->NewGlobalRef(env, ctx->buff);
	} else {
		LOGV("[Android Audio] ctx->buff = %p", ctx->buff );
		return GF_NOT_SUPPORTED;
	}

	LOGV("[Android Audio] ConfigureOutput DONE.", *nbBitsPerSample);
	return GF_OK;
}

/* Called by the audio thread */
static void WAV_WriteAudio(GF_AudioOutput *dr)
{
	DroidContext *ctx = (DroidContext *)dr->opaque;
	if (!ctx)
		return;
	JNIEnv* env = ctx->env;
	u32 written;
	void* pBuffer;
	if (!env)
		return;
#ifdef DROID_EXTREME_LOGS
	LOGV("[Android Audio] WAV_WriteAudio() : entering",ctx->sampleRateInHz);
#endif /* DROID_EXTREME_LOGS */
	pBuffer = (*env)->GetPrimitiveArrayCritical(env, ctx->buff, NULL);
	if (pBuffer)
	{
		written = dr->FillBuffer(dr->audio_renderer, pBuffer, ctx->mbufferSizeInBytes);
		(*env)->ReleasePrimitiveArrayCritical(env, ctx->buff, pBuffer, 0);
		if (written)
		{
			if ( ctx->audioFormat == ENCODING_PCM_8BIT )
				(*env)->CallNonvirtualIntMethod(env, mtrack, cAudioTrack, mWriteB, ctx->buff, 0, ctx->mbufferSizeInBytes);
			else
				(*env)->CallNonvirtualIntMethod(env, mtrack, cAudioTrack, mWriteS, ctx->buff, 0, ctx->mbufferSizeInBytes/2);
		}
	}
	else
	{
		LOGV("[Android Audio] Failed to get pointer to array bytes = %p", pBuffer);
	}
#ifdef DROID_EXTREME_LOGS
	LOGV("[Android Audio] WAV_WriteAudio() : done",ctx->sampleRateInHz);
#endif /* DROID_EXTREME_LOGS */
}

/* Called by the main thread */
static void WAV_Play(GF_AudioOutput *dr, u32 PlayType)
{
	DroidContext *ctx = (DroidContext *)dr->opaque;
	JNIEnv* env = GetEnv();

	LOGV("[Android Audio] Play: %d\n", PlayType);

	switch ( PlayType )
	{
	case 0:
		// Stop playing
		(*env)->CallNonvirtualVoidMethod(env, mtrack, cAudioTrack, mStop);
		// Clear the internal buffers
		(*env)->CallNonvirtualVoidMethod(env, mtrack, cAudioTrack, mFlush);
		break;
	case 2:
		(*env)->CallNonvirtualVoidMethod(env, mtrack, cAudioTrack, mFlush);
	case 1:
		(*env)->CallNonvirtualVoidMethod(env, mtrack, cAudioTrack, mPlay);
		break;
	default:
		LOGW("[Android Audio] Unknown Play method=%d.\n", PlayType);
	}
	LOGV("[Android Audio] Play DONE (%d).\n", PlayType);
}

static void WAV_UpdateVolume(DroidContext *ctx) {
	float lV, rV;
	JNIEnv* env = GetEnv();
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
	if (env && setStereoVolume && mtrack && cAudioTrack) {
		int success;
		if (0!= (success=((*env)->CallNonvirtualIntMethod(env, mtrack, cAudioTrack, setStereoVolume, lV, rV))))
			LOGE3("SetVolume(%f,%f) returned Error code %d", lV, rV, success );
	} else {
		LOGD2("SetVolume(%f,%f)", lV, rV );
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
	JNIEnv* env = ctx->env;
	u32 sampleRateInHz, channelConfig, audioFormat;

	LOGV("Query sample=%d", *desired_samplerate );

#ifdef TEST_QUERY_SAMPLE
	sampleRateInHz = *desired_samplerate;
	channelConfig = (*NbChannels == 1) ? CHANNEL_CONFIGURATION_MONO : CHANNEL_CONFIGURATION_STEREO;
	audioFormat = (*nbBitsPerSample == 8)? ENCODING_PCM_8BIT : ENCODING_PCM_16BIT;

	LOGV3("[Android Audio] Query: SampleRate ChannelConfig AudioFormat: %d %d %d \n",
	      sampleRateInHz,
	      (channelConfig == CHANNEL_CONFIGURATION_MONO)? 1 : 2,
	      (ctx->audioFormat == ENCODING_PCM_8BIT)? 8 : 16);

	switch (*desired_samplerate) {
	case 11025:
		*desired_samplerate = 11025;
		if ( (*env)->CallStaticIntMethod(env, cAudioTrack, mGetMinBufferSize,
		                                 *desired_samplerate, channelConfig, audioFormat) > 0 )
			return GF_OK;
	case 22050:
		*desired_samplerate = 22050;
		if ( (*env)->CallStaticIntMethod(env, cAudioTrack, mGetMinBufferSize,
		                                 *desired_samplerate, channelConfig, audioFormat) > 0 )
			return GF_OK;
		break;
	case 8000:
		*desired_samplerate = 8000;
		if ( (*env)->CallStaticIntMethod(env, cAudioTrack, mGetMinBufferSize,
		                                 *desired_samplerate, channelConfig, audioFormat) > 0 )
			return GF_OK;
	case 16000:
		*desired_samplerate = 16000;
		if ( (*env)->CallStaticIntMethod(env, cAudioTrack, mGetMinBufferSize,
		                                 *desired_samplerate, channelConfig, audioFormat) > 0 )
			return GF_OK;
	case 32000:
		*desired_samplerate = 32000;
		if ( (*env)->CallStaticIntMethod(env, cAudioTrack, mGetMinBufferSize,
		                                 *desired_samplerate, channelConfig, audioFormat) > 0 )
			return GF_OK;
		break;
	case 24000:
		*desired_samplerate = 24000;
		if ( (*env)->CallStaticIntMethod(env, cAudioTrack, mGetMinBufferSize,
		                                 *desired_samplerate, channelConfig, audioFormat) > 0 )
			return GF_OK;
	case 48000:
		*desired_samplerate = 48000;
		if ( (*env)->CallStaticIntMethod(env, cAudioTrack, mGetMinBufferSize,
		                                 *desired_samplerate, channelConfig, audioFormat) > 0 )
			return GF_OK;
		break;
	case 44100:
		*desired_samplerate = 44100;
		if ( (*env)->CallStaticIntMethod(env, cAudioTrack, mGetMinBufferSize,
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

GPAC_MODULE_STATIC_DELARATION( droid_audio )
