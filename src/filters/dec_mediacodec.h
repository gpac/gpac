/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2020
 *					All rights reserved
 *
 *  This file is part of GPAC / mediacodec decoder filter
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

#ifndef _MEDIACODEC_DEC_H_
#define _MEDIACODEC_DEC_H_

#include <gpac/filters.h>

#ifndef GPAC_CONFIG_ANDROID
#define MEDIACODEC_EMUL_API
#endif

#ifdef MEDIACODEC_EMUL_API

typedef void AMediaCodec;
typedef void AMediaFormat;
typedef void ANativeWindow;
typedef int window;
typedef void *jobject;

typedef struct {
	u32 flags;
	s32 offset;
	s64 presentationTimeUs;
	s32 size;
} AMediaCodecBufferInfo;

void AMediaFormat_setString(void *, const char *, const char *);
void AMediaFormat_setInt32(void *, const char *, int);
void AMediaFormat_setBuffer(void *, const char *, char *, int);
AMediaFormat *AMediaFormat_new();
AMediaCodec *AMediaCodec_createCodecByName(char *);
int AMediaCodec_configure(AMediaCodec *, const AMediaFormat *format, ANativeWindow *surface, void *crypto, uint32_t flags);
int AMediaCodec_start(AMediaCodec *);
int AMediaCodec_stop(AMediaCodec *);

ssize_t AMediaCodec_dequeueInputBuffer(AMediaCodec *, int64_t timeoutUs);
int AMediaCodec_flush(AMediaCodec *);
char *AMediaCodec_getInputBuffer(AMediaCodec *, size_t idx, size_t *out_size);
int AMediaCodec_queueInputBuffer(AMediaCodec *, size_t idx, int offset, size_t size, uint64_t time, uint32_t flags);
int AMediaCodec_dequeueOutputBuffer(AMediaCodec *, AMediaCodecBufferInfo *info, int64_t timeoutUs);
char *AMediaCodec_getOutputBuffer(AMediaCodec *, size_t idx, size_t *out_size);
int AMediaFormat_getInt32(AMediaFormat *, const char *name, int32_t *out);
int AMediaCodec_releaseOutputBuffer(AMediaCodec*, size_t idx, int render);
int AMediaFormat_delete(AMediaFormat *);
AMediaFormat *AMediaCodec_getOutputFormat(AMediaCodec *);
int AMediaCodec_delete(AMediaCodec *);
void ANativeWindow_release(ANativeWindow *);

#define AMEDIA_OK	0
#define AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED	1
#define AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED	2
#define AMEDIACODEC_INFO_TRY_AGAIN_LATER	3
#define AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM	1

const char* AMEDIAFORMAT_KEY_AAC_PROFILE = "aac-profile";
const char* AMEDIAFORMAT_KEY_BIT_RATE = "bitrate";
const char* AMEDIAFORMAT_KEY_CHANNEL_COUNT = "channel-count";
const char* AMEDIAFORMAT_KEY_CHANNEL_MASK = "channel-mask";
const char* AMEDIAFORMAT_KEY_COLOR_FORMAT = "color-format";
const char* AMEDIAFORMAT_KEY_DURATION = "durationUs";
const char* AMEDIAFORMAT_KEY_FLAC_COMPRESSION_LEVEL = "flac-compression-level";
const char* AMEDIAFORMAT_KEY_FRAME_RATE = "frame-rate";
const char* AMEDIAFORMAT_KEY_HEIGHT = "height";
const char* AMEDIAFORMAT_KEY_IS_ADTS = "is-adts";
const char* AMEDIAFORMAT_KEY_IS_AUTOSELECT = "is-autoselect";
const char* AMEDIAFORMAT_KEY_IS_DEFAULT = "is-default";
const char* AMEDIAFORMAT_KEY_IS_FORCED_SUBTITLE = "is-forced-subtitle";
const char* AMEDIAFORMAT_KEY_I_FRAME_INTERVAL = "i-frame-interval";
const char* AMEDIAFORMAT_KEY_LANGUAGE = "language";
const char* AMEDIAFORMAT_KEY_MAX_HEIGHT = "max-height";
const char* AMEDIAFORMAT_KEY_MAX_INPUT_SIZE = "max-input-size";
const char* AMEDIAFORMAT_KEY_MAX_WIDTH = "max-width";
const char* AMEDIAFORMAT_KEY_MIME = "mime";
const char* AMEDIAFORMAT_KEY_PUSH_BLANK_BUFFERS_ON_STOP = "push-blank-buffers-on-shutdown";
const char* AMEDIAFORMAT_KEY_REPEAT_PREVIOUS_FRAME_AFTER = "repeat-previous-frame-after";
const char* AMEDIAFORMAT_KEY_SAMPLE_RATE = "sample-rate";
const char* AMEDIAFORMAT_KEY_WIDTH = "width";
const char* AMEDIAFORMAT_KEY_STRIDE = "stride";

#else

#include <media/NdkMediaCodec.h>
#include <media/NdkMediaExtractor.h>
#include <media/NdkMediaFormat.h>
#include <jni.h>

void ANativeWindow_release(ANativeWindow *);

#endif

#include "../../src/compositor/gl_inc.h"

typedef struct {
	jobject oSurfaceTex;
	int texture_id;
} GF_MCDecSurfaceTexture;
	
GF_Err mcdec_create_surface(GLuint tex_id, ANativeWindow ** window, Bool * surface_rendering, GF_MCDecSurfaceTexture * surfaceTex);
GF_Err mcdec_delete_surface(GF_MCDecSurfaceTexture surfaceTex);
char * mcdec_find_decoder(const char * mime, u32 width, u32 height,  Bool * is_adaptive);
u32 mcdec_exit_callback(void * param);

GF_Err mcdec_update_surface(GF_MCDecSurfaceTexture surfaceTex);
GF_Err mcdec_get_transform_matrix(struct __matrix * mx, GF_MCDecSurfaceTexture surfaceTex);

#endif //_MEDIACODEC_DEC_H_

