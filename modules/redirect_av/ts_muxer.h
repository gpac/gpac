/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean le Feuvre
 *			Copyright (c) 2010-20XX Telecom ParisTech
 *			All rights reserved
 *
 *  This file is part of GPAC / AVI Recorder demo module
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
#ifndef _AVR_TS_MUXER_H_
#define _AVR_TS_MUXER_H_

#include <gpac/modules/term_ext.h>
#include <gpac/internal/terminal_dev.h>
#include <gpac/internal/compositor_dev.h>
#include <gpac/network.h>
#include <gpac/tools.h>
#include <libavcodec/avcodec.h>

#if ((LIBAVCODEC_VERSION_MAJOR == 52) && (LIBAVCODEC_VERSION_MINOR <= 20)) || (LIBAVCODEC_VERSION_MAJOR < 52)
#undef USE_AVCODEC2
#else
#define USE_AVCODEC2	1
#endif

#define TS_MUX_MODE_PUT

#undef AVR_DUMP_RAW_AVI

#ifdef AVR_DUMP_RAW_AVI
/*sample raw AVI writing*/
#include <gpac/internal/avilib.h>
#endif /* AVR_DUMP_RAW_AVI */

typedef struct avr_ts_muxer GF_AbstractTSMuxer;

#if LIBAVUTIL_VERSION_MAJOR<51
#define AVMEDIA_TYPE_AUDIO CODEC_TYPE_AUDIO
#define AVMEDIA_TYPE_VIDEO CODEC_TYPE_VIDEO
#define AV_PKT_FLAG_KEY	PKT_FLAG_KEY
#endif


#include <gpac/ringbuffer.h>

typedef struct
{
	GF_Terminal *term;

	Bool is_open;
	GF_AudioListener audio_listen;
	GF_VideoListener video_listen;
	GF_TermEventFilter term_listen;
#ifdef AVR_DUMP_RAW_AVI
	avi_t *avi_out;
#endif
	char *frame;
	u32 size;
	GF_AbstractTSMuxer * ts_implementation;
	Bool encode;
	AVCodec *audioCodec;
	AVCodec *videoCodec;
	AVFrame *YUVpicture, *RGBpicture;
	struct SwsContext * swsContext;
	uint8_t * yuv_data;
	uint8_t * videoOutbuf;
	u32 videoOutbufSize;
	GF_Ringbuffer * pcmAudio;
	u32 audioCurrentTime;
	GF_Thread * encodingThread;
	GF_Thread * audioEncodingThread;
	GF_Mutex * frameMutex;
	GF_Mutex * encodingMutex;
	volatile Bool is_running;
	u64 frameTime;
	u64 frameTimeEncoded;
	/**
	 * Audio parameters for encoding
	 */
	u32 audioSampleRate;
	u16 audioChannels;
	/**
	 * Video parameters for encoding
	 */
	u32 srcWidth;
	u32 srcHeight;
	const char * destination;
	GF_GlobalLock * globalLock;
	Bool started;
} GF_AVRedirect;

GF_AbstractTSMuxer * ts_amux_new(GF_AVRedirect * avr, u32 videoBitrateInBitsPerSec, u32 width, u32 height, u32 audioBitRateInBitsPerSec);

void ts_amux_del(GF_AbstractTSMuxer * muxerToDelete);

Bool ts_encode_audio_frame(GF_AbstractTSMuxer * ts, uint8_t * data, int encoded, u64 pts);

Bool ts_encode_video_frame(GF_AbstractTSMuxer* ts, uint8_t* data, int encoded);

AVCodecContext * ts_get_video_codec_context(GF_AbstractTSMuxer * ts);

AVCodecContext * ts_get_audio_codec_context(GF_AbstractTSMuxer * ts);

#endif /* _AVR_TS_MUXER_H_ */
