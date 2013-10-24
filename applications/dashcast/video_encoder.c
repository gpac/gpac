/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Arash Shafiei
 *			Copyright (c) Telecom ParisTech 2000-2013
 *					All rights reserved
 *
 *  This file is part of GPAC / dashcast
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

#include "video_encoder.h"

#include "libavutil/opt.h"
#include "libavdevice/avdevice.h"


#if (defined(WIN32) || defined(_WIN32_WCE)) && !defined(__GNUC__)

#define _TOSTR(_val) #_val
#define TOSTR(_val) _TOSTR(_val)

#pragma comment(lib, "avcodec-"TOSTR(LIBAVCODEC_VERSION_MAJOR) )
#pragma comment(lib, "avdevice-"TOSTR(LIBAVDEVICE_VERSION_MAJOR) )
#pragma comment(lib, "avformat-"TOSTR(LIBAVFORMAT_VERSION_MAJOR) )
#pragma comment(lib, "avutil-"TOSTR(LIBAVUTIL_VERSION_MAJOR) )
#pragma comment(lib, "swscale-"TOSTR(LIBSWSCALE_VERSION_MAJOR) )

#endif


//#define DEBUG 1


/**
 * A function which pushes argument to a libav codec using its private data.
 * param priv_data
 * param options a list of space separated and ':' affected options (e.g. "a:b c:d e:f"). @options be non NULL.
 */
void build_dict(void *priv_data, const char *options) {
	char *opt = strdup(options);
	char *tok = strtok(opt, "=");
	char *tokval = NULL;
	while (tok && (tokval=strtok(NULL, " "))) {
		if (av_opt_set(priv_data, tok, tokval, 0) < 0)
			fprintf(stderr, "Unknown custom option \"%s\" with value \"%s\" in %s\n", tok, tokval, options);
		tok = strtok(NULL, "=");
	}
	free(opt);
}

