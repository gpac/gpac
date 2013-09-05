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

int dc_video_encoder_open(VideoOutputFile * p_voutf, VideoData * p_vdata) {

	//AVCodec * p_video_codec;
	//AVStream * p_video_stream;

	p_voutf->i_vbuf_size = 9 * p_vdata->i_width * p_vdata->i_height + 10000;
	p_voutf->p_vbuf = (uint8_t *) av_malloc(p_voutf->i_vbuf_size);

//	p_voutf->p_codec = avcodec_find_encoder_by_name("libx264"/*p_vdata->psz_codec*/);
	p_voutf->p_codec = avcodec_find_encoder(CODEC_ID_H264);
	if (p_voutf->p_codec == NULL) {
		fprintf(stderr, "Output video codec not found\n");
		return -1;
	}

	p_voutf->p_codec_ctx = avcodec_alloc_context3(p_voutf->p_codec);

	//Create new video stream
//	p_video_stream = avformat_new_stream(p_voutf->p_fmt, p_video_codec);
//	if (!p_video_stream) {
//		fprintf(stderr, "Cannot create output video stream\n");
//		return -1;
//	}

	p_voutf->p_codec_ctx->codec_id = p_voutf->p_codec->id;
	p_voutf->p_codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
	p_voutf->p_codec_ctx->bit_rate = p_vdata->i_bitrate;
	p_voutf->p_codec_ctx->width = p_vdata->i_width;
	p_voutf->p_codec_ctx->height = p_vdata->i_height;
	{
		AVRational time_base; 
		time_base.num = 1;
		time_base.den = p_vdata->i_framerate;
		p_voutf->p_codec_ctx->time_base = time_base;
	}
	p_voutf->p_codec_ctx->pix_fmt = PIX_FMT_YUV420P;
	p_voutf->p_codec_ctx->gop_size = /*p_voutf->i_gop_size;*/p_vdata->i_framerate;

	av_opt_set(p_voutf->p_codec_ctx->priv_data, "preset", "ultrafast", 0);
	av_opt_set(p_voutf->p_codec_ctx->priv_data, "tune", "zerolatency", 0);

	if(p_voutf->i_gdr == 1) {
		av_opt_set_int(p_voutf->p_codec_ctx->priv_data, "intra-refresh", 1, 0);
		av_opt_set_int(p_voutf->p_codec_ctx->priv_data, "key-int", 8, 0);
	}

//	if (p_voutf->p_fmt->oformat->flags & AVFMT_GLOBALHEADER)
	//the global header gives access to the extradata (SPS/PPS)
	p_voutf->p_codec_ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;

//	p_voutf->p_codec_ctx->codec_id = p_video_codec->id;
//	p_voutf->p_codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
//	p_voutf->p_codec_ctx->bit_rate = p_vdata->i_bitrate;
//	p_voutf->p_codec_ctx->width = p_vdata->i_width;
//	p_voutf->p_codec_ctx->height = p_vdata->i_height;
//	p_voutf->p_codec_ctx->time_base = (AVRational) {1 ,
//				p_voutf->p_vdata->i_framerate};
//	p_voutf->p_codec_ctx->codec->pix_fmt = PIX_FMT_YUV420P;
	p_voutf->p_codec_ctx->gop_size = p_vdata->i_framerate;
//
//	av_opt_set(p_voutf->p_codec_ctx->priv_data, "preset", "ultrafast", 0);
//	av_opt_set(p_voutf->p_codec_ctx->priv_data, "tune", "zerolatency", 0);

	/*
	 p_voutf->p_codec_ctx->max_b_frames = 0;
	 p_voutf->p_codec_ctx->thread_count = 1;
	 p_voutf->p_codec_ctx->delay = 0;
	 p_voutf->p_codec_ctx->rc_lookahead = 0;
	 */

	/*
	 * p_video_stream->codec->gop_size = p_voutf->i_vfr;
	 * videoStream->codec->gop_size = 1;
	 * p_video_stream->codec->rc_lookahead = 0;
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

//	if (p_voutf->p_fmt->oformat->flags & AVFMT_GLOBALHEADER)
//		p_video_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;

	p_voutf->i_vstream_idx = 0;//p_video_stream->index;

	/* open the video codec */
	if (avcodec_open2(p_voutf->p_codec_ctx, p_voutf->p_codec, NULL) < 0) {
		fprintf(stderr, "Cannot open output video codec\n");
		return -1;
	}

//	/* open the video codec */
//	if (avcodec_open2(p_video_stream->codec, p_video_codec, NULL) < 0) {
//		fprintf(stderr, "Cannot open output video codec\n");
//		return -1;
//	}

	return 0;
}

