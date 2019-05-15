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

#endif


//#define DEBUG 1


/**
 * A function which pushes argument to a libav codec using its private data.
 * param priv_data
 * param options a list of space separated and ':' affected options (e.g. "a:b c:d e:f"). @options be non NULL.
 */
void build_dict(void *priv_data, const char *options) {
	char *opt = gf_strdup(options);
	char *tok = strtok(opt, "=");
	char *tokval = NULL;
	while (tok && (tokval=strtok(NULL, " "))) {
		if (av_opt_set(priv_data, tok, tokval, 0) < 0)
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Unknown custom option \"%s\" with value \"%s\" in %s\n", tok, tokval, options));
		tok = strtok(NULL, "=");
	}
	gf_free(opt);
}

int dc_video_encoder_open(VideoOutputFile *video_output_file, VideoDataConf *video_data_conf, Bool use_source_timing, AVRational sar)
{
	video_output_file->vbuf_size = 9 * video_data_conf->width * video_data_conf->height + 10000;
	video_output_file->vbuf = (uint8_t *) av_malloc(video_output_file->vbuf_size);
	video_output_file->video_data_conf = video_data_conf;

	video_output_file->codec = avcodec_find_encoder_by_name(video_data_conf->codec);
	if (video_output_file->codec == NULL) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Output video codec %s not found\n", video_data_conf->codec));
		return -1;
	}

	video_output_file->codec_ctx = avcodec_alloc_context3(video_output_file->codec);

	video_output_file->codec_ctx->codec_id = video_output_file->codec->id;
	video_output_file->codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
	video_output_file->codec_ctx->bit_rate = video_data_conf->bitrate;
	video_output_file->codec_ctx->width = video_data_conf->width;
	video_output_file->codec_ctx->height = video_data_conf->height;
	video_output_file->codec_ctx->sample_aspect_ratio = sar;

	video_output_file->codec_ctx->time_base.num = 1;
	video_output_file->codec_ctx->time_base.den = video_output_file->gop_size ? video_output_file->gop_size : video_data_conf->framerate;

	video_output_file->use_source_timing = use_source_timing;
	if (use_source_timing) {
		//for avcodec to do rate allocation, we need to have ctx->timebase == 1/framerate
		video_output_file->codec_ctx->time_base.den = video_data_conf->time_base.den;
		video_output_file->codec_ctx->time_base.num = video_data_conf->time_base.num * video_data_conf->time_base.den / video_data_conf->framerate;
	}
	video_output_file->codec_ctx->pix_fmt = PIX_FMT_YUV420P;
	video_output_file->codec_ctx->gop_size = video_data_conf->framerate;

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

	if ( strlen(video_data_conf->custom) ) {
		build_dict(video_output_file->codec_ctx->priv_data, video_data_conf->custom);
	} else if (video_data_conf->low_delay) {
		GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("Video Encoder: applying default options (preset=ultrafast tune=zerolatency)\n"));
		av_opt_set(video_output_file->codec_ctx->priv_data, "vprofile", "baseline", 0);
		av_opt_set(video_output_file->codec_ctx->priv_data, "preset", "ultrafast", 0);
		av_opt_set(video_output_file->codec_ctx->priv_data, "tune", "zerolatency", 0);
		if (strstr(video_data_conf->codec, "264")) {
			av_opt_set(video_output_file->codec_ctx->priv_data, "x264opts", "no-mbtree:sliced-threads:sync-lookahead=0", 0);
		}
	}

	if (video_output_file->gdr) {
		av_opt_set_int(video_output_file->codec_ctx->priv_data, "intra-refresh", 1, 0);
		av_opt_set_int(video_output_file->codec_ctx->priv_data, "key-int", video_output_file->gdr, 0);
	}

#ifdef AV_CODEC_FLAG_GLOBAL_HEADER
	//the global header gives access to the extradata (SPS/PPS)
	video_output_file->codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
#endif

	video_output_file->vstream_idx = 0;//video_stream->index;

	/* open the video codec - options are passed thru video_output_file->codec_ctx->priv_data */
	if (avcodec_open2(video_output_file->codec_ctx, video_output_file->codec, NULL) < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot open output video codec\n"));
		return -1;
	}

	video_output_file->rep_id = video_data_conf->filename;
	return 0;
}

