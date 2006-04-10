/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
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

#if defined(WIN32) && !defined(__MINGW32__)

#define EMULATE_INTTYPES
#define EMULATE_FAST_INT
#ifndef inline
#define inline __inline
#endif

#ifndef __MINGW32__
#define __attribute__(s)
#endif

#endif


/*include FFMPEG APIs*/
#include <ffmpeg/avformat.h>

/*FFMPEG decoder module */
typedef struct 
{
	u32 ES_ID;
	u32 out_size;
	u32 oti, st;
	u32 previous_par;
	Bool no_par_update;

	Bool check_short_header;
	AVCodecContext *ctx;
    AVCodec *codec;
	AVFrame *frame;
	u32 pix_fmt;

	/*for audio packed frames*/
	u32 frame_start;
	char audio_buf[AVCODEC_MAX_AUDIO_FRAME_SIZE];
} FFDec;

void *FFDEC_Load();
void FFDEC_Delete(void *ifce);


/*
		reader interface

*/

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
} FFDemux;

void *New_FFMPEG_Demux();
void Delete_FFMPEG_Demux(void *ifce);

/*this is the OTI (user-priv) used for all undefined codec used by ffmpeg (carrying specific info from
AVContext*/
#define GPAC_FFMPEG_CODECS_OTI			0x81

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

