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

#include "video_scaler.h"


#ifdef GPAC_USE_LIBAV
#define av_frame_free	av_free
#endif

VideoScaledDataNode * dc_video_scaler_node_create(int width, int height, int crop_x, int crop_y, int pix_fmt)
{
	VideoScaledDataNode *video_scaled_data_node = gf_malloc(sizeof(VideoDataNode));
	if (video_scaled_data_node) {
		video_scaled_data_node->v_frame = avcodec_alloc_frame();
		if (crop_x || crop_y) {
			video_scaled_data_node->cropped_frame = avcodec_alloc_frame();
		} else {
			video_scaled_data_node->cropped_frame = NULL;
		}
	}
	if (!video_scaled_data_node || !video_scaled_data_node->v_frame || ((crop_x || crop_y) && !video_scaled_data_node->cropped_frame)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot allocate VideoNode!\n"));
		av_frame_free(&video_scaled_data_node->v_frame);
		av_frame_free(&video_scaled_data_node->cropped_frame);
		gf_free(video_scaled_data_node);
		return NULL;
	}

	/* Determine required buffer size and allocate buffer */
	avpicture_alloc((AVPicture*)video_scaled_data_node->v_frame, pix_fmt, width, height);
	if (video_scaled_data_node->cropped_frame) {
		avpicture_alloc((AVPicture*)video_scaled_data_node->cropped_frame, pix_fmt, width-crop_x, height-crop_y);
	}

	return video_scaled_data_node;
}

void dc_video_scaler_node_destroy(VideoScaledDataNode *video_scaled_data_node)
{
	av_frame_free(&video_scaled_data_node->v_frame);
	gf_free(video_scaled_data_node);
}

void dc_video_scaler_list_init(VideoScaledDataList *video_scaled_data_list, GF_List * video_lst)
{
	u32 i, j;
	int found;

	video_scaled_data_list->size = 0;
	video_scaled_data_list->video_scaled_data = NULL;

	for (i=0; i<gf_list_count(video_lst); i++) {
		VideoDataConf *video_data_conf = (VideoDataConf*)gf_list_get(video_lst, i);
		found = 0;
		for (j=0; j<video_scaled_data_list->size; j++) {
			if (   video_scaled_data_list->video_scaled_data[j]->out_height == video_data_conf->height
				  && video_scaled_data_list->video_scaled_data[j]->out_width  == video_data_conf->width) {
				found = 1;
				video_scaled_data_list->video_scaled_data[j]->num_consumers++;
				break;
			}
		}
		if (!found) {
			VideoScaledData *video_scaled_data;
			GF_SAFEALLOC(video_scaled_data, VideoScaledData);
			video_scaled_data->out_width  = video_data_conf->width;
			video_scaled_data->out_height = video_data_conf->height;
			video_scaled_data->num_consumers = 1;

			video_scaled_data_list->video_scaled_data = gf_realloc(video_scaled_data_list->video_scaled_data, (video_scaled_data_list->size+1)*sizeof(VideoScaledData*));

			video_scaled_data_list->video_scaled_data[video_scaled_data_list->size] = video_scaled_data;
			video_scaled_data_list->size++;
		}
	}
}

void dc_video_scaler_list_destroy(VideoScaledDataList *video_scaled_data_list)
{
	u32 i;
	for (i=0; i<video_scaled_data_list->size; i++)
		gf_free(video_scaled_data_list->video_scaled_data[i]);

	gf_free(video_scaled_data_list->video_scaled_data);
}

void dc_video_scaler_end_signal(VideoScaledData *video_scaled_data)
{
	dc_producer_end_signal(&video_scaled_data->producer, &video_scaled_data->circular_buf);
	dc_producer_unlock_previous(&video_scaled_data->producer, &video_scaled_data->circular_buf);
}

int dc_video_scaler_data_init(VideoInputData *video_input_data, VideoScaledData *video_scaled_data, int max_source, int video_cb_size)
{
	int i;
	char name[GF_MAX_PATH];
	snprintf(name, sizeof(name), "video scaler %dx%d", video_scaled_data->out_width, video_scaled_data->out_height);

	dc_producer_init(&video_scaled_data->producer, video_cb_size, name);
	dc_consumer_init(&video_scaled_data->consumer, video_cb_size, name);

	video_scaled_data->num_producers = max_source;
	video_scaled_data->out_pix_fmt = PIX_FMT_YUV420P;
	GF_SAFE_ALLOC_N(video_scaled_data->vsprop, max_source, VideoScaledProp);
	memset(video_scaled_data->vsprop, 0, max_source * sizeof(VideoScaledProp));

	dc_circular_buffer_create(&video_scaled_data->circular_buf, video_cb_size, video_input_data->circular_buf.mode, video_scaled_data->num_consumers);
	for (i=0; i<video_cb_size; i++) {
		video_scaled_data->circular_buf.list[i].data = dc_video_scaler_node_create(video_scaled_data->out_width, video_scaled_data->out_height, video_input_data->vprop->crop_x, video_input_data->vprop->crop_y, video_scaled_data->out_pix_fmt);
	}

	return 0;
}