int dc_video_encoder_encode(VideoOutputFile * p_voutf, VideoScaledData * p_vsd) {

	//AVPacket pkt;
	VideoDataNode * p_vn;
	int ret;
	//int i_out_size;

//	AVStream * p_video_stream = p_voutf->p_fmt->streams[p_voutf->i_vstream_idx];
//	AVCodecContext * p_video_codec_ctx = p_video_stream->codec;
	AVCodecContext * p_video_codec_ctx = p_voutf->p_codec_ctx;


	ret = dc_consumer_lock(&p_voutf->vcon, &p_vsd->p_cb);

	if (ret < 0) {
#ifdef DEBUG
		printf("Video encoder got to end of buffer!\n");
#endif

		return -2;
	}

	dc_consumer_unlock_previous(&p_voutf->vcon, &p_vsd->p_cb);

	p_vn = (VideoDataNode *) dc_consumer_consume(&p_voutf->vcon, &p_vsd->p_cb);

	/*
	 * Set PTS (method 1)
	 */
	p_vn->p_vframe->pts = p_video_codec_ctx->frame_number;


	/*
	 * Set PTS (method 2)
	 */
	//int64_t now = av_gettime();
	//p_vn->p_vframe->pts = av_rescale_q(now, AV_TIME_BASE_Q,
	//p_video_codec_ctx->time_base);


	/* Encoding video */
	p_voutf->i_encoded_frame_size = avcodec_encode_video(p_video_codec_ctx,
			p_voutf->p_vbuf, p_voutf->i_vbuf_size, p_vn->p_vframe);

	dc_consumer_advance(&p_voutf->vcon);

	if (p_voutf->i_encoded_frame_size < 0) {
		fprintf(stderr, "Error occured while encoding video frame.\n");
		return -1;
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DashCast] Video Frame TS "LLU" encoded at UTC "LLU" ms\n", /*p_vn->source_number, */p_vn->p_vframe->pts, gf_net_get_utc() ));

	/* if zero size, it means the image was buffered */
//	if (i_out_size > 0) {
//
//		av_init_packet(&pkt);
//		pkt.data = NULL;
//		pkt.size = 0;
//
//		if (p_video_codec_ctx->coded_frame->pts != AV_NOPTS_VALUE) {
//			pkt.pts = av_rescale_q(p_video_codec_ctx->coded_frame->pts,
//					p_video_codec_ctx->time_base, p_video_stream->time_base);
//		}
//
//
//		if (p_video_codec_ctx->coded_frame->key_frame)
//			pkt.flags |= AV_PKT_FLAG_KEY;
//
//		pkt.stream_index = p_video_stream->index;
//		pkt.data = p_voutf->p_vbuf;
//		pkt.size = i_out_size;
//
//		// write the compressed frame in the media file
//		if (av_interleaved_write_frame(p_voutf->p_fmt, &pkt)
//				!= 0) {
//			fprintf(stderr, "Writing frame is not successful\n");
//			return -1;
//		}
//
//		av_free_packet(&pkt);
//
//	}

	return p_voutf->i_encoded_frame_size;
}

void dc_video_encoder_close(VideoOutputFile * p_voutf) {

//	int i;
//
//	// free the streams
//	for (i = 0; i < p_voutf->p_fmt->nb_streams; i++) {
//		avcodec_close(p_voutf->p_fmt->streams[i]->codec);
//		av_freep(&p_voutf->p_fmt->streams[i]->info);
//	}

	av_free(p_voutf->p_vbuf);

	avcodec_close(p_voutf->p_codec_ctx);
	av_free(p_voutf->p_codec_ctx);

}
