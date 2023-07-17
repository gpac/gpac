/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Ivica Arsov, Jean Le Feuvre
 *			Copyright (c) Mines-Telecom 2009-2022
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

typedef struct
{
	JNIEnv* env;

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

	//JNI vars - do NOT use static vars since we may load/unload the module at run time
	jclass class_AudioTrack;
	jobject obj_AudioTrack;
	jmethodID fun_AudioTrack;
	jmethodID fun_setStereoVolume;
	jmethodID fun_GetMinBufferSize;
	jmethodID fun_Play;
	jmethodID fun_Stop;
	jmethodID fun_Release;
	jmethodID fun_WriteB;
	jmethodID fun_WriteS;
	jmethodID fun_Flush;
} DroidContext;


// Called by the main thread
static GF_Err AAOut_Setup(GF_AudioOutput *dr, void *os_handle, u32 num_buffers, u32 total_duration)
{
	DroidContext *ctx = (DroidContext *)dr->opaque;
	JNIEnv* env = GetEnv();

	GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[AndroidAudio] Setup for %d buffers", num_buffers ));

	ctx->force_config = (num_buffers && total_duration) ? 1 : 0;
	ctx->cfg_num_buffers = num_buffers;
	if (ctx->cfg_num_buffers <= 1) ctx->cfg_num_buffers = 2;
	ctx->cfg_duration = total_duration;
	if (!ctx->force_config) ctx->num_buffers = 1;
	ctx->volume = 100;
	ctx->pan = 50;

	if (ctx->class_AudioTrack) return GF_OK;

	ctx->class_AudioTrack = (*env)->FindClass(env, "android/media/AudioTrack");
	if (!ctx->class_AudioTrack) {
		return GF_NOT_SUPPORTED;
	}

	ctx->class_AudioTrack = (*env)->NewGlobalRef(env, ctx->class_AudioTrack);

	ctx->fun_AudioTrack = (*env)->GetMethodID(env, ctx->class_AudioTrack, "<init>", "(IIIIII)V");
	ctx->fun_GetMinBufferSize = (*env)->GetStaticMethodID(env, ctx->class_AudioTrack, "getMinBufferSize", "(III)I");
	ctx->fun_Play = (*env)->GetMethodID(env, ctx->class_AudioTrack, "play", "()V");
	ctx->fun_Stop = (*env)->GetMethodID(env, ctx->class_AudioTrack, "stop", "()V");
	ctx->fun_Release = (*env)->GetMethodID(env, ctx->class_AudioTrack, "release", "()V");
	ctx->fun_WriteB = (*env)->GetMethodID(env, ctx->class_AudioTrack, "write", "([BII)I");
	ctx->fun_WriteS = (*env)->GetMethodID(env, ctx->class_AudioTrack, "write", "([SII)I");
	ctx->fun_Flush = (*env)->GetMethodID(env, ctx->class_AudioTrack, "flush", "()V");
	ctx->fun_setStereoVolume = (*env)->GetMethodID(env, ctx->class_AudioTrack, "setStereoVolume", "(FF)I");
	return GF_OK;
}

// Called by the audio thread
static void AAOut_Shutdown(GF_AudioOutput *dr)
{
	DroidContext *ctx = (DroidContext *)dr->opaque;
	JNIEnv* env = NULL;
	jint res = 0;

	GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[AndroidAudio] Shutdown" ));

	res = (*GetJavaVM())->GetEnv(GetJavaVM(), (void**)&env, JNI_VERSION_1_2);
	if ( res == JNI_EDETACHED ) {
		(*GetJavaVM())->AttachCurrentThread(GetJavaVM(), &env, NULL);
	}

	(*env)->CallNonvirtualVoidMethod(env, ctx->obj_AudioTrack, ctx->class_AudioTrack, ctx->fun_Stop);
	(*env)->CallNonvirtualVoidMethod(env, ctx->obj_AudioTrack, ctx->class_AudioTrack, ctx->fun_Release);

	(*env)->PopLocalFrame(env, NULL);

	(*env)->DeleteGlobalRef(env, ctx->buff);
	(*env)->DeleteGlobalRef(env, ctx->obj_AudioTrack);
	(*env)->DeleteGlobalRef(env, ctx->class_AudioTrack);

	//if ( res == JNI_EDETACHED ) {
	(*GetJavaVM())->DetachCurrentThread(GetJavaVM());
	//}
}