int dc_video_scaler_data_set_prop(VideoInputData *video_input_data, VideoScaledData *video_scaled_data, int index)
{
	video_scaled_data->vsprop[index].in_width   = video_input_data->vprop[index].width - video_input_data->vprop[index].crop_x;
	video_scaled_data->vsprop[index].in_height  = video_input_data->vprop[index].height - video_input_data->vprop[index].crop_y;
	video_scaled_data->vsprop[index].in_pix_fmt = video_input_data->vprop[index].pix_fmt;

	video_scaled_data->vsprop[index].sws_ctx = sws_getContext(
			video_scaled_data->vsprop[index].in_width,
			video_scaled_data->vsprop[index].in_height,
			video_scaled_data->vsprop[index].in_pix_fmt,
			video_scaled_data->out_width, video_scaled_data->out_height,
			video_scaled_data->out_pix_fmt, SWS_FAST_BILINEAR, NULL, NULL, NULL);
	if (video_scaled_data->vsprop[index].sws_ctx == NULL) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot initialize the conversion context!\n"));
		return -1;
	}

	return 0;
}

int dc_video_scaler_scale(VideoInputData *video_input_data, VideoScaledData *video_scaled_data)
{
	int ret, index, src_height;
	VideoDataNode *video_data_node;
	VideoScaledDataNode *video_scaled_data_node;
	AVFrame *src_vframe;

	ret = dc_consumer_lock(&video_scaled_data->consumer, &video_input_data->circular_buf);
	if (ret < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Video scaler got an end of buffer!\n"));
		return -2;
	}

	if (video_input_data->circular_buf.size > 1) 
		dc_consumer_unlock_previous(&video_scaled_data->consumer, &video_input_data->circular_buf);

	dc_producer_lock(&video_scaled_data->producer, &video_scaled_data->circular_buf);
	dc_producer_unlock_previous(&video_scaled_data->producer, &video_scaled_data->circular_buf);

	video_data_node = (VideoDataNode*)dc_consumer_consume(&video_scaled_data->consumer, &video_input_data->circular_buf);
	video_scaled_data_node = (VideoScaledDataNode*)dc_producer_produce(&video_scaled_data->producer, &video_scaled_data->circular_buf);
	index = video_data_node->source_number;

	video_scaled_data->frame_duration = video_input_data->frame_duration;

	//crop if necessary
	if (video_input_data->vprop[index].crop_x || video_input_data->vprop[index].crop_y) {
#if 0
		av_frame_copy_props(video_scaled_data_node->cropped_frame, video_data_node->vframe);
		video_scaled_data_node->cropped_frame->width  = video_input_data->vprop[index].width  - video_input_data->vprop[index].crop_x;
		video_scaled_data_node->cropped_frame->height = video_input_data->vprop[index].height - video_input_data->vprop[index].crop_y;
#endif
		if (av_picture_crop((AVPicture*)video_scaled_data_node->cropped_frame, (AVPicture*)video_data_node->vframe, PIX_FMT_YUV420P, video_input_data->vprop[index].crop_y, video_input_data->vprop[index].crop_x) < 0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Video scaler: error while cropping picture.\n"));
			return -1;
		}
		src_vframe = video_scaled_data_node->cropped_frame;
		src_height = video_input_data->vprop[index].height - video_input_data->vprop[index].crop_y;
	} else {
		assert(!video_scaled_data_node->cropped_frame);
		src_vframe = video_data_node->vframe;
		src_height = video_input_data->vprop[index].height;
	}

	//rescale the cropped frame
	sws_scale(video_scaled_data->vsprop[index].sws_ctx,
			(const uint8_t * const *)src_vframe->data, src_vframe->linesize, 0, src_height,
			video_scaled_data_node->v_frame->data, video_scaled_data_node->v_frame->linesize);
	
	video_scaled_data_node->v_frame->pts = video_data_node->vframe->pts;

	if (video_data_node->nb_raw_frames_ref) {
		if (video_data_node->nb_raw_frames_ref==1) {
#ifndef GPAC_USE_LIBAV
			av_frame_unref(video_data_node->vframe);
#endif
			av_free_packet(&video_data_node->raw_packet);
		}
		video_data_node->nb_raw_frames_ref--;
	}

	dc_consumer_advance(&video_scaled_data->consumer);
	dc_producer_advance(&video_scaled_data->producer, &video_scaled_data->circular_buf);

	if (video_input_data->circular_buf.size == 1) 
		dc_consumer_unlock_previous(&video_scaled_data->consumer, &video_input_data->circular_buf);
	return 0;
}

int dc_video_scaler_data_destroy(VideoScaledData *video_scaled_data)
{
	int i;
	for (i=0; i<(int) video_scaled_data->circular_buf.size; i++) {
		if (video_scaled_data->circular_buf.list) {
			dc_video_scaler_node_destroy(video_scaled_data->circular_buf.list[i].data);
		}
	}

	for (i=0 ; i<video_scaled_data->num_producers; i++) {
		if (video_scaled_data->vsprop[i].sws_ctx)
			av_free(video_scaled_data->vsprop[i].sws_ctx);
	}
	gf_free(video_scaled_data->vsprop);
	//av_free(video_scaled_data->sws_ctx);

	dc_circular_buffer_destroy(&video_scaled_data->circular_buf);

	return 0;
}

VideoScaledData * dc_video_scaler_get_data(VideoScaledDataList *video_scaled_data_list, int width, int height)
{
	u32 i;
	for (i=0; i<video_scaled_data_list->size; i++) {
		if (video_scaled_data_list->video_scaled_data[i]->out_width == width && video_scaled_data_list->video_scaled_data[i]->out_height == height)
			return video_scaled_data_list->video_scaled_data[i];
	}

	return NULL;
}
