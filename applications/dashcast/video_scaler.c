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

VideoScaledDataNode * dc_video_scaler_node_create(int i_width, int i_height, int i_pix_fmt) {

	int num_bytes;
	VideoScaledDataNode * p_vsdn;
	p_vsdn = malloc(sizeof(VideoDataNode));

	p_vsdn->p_vframe = avcodec_alloc_frame();

	if (p_vsdn->p_vframe == NULL) {
		fprintf(stderr, "Cannot allocate VideoNode!\n");
		return NULL;
	}

	/* Determine required buffer size and allocate buffer */
	num_bytes = avpicture_get_size(i_pix_fmt, i_width, i_height);

	p_vsdn->p_pic_data_buf = (uint8_t *) av_malloc(num_bytes * sizeof(uint8_t));

	avpicture_fill((AVPicture *) p_vsdn->p_vframe, p_vsdn->p_pic_data_buf, i_pix_fmt, i_width,
			i_height);

	return p_vsdn;
}

void dc_video_scaler_node_destroy(VideoScaledDataNode * p_vsdn) {

	av_free(p_vsdn->p_vframe);
	av_free(p_vsdn->p_pic_data_buf);

	free(p_vsdn);

}


void dc_video_scaler_list_init(
		VideoScaledDataList * p_vsdl,
		GF_List * p_video_lst) {

	int i, j;

	int found;
	p_vsdl->i_size = 0;
	p_vsdl->p_vsd = NULL;

	for (i = 0; i < gf_list_count(p_video_lst) ; i++) {

		found = 0;
		VideoData * p_vconf = gf_list_get(p_video_lst, i);
		for (j = 0; j < p_vsdl->i_size; j++) {
			if (p_vsdl->p_vsd[j]->i_height
					== p_vconf->i_height
					&& p_vsdl->p_vsd[j]->i_width
							== p_vconf->i_width) {

				found = 1;

				p_vsdl->p_vsd[j]->i_maxcon++;

				break;
			}
		}
		if (!found) {

			VideoScaledData * p_vsd = malloc(sizeof(VideoScaledData));
			p_vsd->i_width = p_vconf->i_width;
			p_vsd->i_height = p_vconf->i_height;

			p_vsd->i_maxcon = 1;

			if (p_vsdl->p_vsd == NULL) {
				p_vsdl->p_vsd = malloc(sizeof(VideoScaledData *));
			} else {
				p_vsdl->p_vsd = realloc(p_vsdl->p_vsd,
						(p_vsdl->i_size + 1) * sizeof(VideoScaledData *));
			}

			p_vsdl->p_vsd[p_vsdl->i_size] = p_vsd;

			p_vsdl->i_size++;
		}
	}

}

void dc_video_scaler_list_destroy(VideoScaledDataList * p_vsdl) {
	int i;
	for (i = 0 ; i<p_vsdl->i_size ; i++)
		free(p_vsdl->p_vsd[i]);

	free(p_vsdl->p_vsd);
}

void dc_video_scaler_end_signal(VideoScaledData * p_vconv) {

	dc_producer_end_signal(&p_vconv->svpro, &p_vconv->p_cb);
	dc_producer_unlock_previous(&p_vconv->svpro, &p_vconv->p_cb);
}

int dc_video_scaler_data_init(VideoInputData * p_vin,
		VideoScaledData * p_vsd) {

	int i;

	char name[256];
	sprintf(name, "video scaler %dx%d", p_vsd->i_width, p_vsd->i_height);
	dc_producer_init(&p_vsd->svpro, VIDEO_CB_SIZE, name);
	dc_consumer_init(&p_vsd->svcon, VIDEO_CB_SIZE, name);

	p_vsd->i_pix_fmt = PIX_FMT_YUV420P;

	p_vsd->p_sws_ctx = sws_getContext(p_vin->i_width,
			p_vin->i_height, p_vin->i_pix_fmt,
				p_vsd->i_width, p_vsd->i_height,
				p_vsd->i_pix_fmt, SWS_FAST_BILINEAR, NULL, NULL, NULL);

	if (p_vsd->p_sws_ctx == NULL) {
		fprintf(stderr, "Cannot initialize the conversion context!\n");
		return -1;
	}

	dc_circular_buffer_create(&p_vsd->p_cb, VIDEO_CB_SIZE, p_vin->p_cb.mode,
			p_vsd->i_maxcon);


	for (i = 0; i < VIDEO_CB_SIZE; i++) {
		p_vsd->p_cb.p_list[i].p_data = dc_video_scaler_node_create(
				p_vsd->i_width ,p_vsd->i_height, p_vsd->i_pix_fmt);
	}

	return 0;
}

int dc_video_scaler_scale(VideoInputData * p_vin, VideoScaledData * p_vsd) {

	int ret;
	VideoDataNode * p_vdn;
	VideoScaledDataNode * p_vsdn;

	ret = dc_consumer_lock(&p_vsd->svcon, &p_vin->p_cb);

	if (ret < 0) {
#ifdef DEBUG
		printf("Video scaler got to end of buffer!\n");
#endif
		return -2;
	}

	dc_consumer_unlock_previous(&p_vsd->svcon, &p_vin->p_cb);

	dc_producer_lock(&p_vsd->svpro, &p_vsd->p_cb);
	dc_producer_unlock_previous(&p_vsd->svpro, &p_vsd->p_cb);

	p_vdn = (VideoDataNode *) dc_consumer_consume(&p_vsd->svcon, &p_vin->p_cb);
	p_vsdn = (VideoScaledDataNode *) dc_producer_produce(&p_vsd->svpro, &p_vsd->p_cb);

	sws_scale(p_vsd->p_sws_ctx,
			(const uint8_t * const *) p_vdn->p_vframe->data,
			p_vdn->p_vframe->linesize, 0,
			p_vin->i_height, p_vsdn->p_vframe->data,
			p_vsdn->p_vframe->linesize);

	dc_consumer_advance(&p_vsd->svcon);
	dc_producer_advance(&p_vsd->svpro);

	return 0;
}



int dc_video_scaler_data_destroy(VideoScaledData * p_vsd) {

	int i;

	for (i = 0; i < VIDEO_CB_SIZE; i++) {
		dc_video_scaler_node_destroy(p_vsd->p_cb.p_list[i].p_data);
	}

	av_free(p_vsd->p_sws_ctx);

	dc_circular_buffer_destroy(&p_vsd->p_cb);


	return 0;
}

VideoScaledData * dc_video_scaler_get_data(VideoScaledDataList * p_vsdl, int i_width, int i_height) {
	int i;

	for (i = 0 ; i< p_vsdl->i_size ; i++) {
		if (p_vsdl->p_vsd[i]->i_width == i_width &&
				p_vsdl->p_vsd[i]->i_height == i_height)
			return p_vsdl->p_vsd[i];
	}

	return NULL;
}


