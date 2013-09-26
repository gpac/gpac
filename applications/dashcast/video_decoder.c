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

#include "video_decoder.h"
#include <time.h>
#include <gpac/network.h>

//#define DASHCAST_DEBUG_TIME_

int dc_video_decoder_open(VideoInputFile * p_vin, VideoData * p_vdata,
		int i_mode, int i_no_loop) {

	s32 ret;
	u32 i;
	s32 open_res;
	AVInputFormat * p_in_fmt = NULL;
	AVDictionary * p_options = NULL;
	AVCodecContext * p_codec_ctx;
	AVCodec * p_codec;

	memset(p_vin, 0, sizeof(VideoInputFile));

	if (p_vdata->i_width > 0 && p_vdata->i_height > 0) {
		char vres[16];
		sprintf(vres, "%dx%d", p_vdata->i_width, p_vdata->i_height);
		ret = av_dict_set(&p_options, "video_size", vres, 0);
		if (ret < 0) {
			printf("Could not set video size %s.\n", vres);
			return -1;
		}
	}

	if (p_vdata->i_framerate > 0) {
		char vfr[16];
		sprintf(vfr, "%d", p_vdata->i_framerate);
		ret = av_dict_set(&p_options, "framerate", vfr, 0);
		if (ret < 0) {
			printf("Could not set video framerate %s.\n", vfr);
			return -1;
		}
	}

	if (strlen(p_vdata->pixel_format)) {
		ret = av_dict_set(&p_options, "pixel_format", p_vdata->pixel_format, 0);
		if (ret < 0) {
			printf("Could not set pixel format %s.\n", p_vdata->pixel_format);
			return -1;
		}
	}

#ifndef WIN32
	if (strcmp(p_vdata->psz_v4l2f, "") != 0) {
		ret = av_dict_set(&p_options, "input_format", p_vdata->psz_v4l2f, 0);
		if (ret < 0) {
			printf("Could not set input format %s.\n", p_vdata->psz_v4l2f);
			return -1;
		}
	}
#endif
	
	if (p_vdata->psz_format && strcmp(p_vdata->psz_format, "") != 0) {
		p_in_fmt = av_find_input_format(p_vdata->psz_format);
		if (p_in_fmt == NULL) {
			printf("Cannot find the format %s.\n", p_vdata->psz_format);
			return -1;
		}
	}

	p_vin->p_fmt_ctx = NULL;

	/*  Open video */
	open_res = avformat_open_input(&p_vin->p_fmt_ctx, p_vdata->psz_name, p_in_fmt, p_options ? &p_options : NULL);
	if ( (open_res < 0) && !stricmp(p_vdata->psz_name, "screen-capture-recorder") ) {
		fprintf(stderr, "Buggy screen capture input (open failed with code %d), retrying without specifying resolution\n", open_res);
		av_dict_set(&p_options, "video_size", NULL, 0);
		open_res = avformat_open_input(&p_vin->p_fmt_ctx, p_vdata->psz_name, p_in_fmt, p_options ? &p_options : NULL);
		
	}

	if ( (open_res < 0) && p_options) {
		fprintf(stderr, "Error %d opening input - retrying without options\n", open_res);
		av_dict_free(&p_options);
		open_res = avformat_open_input(&p_vin->p_fmt_ctx, p_vdata->psz_name, p_in_fmt, NULL);
	}

	if (open_res < 0) {
		fprintf(stderr, "Cannot open file %s\n", p_vdata->psz_name);
		return -1;
	}

	/*  Retrieve stream information */
	if (avformat_find_stream_info(p_vin->p_fmt_ctx, NULL) < 0) {
		fprintf(stderr, "Cannot find stream information\n");
		return -1;
	}

	av_dump_format(p_vin->p_fmt_ctx, 0, p_vdata->psz_name, 0);

	/*  Find the first video stream */
	p_vin->i_vstream_idx = -1;
	for (i = 0; i < p_vin->p_fmt_ctx->nb_streams; i++) {
		if (p_vin->p_fmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			p_vin->i_vstream_idx = i;
			break;
		}
	}
	if (p_vin->i_vstream_idx == -1) {
		fprintf(stderr, "Cannot find a video stream\n");
		return -1;
	}

	/*  Get a pointer to the codec context for the video stream */
	p_codec_ctx = p_vin->p_fmt_ctx->streams[p_vin->i_vstream_idx]->codec;

	//int64_t dur = p_vin->p_fmt_ctx->streams[p_vin->i_vstream_idx]->duration;
	//int time_base = p_vin->p_fmt_ctx->streams[p_vin->i_vstream_idx]->time_base.den;
	//printf("DURATION: %d \n", dur/time_base);

	/*  Find the decoder for the video stream */
	p_codec = avcodec_find_decoder(p_codec_ctx->codec_id);
	if (p_codec == NULL) {
		fprintf(stderr, "Codec is not supported.\n");
		avformat_close_input(&p_vin->p_fmt_ctx);
		return -1;
	}

	/*  Open codec */
	if (avcodec_open2(p_codec_ctx, p_codec, NULL) < 0) {
		fprintf(stderr, "Cannot open codec.\n");
		avformat_close_input(&p_vin->p_fmt_ctx);
		return -1;
	}

	p_vin->i_width = p_codec_ctx->width;
	p_vin->i_height = p_codec_ctx->height;
	p_vin->i_pix_fmt = p_codec_ctx->pix_fmt;

	p_vdata->i_framerate = p_codec_ctx->time_base.den / p_codec_ctx->time_base.num;
	
	if (p_vdata->i_framerate <= 1 || p_vdata->i_framerate > 1000) {

		p_vdata->i_framerate =
				p_vin->p_fmt_ctx->streams[p_vin->i_vstream_idx]->avg_frame_rate.num
						/ (p_vin->p_fmt_ctx->streams[p_vin->i_vstream_idx]->avg_frame_rate.den
								== 0 ?
								1 :
								p_vin->p_fmt_ctx->streams[p_vin->i_vstream_idx]->avg_frame_rate.den);

		if (p_vdata->i_framerate / 1000 != 0)
			p_vdata->i_framerate = p_vdata->i_framerate / 1000;

		if (p_vdata->i_framerate <= 1 || p_vdata->i_framerate > 1000) {
			p_vdata->i_framerate = p_vin->p_fmt_ctx->streams[p_vin->i_vstream_idx]->avg_frame_rate.num / (p_vin->p_fmt_ctx->streams[p_vin->i_vstream_idx]->avg_frame_rate.den
			== 0 ? 1 : p_vin->p_fmt_ctx->streams[p_vin->i_vstream_idx]->avg_frame_rate.den);

			if (p_vdata->i_framerate / 1000 != 0) {
				p_vdata->i_framerate = p_vdata->i_framerate / 1000;
			}
		}
	}

	if (p_vdata->i_framerate <= 1 || p_vdata->i_framerate > 1000) {
		printf("Invalid input framerate.\n");
		return -1;
	}

	p_vdata->time_base = p_vin->p_fmt_ctx->streams[p_vin->i_vstream_idx]->time_base;
	p_vin->i_mode = i_mode;
	p_vin->i_no_loop = i_no_loop;

	return 0;
}

