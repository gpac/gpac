/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / MP4 reader module
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


#ifndef __FFMPEG_IN_H
#define __FFMPEG_IN_H



/*include net API*/
#include <gpac/modules/service.h>
/*include decoder API*/
#include <gpac/modules/codec.h>
#include <gpac/constants.h>
#include <gpac/thread.h>


//#define DISABLE_FFMPEG_DEMUX

#if defined(WIN32) && !defined(__MINGW32__)

#define EMULATE_INTTYPES
#define EMULATE_FAST_INT
#ifndef inline
#define inline __inline
#endif

#if defined(__SYMBIAN32__)
#define EMULATE_INTTYPES
#endif


#ifndef __MINGW32__
#define __attribute__(s)
#endif

#endif


/*include FFMPEG APIs*/
#ifdef _WIN32_WCE
#define inline	__inline
#endif


#if defined(WIN32)
#  define INT64_C(x)  (x ## i64)
#  define UINT64_C(x)  (x ## Ui64)
#endif


#ifdef FFMPEG_OLD_HEADERS
#include <ffmpeg/avformat.h>
#else
#include <libavformat/avformat.h>
#endif

void gf_av_vlog(void* avcl, int level, const char *fmt, va_list vl);


#if LIBAVCODEC_VERSION_INT > AV_VERSION_INT(52, 0, 0)
#define FFMPEG_SWSCALE
#ifdef FFMPEG_OLD_HEADERS
#include <ffmpeg/swscale.h>
#else
#include <libswscale/swscale.h>
#endif
#endif



#if LIBAVUTIL_VERSION_MAJOR<51
#define AVMEDIA_TYPE_AUDIO CODEC_TYPE_AUDIO
#define AVMEDIA_TYPE_VIDEO CODEC_TYPE_VIDEO
#endif


#ifndef FFMPEG_OLD_HEADERS

#if ((LIBAVCODEC_VERSION_MAJOR == 52) && (LIBAVCODEC_VERSION_MINOR <= 20)) || (LIBAVCODEC_VERSION_MAJOR < 52)
#undef USE_AVCODEC2
#else
#define USE_AVCODEC2	1
#endif

#else
#undef USE_AVCODEC2
#endif

#if (LIBAVCODEC_VERSION_MAJOR >= 55)
#define USE_AVCTX3
#elif (LIBAVCODEC_VERSION_MAJOR >= 54) && (LIBAVCODEC_VERSION_MINOR >= 35)
#define USE_AVCTX3
#endif


/*FFMPEG decoder module */
typedef struct
{
	char szCodec[100];
	u32 out_size;
	u32 oti, st;
	u32 previous_par;
	Bool no_par_update;
	Bool needs_output_resize;
	Bool had_pic;

	Bool check_short_header;
	u32 pix_fmt;
	u32 out_pix_fmt;
	Bool is_image;

	u32 raw_pix_fmt;
	Bool flipped;
	Bool direct_output;
	u32 stride;

	u32 output_cb_size;
	/*for audio packed frames*/
	u32 frame_start;
	char audio_buf[192000];
	Bool check_h264_isma;

	u32 base_ES_ID;
	AVCodecContext *base_ctx;
	AVCodec *base_codec;
	AVFrame *base_frame;
#ifdef FFMPEG_SWSCALE
	struct SwsContext *base_sws;
#endif

	u32 depth_ES_ID;
	u32 yuv_size;
	AVCodecContext *depth_ctx;
	AVCodec *depth_codec;
	AVFrame *depth_frame;
#ifdef FFMPEG_SWSCALE
	struct SwsContext *depth_sws;
#endif

#ifdef USE_AVCTX3
	AVFrame *audio_frame;
#endif



	Bool output_as_8bit;
	u32 display_bpp;
	Bool conv_to_8bit;
	char *conv_buffer;
} FFDec;

void *FFDEC_Load();
void FFDEC_Delete(void *ifce);


/*
		reader interface

*/

//#define FFMPEG_IO_BUF_SIZE	16384

//#define FFMPEG_DUMP_REMOTE

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(52, 113, 1)
#define USE_PRE_0_7 1
#endif

typedef struct
{
	/*the service we're responsible for*/
	GF_ClientService *service;

	/*input file*/
	AVFormatContext *ctx;

	Bool seekable;
	Double seek_time;

	s32 audio_st, video_st;
	/*app channels (only deal with 1 audio and one video for now)*/
	LPNETCHANNEL audio_ch;
	LPNETCHANNEL video_ch;
	Bool audio_run, video_run;
	AVRational audio_tscale, video_tscale;
	u32 data_buffer_ms;

	/*demuxer thread - we cannot use direct fetching because of demultiplex structure of libavformat
	(reading one channel may lock the other)*/
	GF_Thread *thread;
	GF_Mutex *mx;
	u32 is_paused, is_running;

	u32 service_type;
	Bool unreliable_audio_timing;

	/*IO wrapper*/
	/*file downloader*/
	GF_DownloadSession *dnload;

#ifdef USE_PRE_0_7
	ByteIOContext   io;
#else
	AVIOContext io;
#endif
	char *buffer;
	u32 buffer_size;

	u32 buffer_used;

#ifdef FFMPEG_DUMP_REMOTE
	FILE *outdbg;
#endif
} FFDemux;

void *New_FFMPEG_Demux();
void Delete_FFMPEG_Demux(void *ifce);


/*The DSI sent is:

	u32 codec_id

- for audio -
	u32 sample_rate: sampling rate or 0 if unknown
	u16 nb_channels: num channels or 0 if unknown
	u16 nb_bits_per_sample: nb bits or 0 if unknown
	u16 num_samples: num audio samples per frame or 0 if unknown
	u16 block_align: audio block align

- for video -
	u32 width: video width or 0 if unknown;
	u32 height: video height or 0 if unknown;

- for both -
	u32 codec_tag: ffmpeg ctx codec tag
	u32 bit_rate: ffmpeg ctx bit rate

- till end of DSI bitstream-
	char *data: extra_data
*/



#endif