/* Called by the audio thread */
static GF_Err AAOut_ConfigureOutput(GF_AudioOutput *dr, u32 *SampleRate, u32 *NbChannels, u32 *nbBitsPerSample, u64 channel_cfg)
{
	JNIEnv* env = NULL;
	u32 bytes_per_samp;
	DroidContext *ctx = (DroidContext *)dr->opaque;

	if (!ctx) return GF_BAD_PARAM;

	ctx->sampleRateInHz = *SampleRate;
	ctx->channelConfig = (*NbChannels == 1) ? CHANNEL_CONFIGURATION_MONO : CHANNEL_CONFIGURATION_STEREO;
	ctx->audioFormat = (*nbBitsPerSample == 8) ? ENCODING_PCM_8BIT : ENCODING_PCM_16BIT;

	// Get the java environment in the new thread
	(*GetJavaVM())->AttachCurrentThread(GetJavaVM(), &env, NULL);
	ctx->env = env;

	(*env)->PushLocalFrame(env, 2);

	ctx->num_buffers = 1;
	ctx->mbufferSizeInBytes = (*env)->CallStaticIntMethod(env, ctx->class_AudioTrack, ctx->fun_GetMinBufferSize,
	                          ctx->sampleRateInHz, ctx->channelConfig, ctx->audioFormat);

	bytes_per_samp = 1;
	if (ctx->channelConfig == CHANNEL_CONFIGURATION_STEREO) bytes_per_samp *= 2;
	if (ctx->audioFormat == ENCODING_PCM_16BIT) bytes_per_samp *= 2;

	ctx->total_length_ms =  1000 * ctx->num_buffers * ctx->mbufferSizeInBytes / bytes_per_samp / ctx->sampleRateInHz;

	GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[AndroidAudio] Configure Output for %u Hz %u channels - Buffer size %u ms", ctx->sampleRateInHz, *NbChannels, ctx->total_length_ms));

	/*initial delay is full buffer size*/
	ctx->delay = ctx->total_length_ms;

	ctx->obj_AudioTrack = (*env)->NewObject(env, ctx->class_AudioTrack, ctx->fun_AudioTrack, STREAM_MUSIC, ctx->sampleRateInHz,
	                           ctx->channelConfig, ctx->audioFormat, ctx->mbufferSizeInBytes, MODE_STREAM); //AudioTrack.MODE_STREAM
	if (ctx->obj_AudioTrack) {
		ctx->obj_AudioTrack = (*env)->NewGlobalRef(env, ctx->obj_AudioTrack);
		(*env)->CallNonvirtualVoidMethod(env, ctx->obj_AudioTrack, ctx->class_AudioTrack, ctx->fun_Play);
	}  else {
		return GF_NOT_SUPPORTED;
	}

	if ( ctx->audioFormat == ENCODING_PCM_8BIT )
		ctx->buff = (*env)->NewByteArray(env, ctx->mbufferSizeInBytes);
	else
		ctx->buff = (*env)->NewShortArray(env, ctx->mbufferSizeInBytes/2);

	if (ctx->buff) {
		ctx->buff = (*env)->NewGlobalRef(env, ctx->buff);
	} else {
		return GF_NOT_SUPPORTED;
	}
	return GF_OK;
}

/* Called by the audio thread */
static void AAOut_WriteAudio(GF_AudioOutput *dr)
{
	u32 written;
	void* pBuffer;
	DroidContext *ctx = (DroidContext *)dr->opaque;
	if (!ctx) return;
	JNIEnv* env = ctx->env;
	if (!env) return;

	if (ctx->audioFormat == ENCODING_PCM_8BIT)
		pBuffer = (*env)->GetByteArrayElements(env, ctx->buff, NULL);
	else
		pBuffer = (*env)->GetShortArrayElements(env, ctx->buff, NULL);

	if (pBuffer) {
		written = dr->FillBuffer(dr->audio_renderer, pBuffer, ctx->mbufferSizeInBytes);

		if (ctx->audioFormat == ENCODING_PCM_8BIT) {
			(*env)->ReleaseByteArrayElements(env, ctx->buff, pBuffer, 0);
			if (written)
				(*env)->CallNonvirtualIntMethod(env, ctx->obj_AudioTrack, ctx->class_AudioTrack, ctx->fun_WriteB, ctx->buff, 0, written);
		} else {
			(*env)->ReleaseShortArrayElements(env, ctx->buff, pBuffer, 0);
			if (written)
				(*env)->CallNonvirtualIntMethod(env, ctx->obj_AudioTrack, ctx->class_AudioTrack, ctx->fun_WriteS, ctx->buff, 0, written/2);
		}
	} else {
		GF_LOG(GF_LOG_WARNING, GF_LOG_MMIO, ("[AndroidAudio] Failed to get pointer to array bytes"));
	}
}

/* Called by the main thread */
static void AAOut_Play(GF_AudioOutput *dr, u32 PlayType)
{
	DroidContext *ctx = (DroidContext *)dr->opaque;
	JNIEnv* env = GetEnv();
	if (!env) return;

	switch (PlayType) {
	case 0:
		GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[AndroidAudio] Pause\n"));
		(*env)->CallNonvirtualVoidMethod(env, ctx->obj_AudioTrack, ctx->class_AudioTrack, ctx->fun_Stop);
		(*env)->CallNonvirtualVoidMethod(env, ctx->obj_AudioTrack, ctx->class_AudioTrack, ctx->fun_Flush);
		break;
	case 2:
		GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[AndroidAudio] Resume\n"));
		(*env)->CallNonvirtualVoidMethod(env, ctx->obj_AudioTrack, ctx->class_AudioTrack, ctx->fun_Flush);
		(*env)->CallNonvirtualVoidMethod(env, ctx->obj_AudioTrack, ctx->class_AudioTrack, ctx->fun_Play);
		break;
	case 1:
		GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[AndroidAudio] Play\n"));
		(*env)->CallNonvirtualVoidMethod(env, ctx->obj_AudioTrack, ctx->class_AudioTrack, ctx->fun_Play);
		break;
	default:
		GF_LOG(GF_LOG_WARNING, GF_LOG_MMIO, ("[AndroidAudio] Unknown play method %d\n", PlayType));
		break;
	}
}