int dc_video_encoder_encode(VideoOutputFile *video_output_file, VideoScaledData *video_scaled_data)
{
	VideoScaledDataNode *video_data_node;
	int ret;
	u64 time_spent;
	int got_packet = 0;
	AVPacket pkt;
	
	AVCodecContext *video_codec_ctx = video_output_file->codec_ctx;

	//FIXME: deadlock when pressing 'q' with BigBuckBunny_640x360.m4v
	ret = dc_consumer_lock(&video_output_file->consumer, &video_scaled_data->circular_buf);
	if (ret < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Video encoder got an end of buffer!\n"));
		return -2;
	}

	if (video_scaled_data->circular_buf.size > 1)
		dc_consumer_unlock_previous(&video_output_file->consumer, &video_scaled_data->circular_buf);

	video_data_node = (VideoScaledDataNode*)dc_consumer_consume(&video_output_file->consumer, &video_scaled_data->circular_buf);

	/*
	 * Set PTS (method 1)
	 */
	if (!video_output_file->use_source_timing) {
		video_data_node->vframe->pts = video_codec_ctx->frame_number;
	}

	time_spent = gf_sys_clock_high_res();
	/* Encoding video */
	av_init_packet(&pkt);
	pkt.data = video_output_file->vbuf;
	pkt.size = video_output_file->vbuf_size;
	pkt.pts = pkt.dts = video_data_node->vframe->pkt_dts = video_data_node->vframe->pkt_pts = video_data_node->vframe->pts;
	video_data_node->vframe->pict_type = 0;
	video_data_node->vframe->width = video_codec_ctx->width;
	video_data_node->vframe->height = video_codec_ctx->height;
	video_data_node->vframe->format = video_codec_ctx->pix_fmt;


#ifdef LIBAV_ENCODE_OLD
	if (!video_output_file->segment_started)
		video_data_node->vframe->pict_type = FF_I_TYPE;

	video_output_file->encoded_frame_size = avcodec_encode_video(video_codec_ctx, video_output_file->vbuf, video_output_file->vbuf_size, video_data_node->vframe);
	got_packet = video_output_file->encoded_frame_size>=0 ? 1 : 0;
#else
	//this is correct but unfortunately doesn't work with some versions of FFMPEG (output is just grey video ...)
	if (!video_output_file->segment_started)
		video_data_node->vframe->pict_type = AV_PICTURE_TYPE_I;

	video_output_file->encoded_frame_size = avcodec_encode_video2(video_codec_ctx, &pkt, video_data_node->vframe, &got_packet);
#endif

	time_spent = gf_sys_clock_high_res() - time_spent;
	//this is not true with libav !
#ifndef GPAC_USE_LIBAV
	if (video_output_file->encoded_frame_size >= 0)
		video_output_file->encoded_frame_size = pkt.size;
#else
	if (got_packet)
		video_output_file->encoded_frame_size = pkt.size;
#endif
	if (video_output_file->encoded_frame_size >= 0) {
		if (got_packet) {
			video_codec_ctx->coded_frame->pts = video_codec_ctx->coded_frame->pkt_pts = pkt.pts;
			video_codec_ctx->coded_frame->pkt_dts = pkt.dts;
			video_codec_ctx->coded_frame->key_frame = (pkt.flags & AV_PKT_FLAG_KEY) ? 1 : 0;
			video_output_file->frame_ntp = video_data_node->frame_ntp;
			video_output_file->frame_utc = video_data_node->frame_utc;

			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DashCast] Video %s Frame TS "LLU" encoded at UTC "LLU" ms in "LLU" us size %d bytes\n", video_output_file->rep_id, pkt.pts, gf_net_get_utc(), time_spent, video_output_file->encoded_frame_size ));
		}
	}

	dc_consumer_advance(&video_output_file->consumer);

	if (video_scaled_data->circular_buf.size == 1)
		dc_consumer_unlock_previous(&video_output_file->consumer, &video_scaled_data->circular_buf);

	if (video_output_file->encoded_frame_size < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Error occured while encoding video frame.\n"));
		return -1;
	}

	/* if zero size, it means the image was buffered */
//	if (out_size > 0) {
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
//		if (av_interleaved_write_frame(video_output_file->av_fmt_ctx, &pkt)
//				!= 0) {
//			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Writing frame is not successful\n"));
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
//	for (i = 0; i < video_output_file->av_fmt_ctx->nb_streams; i++) {
//		avcodec_close(video_output_file->av_fmt_ctx->streams[i]->codec);
//		av_freep(&video_output_file->av_fmt_ctx->streams[i]->info);
//	}
	av_free(video_output_file->vbuf);
	avcodec_close(video_output_file->codec_ctx);
	av_free(video_output_file->codec_ctx);
}
