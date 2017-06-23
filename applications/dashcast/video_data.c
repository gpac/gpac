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

#include "video_data.h"


void dc_video_data_set_default(VideoDataConf *video_data_conf)
{
	memset(video_data_conf, 0, sizeof(VideoDataConf));
	video_data_conf->bitrate = -1;
	video_data_conf->framerate = -1;
	video_data_conf->crop_x = 0;
	video_data_conf->crop_y = 0;
	video_data_conf->height = -1;
	video_data_conf->width = -1;
}

void dc_video_input_data_end_signal(VideoInputData *video_input_data)
{
	dc_producer_end_signal(&video_input_data->producer, &video_input_data->circular_buf);
	dc_producer_end_signal_previous(&video_input_data->producer, &video_input_data->circular_buf);
}

int dc_video_input_data_init(VideoInputData *video_input_data, /*int width, int height, int pix_fmt*/ int num_consumers, int mode, int max_source, int video_cb_size)
{
	int i;

	dc_producer_init(&video_input_data->producer, video_cb_size, "video decoder");

	//video_input_data->width = width;
	//video_input_data->height = height;
	//video_input_data->pix_fmt = pix_fmt;

	video_input_data->vprop = (VideoInputProp*)gf_malloc(max_source * sizeof(VideoInputProp));

	dc_circular_buffer_create(&video_input_data->circular_buf, video_cb_size, mode, num_consumers);

	for (i=0; i<video_cb_size; i++) {
		VideoDataNode *video_data_node;
		GF_SAFEALLOC(video_data_node, VideoDataNode);
		if (!video_data_node) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("Cannot allocate video input\n"));
			return -1;
		}
		video_input_data->circular_buf.list[i].data = (void *) video_data_node;
		video_data_node->vframe = FF_ALLOC_FRAME();
	}

	return 0;
}

void dc_video_input_data_set_prop(VideoInputData *video_input_data, int index, int width, int height, int crop_x, int crop_y, int pix_fmt, AVRational sar)
{
	video_input_data->vprop[index].width = width;
	video_input_data->vprop[index].height = height;
	video_input_data->vprop[index].crop_x = crop_x;
	video_input_data->vprop[index].crop_y = crop_y;
	video_input_data->vprop[index].pix_fmt = pix_fmt;
	video_input_data->vprop[index].sar = sar;
}

void dc_video_input_data_destroy(VideoInputData *video_input_data)
{
	int i;
	for (i=0; i<(int) video_input_data->circular_buf.size; i++) {
		if (video_input_data->circular_buf.list) {
			VideoDataNode *video_data_node = (VideoDataNode*)video_input_data->circular_buf.list[i].data;
			av_free(video_data_node->vframe);
			gf_free(video_data_node);
		}
	}

	dc_circular_buffer_destroy(&video_input_data->circular_buf);
	gf_free(video_input_data->vprop);
}