int dc_video_decoder_read(VideoInputFile * p_in_ctx, VideoInputData * p_vd,
		int source_number, int use_source_timing) {

#ifdef DASHCAST_DEBUG_TIME_
	struct timeval start, end;
	long elapsed_time;
#endif
	int ret;
	AVPacket packet;
	int i_got_frame;
	int already_locked = 0;
	AVCodecContext * p_codec_ctx;
	VideoDataNode * p_vdn;
	AVStream *p_video_stream;

	/*  Get a pointer to the codec context for the video stream */
	p_codec_ctx = p_in_ctx->p_fmt_ctx->streams[p_in_ctx->i_vstream_idx]->codec;
	p_video_stream = p_in_ctx->p_fmt_ctx->streams[p_in_ctx->i_vstream_idx];

	/*  Read frames */
	while (1) {

#ifdef DASHCAST_DEBUG_TIME_
		gettimeofday(&start, NULL);
#endif

		memset(&packet, 0, sizeof(AVPacket));
		ret = av_read_frame(p_in_ctx->p_fmt_ctx, &packet);

#ifdef DASHCAST_DEBUG_TIME_
		gettimeofday(&end, NULL);
		elapsed_time = (end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec);
		printf("fps: %f\n", 1000000.0/elapsed_time);
#endif

		if (ret == AVERROR_EOF) {

			if (p_in_ctx->i_mode == LIVE_MEDIA && p_in_ctx->i_no_loop == 0) {
				av_seek_frame(p_in_ctx->p_fmt_ctx, p_in_ctx->i_vstream_idx, 0,
						0);
				av_free_packet(&packet);
				continue;
			}

			dc_producer_lock(&p_vd->pro, &p_vd->p_cb);
			dc_producer_unlock_previous(&p_vd->pro, &p_vd->p_cb);
			p_vdn = (VideoDataNode *) dc_producer_produce(&p_vd->pro,
					&p_vd->p_cb);

			p_vdn->source_number = source_number;
			/* Flush decoder */
			memset(&packet, 0, sizeof(AVPacket));
			avcodec_get_frame_defaults(p_vdn->p_vframe);
			avcodec_decode_video2(p_codec_ctx, p_vdn->p_vframe, &i_got_frame,
					&packet);

			if (i_got_frame) {
				dc_producer_advance(&p_vd->pro);
				return 0;
			}

			dc_producer_end_signal(&p_vd->pro, &p_vd->p_cb);
			dc_producer_unlock(&p_vd->pro, &p_vd->p_cb);
			return -2;
		}

		else if (ret < 0) {
			fprintf(stderr, "Cannot read video frame.\n");
			continue;
		}

		/*  Is this a packet from the video stream? */
		if (packet.stream_index == p_in_ctx->i_vstream_idx) {

			if (!already_locked) {

				if (dc_producer_lock(&p_vd->pro, &p_vd->p_cb) < 0) {
					printf("[dashcast] Live system dropped a video frame\n");
					continue;
				}

				dc_producer_unlock_previous(&p_vd->pro, &p_vd->p_cb);

				already_locked = 1;
			}

			p_vdn = (VideoDataNode *) dc_producer_produce(&p_vd->pro, &p_vd->p_cb);

			p_vdn->source_number = source_number;

			/*  Set video frame to default */
			avcodec_get_frame_defaults(p_vdn->p_vframe);

			/*  Decode video frame */
			if (avcodec_decode_video2(p_codec_ctx, p_vdn->p_vframe, &i_got_frame, &packet) < 0) {
				av_free_packet(&packet);
				fprintf(stderr, "Error while decoding video.\n");
				dc_producer_end_signal(&p_vd->pro, &p_vd->p_cb);
				dc_producer_unlock(&p_vd->pro, &p_vd->p_cb);
				return -1;
			}

			if (use_source_timing) {
				if (p_in_ctx->pts_init == 0) {
					p_in_ctx->pts_init = 1;
					p_in_ctx->first_pts = packet.pts;
					//TODO - check with audio if sync is OK 
				} else if (p_in_ctx->pts_init == 1) {
					p_in_ctx->pts_init = 2;
					p_vd->frame_duration = packet.pts - p_in_ctx->first_pts;
				}
				p_vdn->p_vframe->pts = packet.pts - p_in_ctx->first_pts;	
				if (p_vdn->p_vframe->pts - p_in_ctx->prev_pts < p_vd->frame_duration)
					p_vd->frame_duration = p_vdn->p_vframe->pts - p_in_ctx->prev_pts;

				p_in_ctx->prev_pts = p_vdn->p_vframe->pts;
			}


			/*  Did we get a video frame? */
			if (i_got_frame) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DashCast] Video Frame TS "LLU" decoded at UTC "LLU" ms\n", p_vdn->p_vframe->pts, gf_net_get_utc() ));
				// For a decode/encode process we must free this memory.
				//But if the input is raw and there is no need to decode then
				// the packet is directly passed for decoded frame. So freeing it cause problem.
				if (p_codec_ctx->codec->id == CODEC_ID_RAWVIDEO) {
					p_vdn->is_raw_data = 1;
#ifdef GPAC_USE_LIBAV
					//we don't have ref count in libav, store packet (contains raw video) and destroy it later
					p_vdn->raw_packet = packet;
					dc_producer_advance(&p_vd->pro);
					return 0;
#else
					p_vdn->p_vframe = av_frame_clone(p_vdn->p_vframe);
#endif
				}
				av_free_packet(&packet);
				dc_producer_advance(&p_vd->pro);
				return 0;
			}
		}

		/*  Free the packet that was allocated by av_read_frame */
		av_free_packet(&packet);

	}

	fprintf(stderr, "Unknown error while reading video frame.\n");
	return -1;

}

void dc_video_decoder_close(VideoInputFile * p_in_ctx) {

	/*  Close the video format context */
	avformat_close_input(&p_in_ctx->p_fmt_ctx);

}
