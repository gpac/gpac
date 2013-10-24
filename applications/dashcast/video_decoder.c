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


int dc_video_decoder_open(VideoInputFile *video_input_data, VideoDataConf *video_data_conf, int mode, int no_loop)
{
	s32 ret;
	u32 i;
	s32 open_res;
	AVInputFormat *in_fmt = NULL;
	AVDictionary *options = NULL;
	AVCodecContext *codec_ctx;
	AVCodec *codec;

	memset(video_input_data, 0, sizeof(VideoInputFile));

	if (video_data_conf->width > 0 && video_data_conf->height > 0) {
		char vres[16];
		snprintf(vres, sizeof(vres), "%dx%d", video_data_conf->width, video_data_conf->height);
		ret = av_dict_set(&options, "video_size", vres, 0);
		if (ret < 0) {
			fprintf(stderr, "Could not set video size %s.\n", vres);
			return -1;
		}
	}

	if (video_data_conf->framerate > 0) {
		char vfr[16];
		snprintf(vfr, sizeof(vfr), "%d", video_data_conf->framerate);
		ret = av_dict_set(&options, "framerate", vfr, 0);
		if (ret < 0) {
			fprintf(stderr, "Could not set video framerate %s.\n", vfr);
			return -1;
		}
	}

	if (strlen(video_data_conf->pixel_format)) {
		ret = av_dict_set(&options, "pixel_format", video_data_conf->pixel_format, 0);
		if (ret < 0) {
			fprintf(stderr, "Could not set pixel format %s.\n", video_data_conf->pixel_format);
			return -1;
		}
	}

#ifndef WIN32
	if (strcmp(video_data_conf->v4l2f, "") != 0) {
		ret = av_dict_set(&options, "input_format", video_data_conf->v4l2f, 0);
		if (ret < 0) {
			fprintf(stderr, "Could not set input format %s.\n", video_data_conf->v4l2f);
			return -1;
		}
	}
#endif
	
	if (video_data_conf->format && strcmp(video_data_conf->format, "") != 0) {
		in_fmt = av_find_input_format(video_data_conf->format);
		if (in_fmt == NULL) {
			fprintf(stderr, "Cannot find the format %s.\n", video_data_conf->format);
			return -1;
		}
	}

	video_input_data->fmt_ctx = NULL;

	/* Open video */
	open_res = avformat_open_input(&video_input_data->fmt_ctx, video_data_conf->filename, in_fmt, options ? &options : NULL);
	if ( (open_res < 0) && !stricmp(video_data_conf->filename, "screen-capture-recorder") ) {
		fprintf(stderr, "Buggy screen capture input (open failed with code %d), retrying without specifying resolution\n", open_res);
		av_dict_set(&options, "video_size", NULL, 0);
		open_res = avformat_open_input(&video_input_data->fmt_ctx, video_data_conf->filename, in_fmt, options ? &options : NULL);
	}

	if ( (open_res < 0) && options) {
		fprintf(stderr, "Error %d opening input - retrying without options\n", open_res);
		av_dict_free(&options);
		open_res = avformat_open_input(&video_input_data->fmt_ctx, video_data_conf->filename, in_fmt, NULL);
	}

	if (open_res < 0) {
		fprintf(stderr, "Cannot open file %s\n", video_data_conf->filename);
		return -1;
	}

	/* Retrieve stream information */
	if (avformat_find_stream_info(video_input_data->fmt_ctx, NULL) < 0) {
		fprintf(stderr, "Cannot find stream information\n");
		return -1;
	}

	av_dump_format(video_input_data->fmt_ctx, 0, video_data_conf->filename, 0);

	/* Find the first video stream */
	video_input_data->vstream_idx = -1;
	for (i = 0; i < video_input_data->fmt_ctx->nb_streams; i++) {
		if (video_input_data->fmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			video_input_data->vstream_idx = i;
			break;
		}
	}
	if (video_input_data->vstream_idx == -1) {
		fprintf(stderr, "Cannot find a video stream\n");
		return -1;
	}

	/* Get a pointer to the codec context for the video stream */
	codec_ctx = video_input_data->fmt_ctx->streams[video_input_data->vstream_idx]->codec;

	//int64_t dur = video_input_data->fmt_ctx->streams[video_input_data->vstream_idx]->duration;
	//int time_base = video_input_data->fmt_ctx->streams[video_input_data->vstream_idx]->time_base.den;
	//fprintf(stdout, "DURATION: %d \n", dur/time_base);

	/* Find the decoder for the video stream */
	codec = avcodec_find_decoder(codec_ctx->codec_id);
	if (codec == NULL) {
		fprintf(stderr, "Codec is not supported.\n");
		avformat_close_input(&video_input_data->fmt_ctx);
		return -1;
	}

	/* Open codec */
	if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
		fprintf(stderr, "Cannot open codec.\n");
		avformat_close_input(&video_input_data->fmt_ctx);
		return -1;
	}

	video_input_data->width = codec_ctx->width;
	video_input_data->height = codec_ctx->height;
	video_input_data->pix_fmt = codec_ctx->pix_fmt;
	video_data_conf->framerate = codec_ctx->time_base.den / codec_ctx->time_base.num;	
	if (video_data_conf->framerate <= 1 || video_data_conf->framerate > 1000) {
		const int num = video_input_data->fmt_ctx->streams[video_input_data->vstream_idx]->avg_frame_rate.num;
		const int den = video_input_data->fmt_ctx->streams[video_input_data->vstream_idx]->avg_frame_rate.den == 0 ? 1 : video_input_data->fmt_ctx->streams[video_input_data->vstream_idx]->avg_frame_rate.den;
		video_data_conf->framerate = num / den;
		if (video_data_conf->framerate / 1000 != 0) {
			fprintf(stderr, "Framerate %d was divided by 1000: %d\n", video_data_conf->framerate, video_data_conf->framerate/1000);
			video_data_conf->framerate = video_data_conf->framerate / 1000;
		}

		if (video_data_conf->framerate <= 1 || video_data_conf->framerate > 1000) {
			video_data_conf->framerate = num / den;
			if (video_data_conf->framerate / 1000 != 0) {
				video_data_conf->framerate = video_data_conf->framerate / 1000;
			}
		}
	}

	if (video_data_conf->framerate <= 1 || video_data_conf->framerate > 1000) {
		fprintf(stderr, "Invalid input framerate.\n");
		return -1;
	}

	video_data_conf->time_base = video_input_data->fmt_ctx->streams[video_input_data->vstream_idx]->time_base;
	video_input_data->mode = mode;
	video_input_data->no_loop = no_loop;

	return 0;
}

