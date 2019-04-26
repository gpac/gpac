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
	u32 direct_output_mode;

	u32 stride;

	u32 output_cb_size;
	/*for audio packed frames*/
	u32 frame_start;
	char audio_buf[192000];
	Bool check_h264_isma;

	Bool frame_size_changed;

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
	void *options;
#else
	AVIOContext io;
	AVDictionary *options;
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


/*TODO - we need to cleanup the ffmpeg code to align with only latest version and remove old compatibility code*/

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(54, 25, 0 )

#define CODEC_ID_SVQ3	AV_CODEC_ID_SVQ3
#define CODEC_ID_MPEG4	AV_CODEC_ID_MPEG4
#define CODEC_ID_H264	AV_CODEC_ID_H264
#define CODEC_ID_MPEG2VIDEO AV_CODEC_ID_MPEG2VIDEO
#define CODEC_ID_MJPEG	AV_CODEC_ID_MJPEG
#define CODEC_ID_MP2	AV_CODEC_ID_MP2
#define CODEC_ID_AC3	AV_CODEC_ID_AC3
#define CODEC_ID_EAC3	AV_CODEC_ID_EAC3
#define CODEC_ID_DVD_SUBTITLE	AV_CODEC_ID_DVD_SUBTITLE
#define CODEC_ID_RAWVIDEO	AV_CODEC_ID_RAWVIDEO
#define CODEC_ID_MJPEGB	AV_CODEC_ID_MJPEGB
#define CODEC_ID_LJPEG	AV_CODEC_ID_LJPEG
#define CODEC_ID_GIF	AV_CODEC_ID_GIF
#define CODEC_ID_H263	AV_CODEC_ID_H263
#define CODEC_ID_MP3	AV_CODEC_ID_MP3
#define CODEC_ID_AAC	AV_CODEC_ID_AAC
#define CODEC_ID_MPEG1VIDEO	AV_CODEC_ID_MPEG1VIDEO
#define CODEC_ID_PNG	AV_CODEC_ID_PNG
#define CODEC_ID_AMR_NB	AV_CODEC_ID_AMR_NB
#define CODEC_ID_AMR_WB	AV_CODEC_ID_AMR_WB

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(54, 92, 100 )
#define CODEC_ID_VP9	AV_CODEC_ID_VP9
#endif

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 1, 100 )
#define CODEC_ID_OPUS	AV_CODEC_ID_OPUS
#endif

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(58, 0, 0 )
#define CODEC_ID_AV1	AV_CODEC_ID_AV1
#endif

#endif

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(51, 42, 0)
#define PIX_FMT_YUV420P AV_PIX_FMT_YUV420P
#define PIX_FMT_YUV422P AV_PIX_FMT_YUV422P
#define PIX_FMT_YUV444P AV_PIX_FMT_YUV444P
#define PIX_FMT_YUV420P10LE AV_PIX_FMT_YUV420P10LE
#define PIX_FMT_YUV422P10LE AV_PIX_FMT_YUV422P10LE
#define PIX_FMT_YUV444P10LE AV_PIX_FMT_YUV444P10LE
#define PIX_FMT_BGR24 AV_PIX_FMT_BGR24
#define PIX_FMT_RGB24 AV_PIX_FMT_RGB24
#define PIX_FMT_RGBA AV_PIX_FMT_RGBA
#endif

#if (LIBAVCODEC_VERSION_MAJOR>56)
#ifndef FF_API_AVFRAME_LAVC
#define FF_API_AVFRAME_LAVC
#endif
#endif



#endif