int dc_video_encoder_open(VideoOutputFile *video_output_file, VideoDataConf *video_data_conf, Bool use_source_timing)
{
	video_output_file->vbuf_size = 9 * video_data_conf->width * video_data_conf->height + 10000;
	video_output_file->vbuf = (uint8_t *) av_malloc(video_output_file->vbuf_size);

//	video_output_file->codec = avcodec_find_encoder_by_name("libx264"/*video_data_conf->codec*/);
	video_output_file->codec = avcodec_find_encoder(CODEC_ID_H264);
	if (video_output_file->codec == NULL) {
		fprintf(stderr, "Output video codec %d not found\n", CODEC_ID_H264);
		return -1;
	}

	video_output_file->codec_ctx = avcodec_alloc_context3(video_output_file->codec);

	//Create new video stream
//	video_stream = avformat_new_stream(video_output_file->fmt, video_codec);
//	if (!video_stream) {
//		fprintf(stderr, "Cannot create output video stream\n");
//		return -1;
//	}

	video_output_file->codec_ctx->codec_id = video_output_file->codec->id;
	video_output_file->codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
	video_output_file->codec_ctx->bit_rate = video_data_conf->bitrate;
	video_output_file->codec_ctx->width = video_data_conf->width;
	video_output_file->codec_ctx->height = video_data_conf->height;

	video_output_file->codec_ctx->time_base.num = 1;
	video_output_file->codec_ctx->time_base.den = video_data_conf->framerate;

	video_output_file->use_source_timing = use_source_timing;
	if (use_source_timing) {
		//for avcodec to do rate allcoation, we need to have ctx->timebase == 1/framerate
		video_output_file->codec_ctx->time_base.den = video_data_conf->time_base.den;
		video_output_file->codec_ctx->time_base.num = video_data_conf->time_base.num * video_data_conf->time_base.den / video_data_conf->framerate;
	}
	video_output_file->codec_ctx->pix_fmt = PIX_FMT_YUV420P;
	video_output_file->codec_ctx->gop_size = /*video_output_file->gosize;*/video_data_conf->framerate;

//	video_output_file->codec_ctx->codec_id = video_codec->id;
//	video_output_file->codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
//	video_output_file->codec_ctx->bit_rate = video_data_conf->bitrate;
//	video_output_file->codec_ctx->width = video_data_conf->width;
//	video_output_file->codec_ctx->height = video_data_conf->height;
//	video_output_file->codec_ctx->time_base = (AVRational) {1 ,
//				video_output_file->video_data_conf->framerate};
//	video_output_file->codec_ctx->codec->pix_fmt = PIX_FMT_YUV420P;
	video_output_file->codec_ctx->gop_size = video_data_conf->framerate;
//
//	av_opt_set(video_output_file->codec_ctx->priv_data, "preset", "ultrafast", 0);
//	av_opt_set(video_output_file->codec_ctx->priv_data, "tune", "zerolatency", 0);

	/*
	 video_output_file->codec_ctx->max_b_frames = 0;
	 video_output_file->codec_ctx->thread_count = 1;
	 video_output_file->codec_ctx->delay = 0;
	 video_output_file->codec_ctx->rc_lookahead = 0;
	 */

	/*
	 * video_stream->codec->gosize = video_output_file->vfr;
	 * videoStream->codec->gosize = 1;
	 * video_stream->codec->rc_lookahead = 0;
	 * videoStream->time_base = (AVRational) {1 , 1000000};
	 * videoStream->r_frame_rate = (AVRational) {outVideoCtx->video_framerate, 1};
	 * av_opt_set(videoStream->codec->priv_data, "preset", "slow", 0);
	 * videoStream->codec->me_range = 16;
	 * videoStream->codec->max_qdiff = 4;
	 * videoStream->codec->qmin = 10;
	 * videoStream->codec->qmax = 51;
	 * videoStream->codec->qcompress = 0.6;
	 * videoStream->codec->profile = FF_PROFILE_H264_BASELINE;
	 * videoStream->codec->level = 10;
	 *
	 */

	if (video_data_conf->custom) {
		build_dict(video_output_file->codec_ctx->priv_data, video_data_conf->custom);
		gf_free(video_data_conf->custom);
		video_data_conf->custom = NULL;
	} else {
		fprintf(stdout, "Video Encoder: applying default options (preset=ultrafast tune=zerolatency)\n");
		av_opt_set(video_output_file->codec_ctx->priv_data, "preset", "ultrafast", 0);
		av_opt_set(video_output_file->codec_ctx->priv_data, "tune", "zerolatency", 0);
	}

	if(video_output_file->gdr == 1) {
		av_opt_set_int(video_output_file->codec_ctx->priv_data, "intra-refresh", 1, 0);
		av_opt_set_int(video_output_file->codec_ctx->priv_data, "key-int", 8, 0);
	}

//	if (video_output_file->fmt->oformat->flags & AVFMT_GLOBALHEADER)
	//the global header gives access to the extradata (SPS/PPS)
	video_output_file->codec_ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;

//	if (video_output_file->fmt->oformat->flags & AVFMT_GLOBALHEADER)
//		video_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;

	video_output_file->vstream_idx = 0;//video_stream->index;
	
	/* open the video codec - options are passed thru video_output_file->codec_ctx->priv_data */
	if (avcodec_open2(video_output_file->codec_ctx, video_output_file->codec, NULL) < 0) {
		fprintf(stderr, "Cannot open output video codec\n");
		return -1;
	}

//	/* open the video codec */
//	if (avcodec_open2(video_stream->codec, video_codec, NULL) < 0) {
//		fprintf(stderr, "Cannot open output video codec\n");
//		return -1;
//	}

	return 0;
}

