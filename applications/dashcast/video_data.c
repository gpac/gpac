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

void dc_video_data_set_default(VideoData * vdata) {

	memset(vdata, 0, sizeof(VideoData));
	vdata->i_bitrate = -1;
	vdata->i_framerate = -1;
	vdata->i_height = -1;
	vdata->i_width = -1;
}


void dc_video_input_data_end_signal(VideoInputData * p_vconv) {

	dc_producer_end_signal(&p_vconv->pro, &p_vconv->p_cb);
	dc_producer_end_signal_previous(&p_vconv->pro, &p_vconv->p_cb);

}

int dc_video_input_data_init(VideoInputData * p_vin_data, /*int i_width, int i_height, int i_pix_fmt*/
		int i_con_nb, int mode, int max_source) {

	int i;

	dc_producer_init(&p_vin_data->pro, VIDEO_CB_SIZE, "video decoder");

	//p_vin_data->i_width = i_width;
	//p_vin_data->i_height = i_height;
	//p_vin_data->i_pix_fmt = i_pix_fmt;

	p_vin_data->p_vprop = gf_malloc(max_source * sizeof(VideoInputProp));

	dc_circular_buffer_create(&p_vin_data->p_cb, VIDEO_CB_SIZE, mode,
				i_con_nb);

	for (i = 0; i < VIDEO_CB_SIZE; i++) {
		VideoDataNode *p_vdn;
		GF_SAFEALLOC(p_vdn, VideoDataNode);
		p_vin_data->p_cb.p_list[i].p_data = (void *) p_vdn;
		p_vdn->p_vframe = avcodec_alloc_frame();
	}

	return 0;
}

void dc_video_input_data_set_prop(VideoInputData * p_vind, int index, int width, int height, int pix_fmt) {

	p_vind->p_vprop[index].i_width = width;
	p_vind->p_vprop[index].i_height = height;
	p_vind->p_vprop[index].i_pix_fmt = pix_fmt;
}


void dc_video_input_data_destroy(VideoInputData * p_vd) {

	int i;

	for (i = 0; i < VIDEO_CB_SIZE; i++) {
		VideoDataNode * p_vdn = p_vd->p_cb.p_list[i].p_data;
		av_free(p_vdn->p_vframe);
		gf_free(p_vdn);
	}

	dc_circular_buffer_destroy(&p_vd->p_cb);
	gf_free(p_vd->p_vprop);


}
