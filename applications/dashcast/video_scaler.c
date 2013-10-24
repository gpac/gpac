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


VideoScaledDataNode * dc_video_scaler_node_create(int width, int height, int pix_fmt)
{
	int num_bytes;
	VideoScaledDataNode *video_scaled_data_node = gf_malloc(sizeof(VideoDataNode));
	if (video_scaled_data_node)
		video_scaled_data_node->vframe = avcodec_alloc_frame();
	if (!video_scaled_data_node || !video_scaled_data_node->vframe) {
		fprintf(stderr, "Cannot allocate VideoNode!\n");
		gf_free(video_scaled_data_node);
		return NULL;
	}

	/* Determine required buffer size and allocate buffer */
	num_bytes = avpicture_get_size(pix_fmt, width, height);
	video_scaled_data_node->pic_data_buf = (uint8_t *) av_malloc(num_bytes * sizeof(uint8_t));
	avpicture_fill((AVPicture *) video_scaled_data_node->vframe, video_scaled_data_node->pic_data_buf, pix_fmt, width, height);

	return video_scaled_data_node;
}

void dc_video_scaler_node_destroy(VideoScaledDataNode *video_scaled_data_node)
{
	av_free(video_scaled_data_node->vframe);
	av_free(video_scaled_data_node->pic_data_buf);

	gf_free(video_scaled_data_node);
}

void dc_video_scaler_list_init(VideoScaledDataList *video_scaled_data_list, GF_List * video_lst)
{
	u32 i;
	int j, found;

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

			if (video_scaled_data_list->video_scaled_data == NULL) {
				video_scaled_data_list->video_scaled_data = gf_malloc(sizeof(VideoScaledData *));
			} else {
				video_scaled_data_list->video_scaled_data = realloc(video_scaled_data_list->video_scaled_data, (video_scaled_data_list->size+1)*sizeof(VideoScaledData*));
			}

			video_scaled_data_list->video_scaled_data[video_scaled_data_list->size] = video_scaled_data;
			video_scaled_data_list->size++;
		}
	}
}

void dc_video_scaler_list_destroy(VideoScaledDataList *video_scaled_data_list)
{
	int i;
	for (i = 0; i < video_scaled_data_list->size; i++)
		gf_free(video_scaled_data_list->video_scaled_data[i]);

	gf_free(video_scaled_data_list->video_scaled_data);
}

void dc_video_scaler_end_signal(VideoScaledData *video_scaled_data)
{
	dc_producer_end_signal(&video_scaled_data->producer, &video_scaled_data->circular_buf);
	dc_producer_unlock_previous(&video_scaled_data->producer, &video_scaled_data->circular_buf);
}

int dc_video_scaler_data_init(VideoInputData *video_input_data, VideoScaledData *video_scaled_data, int max_source)
{
	int i;
	char name[256];
	sprintf(name, "video scaler %dx%d", video_scaled_data->out_width, video_scaled_data->out_height);

	dc_producer_init(&video_scaled_data->producer, VIDEO_CB_SIZE, name);
	dc_consumer_init(&video_scaled_data->consumer, VIDEO_CB_SIZE, name);

	video_scaled_data->num_producers = max_source;
	video_scaled_data->out_pix_fmt = PIX_FMT_YUV420P;
	GF_SAFE_ALLOC_N(video_scaled_data->vsprop, max_source, VideoScaledProp);
	memset(video_scaled_data->vsprop, 0, max_source * sizeof(VideoScaledProp));

	dc_circular_buffer_create(&video_scaled_data->circular_buf, VIDEO_CB_SIZE, video_input_data->circular_buf.mode, video_scaled_data->num_consumers);
	for (i=0; i<VIDEO_CB_SIZE; i++) {
		video_scaled_data->circular_buf.list[i].data = dc_video_scaler_node_create(video_scaled_data->out_width, video_scaled_data->out_height, video_scaled_data->out_pix_fmt);
	}

	return 0;
}

int dc_video_scaler_data_set_prop(VideoInputData *video_input_data, VideoScaledData *video_scaled_data, int index)
{
	video_scaled_data->vsprop[index].in_width   = video_input_data->vprop[index].width;
	video_scaled_data->vsprop[index].in_height  = video_input_data->vprop[index].height;
	video_scaled_data->vsprop[index].in_pix_fmt = video_input_data->vprop[index].pix_fmt;

	video_scaled_data->vsprop[index].sws_ctx = sws_getContext(
			video_scaled_data->vsprop[index].in_width,
			video_scaled_data->vsprop[index].in_height,
			video_scaled_data->vsprop[index].in_pix_fmt,
			video_scaled_data->out_width, video_scaled_data->out_height,
			video_scaled_data->out_pix_fmt, SWS_FAST_BILINEAR, NULL, NULL, NULL);
	if (video_scaled_data->vsprop[index].sws_ctx == NULL) {
		fprintf(stderr, "Cannot initialize the conversion context!\n");
		return -1;
	}

	return 0;
}

int dc_video_scaler_scale(VideoInputData *video_input_data, VideoScaledData *video_scaled_data)
{
	int ret, index;
	VideoDataNode *video_data_node;
	VideoScaledDataNode *video_scaled_data_node;

	ret = dc_consumer_lock(&video_scaled_data->consumer, &video_input_data->circular_buf);

	if (ret < 0) {
#ifdef DEBUG
		fprintf(stderr, "Video scaler got an end of buffer!\n");
#endif
		return -2;
	}

	dc_consumer_unlock_previous(&video_scaled_data->consumer, &video_input_data->circular_buf);

	dc_producer_lock(&video_scaled_data->producer, &video_scaled_data->circular_buf);
	dc_producer_unlock_previous(&video_scaled_data->producer, &video_scaled_data->circular_buf);

	video_data_node = (VideoDataNode*)dc_consumer_consume(&video_scaled_data->consumer, &video_input_data->circular_buf);
	video_scaled_data_node = (VideoScaledDataNode*)dc_producer_produce(&video_scaled_data->producer, &video_scaled_data->circular_buf);
	index = video_data_node->source_number;

	video_scaled_data->frame_duration = video_input_data->frame_duration;

	sws_scale(video_scaled_data->vsprop[index].sws_ctx,
			(const uint8_t * const *) video_data_node->vframe->data,
			video_data_node->vframe->linesize, 0,
			video_input_data->vprop[index].height/*video_input_data->height*/,
			video_scaled_data_node->vframe->data, video_scaled_data_node->vframe->linesize);
	
	video_scaled_data_node->vframe->pts = video_data_node->vframe->pts;

	if (video_data_node->is_raw_data) {
#ifdef GPAC_USE_LIBAV
		av_free_packet(&video_data_node->raw_packet);
#else
		av_frame_unref(video_data_node->vframe);
#endif
		video_data_node->is_raw_data = 0;
	}

	dc_consumer_advance(&video_scaled_data->consumer);
	dc_producer_advance(&video_scaled_data->producer);

	return 0;
}

int dc_video_scaler_data_destroy(VideoScaledData *video_scaled_data)
{
	int i;
	for (i=0; i<VIDEO_CB_SIZE; i++) {
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
	int i;
	for (i=0; i<video_scaled_data_list->size; i++) {
		if (video_scaled_data_list->video_scaled_data[i]->out_width == width && video_scaled_data_list->video_scaled_data[i]->out_height == height)
			return video_scaled_data_list->video_scaled_data[i];
	}

	return NULL;
}