int dc_video_decoder_read(VideoInputFile *video_input_file, VideoInputData *video_input_data, int source_number, int use_source_timing, int is_live_capture)
{
#ifdef DASHCAST_DEBUG_TIME_
	struct timeval start, end;
	long elapsed_time;
#endif
	AVPacket packet;
	int ret, got_frame, already_locked = 0;
	AVCodecContext *codec_ctx;
	VideoDataNode *video_data_node;

	/* Get a pointer to the codec context for the video stream */
	codec_ctx = video_input_file->fmt_ctx->streams[video_input_file->vstream_idx]->codec;

	/* Read frames */
	while (1) {
#ifdef DASHCAST_DEBUG_TIME_
		gettimeofday(&start, NULL);
#endif
		memset(&packet, 0, sizeof(AVPacket));
		ret = av_read_frame(video_input_file->fmt_ctx, &packet);
#ifdef DASHCAST_DEBUG_TIME_
		gettimeofday(&end, NULL);
		elapsed_time = (end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec);
		fprintf(stdout, "fps: %f\n", 1000000.0/elapsed_time);
#endif
		if (ret == AVERROR_EOF) {
			if (video_input_file->mode == LIVE_MEDIA && video_input_file->no_loop == 0) {
				av_seek_frame(video_input_file->fmt_ctx, video_input_file->vstream_idx, 0, 0);
				av_free_packet(&packet);
				continue;
			}

			dc_producer_lock(&video_input_data->producer, &video_input_data->circular_buf);
			dc_producer_unlock_previous(&video_input_data->producer, &video_input_data->circular_buf);
			video_data_node = (VideoDataNode *) dc_producer_produce(&video_input_data->producer, &video_input_data->circular_buf);
			video_data_node->source_number = source_number;
			/* Flush decoder */
			memset(&packet, 0, sizeof(AVPacket));
			avcodec_get_frame_defaults(video_data_node->vframe);
			avcodec_decode_video2(codec_ctx, video_data_node->vframe, &got_frame, &packet);
			if (got_frame) {
				dc_producer_advance(&video_input_data->producer);
				return 0;
			}

			dc_producer_end_signal(&video_input_data->producer, &video_input_data->circular_buf);
			dc_producer_unlock(&video_input_data->producer, &video_input_data->circular_buf);
			return -2;
		}

		else if (ret < 0) {
			fprintf(stderr, "Cannot read video frame.\n");
			continue;
		}

		/* Is this a packet from the video stream? */
		if (packet.stream_index == video_input_file->vstream_idx) {
			if (!already_locked) {
				if (dc_producer_lock(&video_input_data->producer, &video_input_data->circular_buf) < 0) {
					fprintf(stderr, "[dashcast] Live system dropped a video frame\n");
					continue;
				}

				dc_producer_unlock_previous(&video_input_data->producer, &video_input_data->circular_buf);

				already_locked = 1;
			}

			video_data_node = (VideoDataNode *) dc_producer_produce(&video_input_data->producer, &video_input_data->circular_buf);
			video_data_node->source_number = source_number;

			/* Set video frame to default */
			avcodec_get_frame_defaults(video_data_node->vframe);

			/* Decode video frame */
			if (avcodec_decode_video2(codec_ctx, video_data_node->vframe, &got_frame, &packet) < 0) {
				av_free_packet(&packet);
				fprintf(stderr, "Error while decoding video.\n");
				dc_producer_end_signal(&video_input_data->producer, &video_input_data->circular_buf);
				dc_producer_unlock(&video_input_data->producer, &video_input_data->circular_buf);
				return -1;
			}

			/* Did we get a video frame? */
			if (got_frame) {
				if (use_source_timing && is_live_capture) {
					u64 pts;
					if (video_input_file->pts_init == 0) {
						video_input_file->pts_init = 1;
						video_input_file->utc_at_init = gf_net_get_utc();
						video_input_file->first_pts = packet.pts;
						video_input_file->computed_pts = 0;
						video_input_data->frame_duration = codec_ctx->time_base.num;
						video_input_file->sync_tolerance = 9*video_input_data->frame_duration/5;
						//TODO - check with audio if sync is OK 
					} 
					//perform FPS re-linearisation 
					pts = packet.pts - video_input_file->first_pts;	
					if (pts - video_input_file->prev_pts > video_input_file->sync_tolerance) {
						u32 nb_lost=0;
						while (pts > video_input_file->computed_pts) {
							video_input_file->computed_pts += video_input_data->frame_duration;
							nb_lost++;
						}

						if (nb_lost) {
							GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DashCast] Capture lost %d video frames \n", nb_lost));
						}
					}

//					fprintf(stdout, "Capture PTS %g - UTC diff %g - Computed PTS %g\n", (Double) pts / codec_ctx->time_base.den, (Double) (gf_net_get_utc() - video_input_file->utc_at_init) / 1000, (Double) video_input_file->computed_pts / codec_ctx->time_base.den);

					video_input_file->prev_pts = pts;
					video_data_node->vframe->pts = video_input_file->computed_pts;
					video_input_file->computed_pts += video_input_data->frame_duration;
				}

				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DashCast] Video Frame TS "LLU" decoded at UTC "LLU" ms\n", video_data_node->vframe->pts, gf_net_get_utc() ));
				// For a decode/encode process we must free this memory.
				//But if the input is raw and there is no need to decode then
				// the packet is directly passed for decoded frame. So freeing it cause problem.
				if (codec_ctx->codec->id == CODEC_ID_RAWVIDEO) {
					video_data_node->is_raw_data = 1;
#ifdef GPAC_USE_LIBAV
					//we don't have ref count in libav, store packet (contains raw video) and destroy it later
					video_data_node->raw_packet = packet;
					dc_producer_advance(&video_input_data->producer);
					return 0;
#else
					video_data_node->vframe = av_frame_clone(video_data_node->vframe);
#endif
				}
				av_free_packet(&packet);
				dc_producer_advance(&video_input_data->producer);
				return 0;
			}
		}

		/* Free the packet that was allocated by av_read_frame */
		av_free_packet(&packet);
	}

	fprintf(stderr, "Unknown error while reading video frame.\n");
	return -1;
}

void dc_video_decoder_close(VideoInputFile *video_input_file)
{
	/* Close the video format context */
	avformat_close_input(&video_input_file->fmt_ctx);
}