static void AAOut_UpdateVolume(DroidContext *ctx)
{
	float lV, rV;
	JNIEnv* env = GetEnv();
	if (!env || !ctx->fun_setStereoVolume || !ctx->obj_AudioTrack || !ctx->class_AudioTrack) return;

	if (ctx->pan > 100) ctx->pan = 100;

	lV = rV = ctx->volume / 100.0;
	if (ctx->pan > 50) {
		float m = (100 - ctx->pan) / 50.0;
		lV*=m;
	} else if (ctx->pan < 50) {
		float m = ctx->pan / 50.0;
		rV*=m;
	}

	int success = (*env)->CallNonvirtualIntMethod(env, ctx->obj_AudioTrack, ctx->class_AudioTrack, ctx->fun_setStereoVolume, lV, rV);
	if (success != 0) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_MMIO, ("[AndroidAudio] SetVolume(%f,%f) failed with error %d", lV, rV, success));
	}
}

static void AAOut_SetVolume(GF_AudioOutput *dr, u32 Volume)
{
	DroidContext *ctx = (DroidContext *)dr->opaque;
	ctx->volume = Volume;
	AAOut_UpdateVolume(ctx);
}

static void AAOut_SetPan(GF_AudioOutput *dr, u32 Pan)
{
	DroidContext *ctx = (DroidContext *)dr->opaque;
	AAOut_UpdateVolume(ctx);
}

/* Called by the audio thread */
static GF_Err AAOut_QueryOutputSampleRate(GF_AudioOutput *dr, u32 *desired_samplerate, u32 *NbChannels, u32 *nbBitsPerSample)
{
#ifdef TEST_QUERY_SAMPLE
	DroidContext *ctx = (DroidContext *)dr->opaque;
	JNIEnv* env = ctx->env;

	u32 sampleRateInHz = *desired_samplerate;
	u32 channelConfig = (*NbChannels == 1) ? CHANNEL_CONFIGURATION_MONO : CHANNEL_CONFIGURATION_STEREO;
	u32 audioFormat = (*nbBitsPerSample == 8)? ENCODING_PCM_8BIT : ENCODING_PCM_16BIT;

	GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[AndroidAudio] Query config: SampleRate ChannelConfig AudioFormat: %u %u %u\n",
	      sampleRateInHz,
	      (channelConfig == CHANNEL_CONFIGURATION_MONO)? 1 : 2,
	      (ctx->audioFormat == ENCODING_PCM_8BIT)? 8 : 16));

	switch (*desired_samplerate) {
	case 11025:
	case 22050:
	case 8000:
	case 16000:
	case 32000:
	case 24000:
	case 48000:
	case 44100:
		if ( (*env)->CallStaticIntMethod(env, ctx->class_AudioTrack, ctx->fun_GetMinBufferSize, *desired_samplerate, channelConfig, audioFormat) > 0 )
			return GF_OK;
		break;
	default:
		break;
	}
#endif

	return GF_OK;
}

static u32 AAOut_GetAudioDelay(GF_AudioOutput *dr)
{
	DroidContext *ctx = (DroidContext *)dr->opaque;

	return ctx->delay;
}

static u32 AAOut_GetTotalBufferTime(GF_AudioOutput *dr)
{
	DroidContext *ctx = (DroidContext *)dr->opaque;

	return ctx->total_length_ms;
}


void *NewAAOutRender()
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
	GF_REGISTER_MODULE_INTERFACE(driv, GF_AUDIO_OUTPUT_INTERFACE, "android", "gpac distribution")

	driv->opaque = ctx;

	driv->SelfThreaded = 0;
	driv->Setup = AAOut_Setup;
	driv->Shutdown = AAOut_Shutdown;
	driv->Configure = AAOut_ConfigureOutput;
	driv->GetAudioDelay = AAOut_GetAudioDelay;
	driv->GetTotalBufferTime = AAOut_GetTotalBufferTime;
	driv->SetVolume = AAOut_SetVolume;
	driv->SetPan = AAOut_SetPan;
	driv->Play = AAOut_Play;
	driv->QueryOutputSampleRate = AAOut_QueryOutputSampleRate;
	driv->WriteAudio = AAOut_WriteAudio;
	return driv;
}

void DeleteAAOutRender(void *ifce)
{
	GF_AudioOutput *dr = (GF_AudioOutput *) ifce;
	if (!ifce)
		return;
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
	if (InterfaceType == GF_AUDIO_OUTPUT_INTERFACE) return NewAAOutRender();
	return NULL;
}

GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_AUDIO_OUTPUT_INTERFACE:
		DeleteAAOutRender((GF_AudioOutput *) ifce);
		break;
	}
}

GPAC_MODULE_STATIC_DECLARATION( droid_audio )