int dc_video_encoder_encode(VideoOutputFile *video_output_file, VideoScaledData *video_scaled_data)
{
	//AVPacket pkt;
	VideoDataNode *video_data_node;
	int ret;
	//int out_size;

//	AVStream *video_stream = video_output_file->fmt->streams[video_output_file->vstream_idx];
//	AVCodecContext *video_codec_ctx = video_stream->codec;
	AVCodecContext *video_codec_ctx = video_output_file->codec_ctx;

	ret = dc_consumer_lock(&video_output_file->consumer, &video_scaled_data->circular_buf);
	if (ret < 0) {
#ifdef DEBUG
		fprintf(stderr, "Video encoder got an end of buffer!\n");
#endif
		return -2;
	}

	dc_consumer_unlock_previous(&video_output_file->consumer, &video_scaled_data->circular_buf);

	video_data_node = (VideoDataNode*)dc_consumer_consume(&video_output_file->consumer, &video_scaled_data->circular_buf);

	/*
	 * Set PTS (method 1)
	 */
	if (!video_output_file->use_source_timing) {
		video_data_node->vframe->pts = video_codec_ctx->frame_number;
	}	
	
	/* Encoding video */
	{
		int got_packet = 0;
		AVPacket pkt;
		av_init_packet(&pkt);
		pkt.data = video_output_file->vbuf;
		pkt.size = video_output_file->vbuf_size;
		video_data_node->vframe->pkt_dts = video_data_node->vframe->pkt_pts = video_data_node->vframe->pts;
#ifdef GPAC_USE_LIBAV
		video_output_file->encoded_frame_size = avcodec_encode_video(video_codec_ctx, video_output_file->vbuf, video_output_file->vbuf_size, video_data_node->vframe);
#else
		video_output_file->encoded_frame_size = avcodec_encode_video2(video_codec_ctx, &pkt, video_data_node->vframe, &got_packet);
#endif
		if (video_output_file->encoded_frame_size >= 0) {
			video_output_file->encoded_frame_size = pkt.size;
			if (got_packet) {	
				video_codec_ctx->coded_frame->pts = pkt.pts;
				video_codec_ctx->coded_frame->pkt_dts = pkt.dts;
				video_codec_ctx->coded_frame->key_frame = !!(pkt.flags & AV_PKT_FLAG_KEY);
			}
		}
	}

	dc_consumer_advance(&video_output_file->consumer);

	if (video_output_file->encoded_frame_size < 0) {
		fprintf(stderr, "Error occured while encoding video frame.\n");
		return -1;
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DashCast] Video Frame TS "LLU" encoded at UTC "LLU" ms\n", /*video_data_node->source_number, */video_data_node->vframe->pts, gf_net_get_utc() ));

	/* if zero size, it means the image was buffered */
//	if (out_size > 0) {
//
//		av_init_packet(&pkt);
//		pkt.data = NULL;
//		pkt.size = 0;
//
//		if (video_codec_ctx->coded_frame->pts != AV_NOPTS_VALUE) {
//			pkt.pts = av_rescale_q(video_codec_ctx->coded_frame->pts,
//					video_codec_ctx->time_base, video_stream->time_base);
//		}
//
//
//		if (video_codec_ctx->coded_frame->key_frame)
//			pkt.flags |= AV_PKT_FLAG_KEY;
//
//		pkt.stream_index = video_stream->index;
//		pkt.data = video_output_file->vbuf;
//		pkt.size = out_size;
//
//		// write the compressed frame in the media file
//		if (av_interleaved_write_frame(video_output_file->fmt, &pkt)
//				!= 0) {
//			fprintf(stderr, "Writing frame is not successful\n");
//			return -1;
//		}
//
//		av_free_packet(&pkt);
//
//	}

	return video_output_file->encoded_frame_size;
}

void dc_video_encoder_close(VideoOutputFile *video_output_file)
{
//	int i;
//
//	// free the streams
//	for (i = 0; i < video_output_file->fmt->nb_streams; i++) {
//		avcodec_close(video_output_file->fmt->streams[i]->codec);
//		av_freep(&video_output_file->fmt->streams[i]->info);
//	}
	av_free(video_output_file->vbuf);
	avcodec_close(video_output_file->codec_ctx);
	av_free(video_output_file->codec_ctx);
}
