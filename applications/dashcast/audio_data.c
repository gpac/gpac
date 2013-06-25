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

#include "audio_data.h"

void dc_audio_data_set_default(AudioData * adata) {

	strcpy(adata->psz_name, "");
	strcpy(adata->psz_format, "");
	strcpy(adata->psz_codec, "");
	adata->i_bitrate = -1;
	adata->i_channels= -1;
	adata->i_samplerate = -1;

}

void dc_audio_data_init(AudioData * adata, char * psz_name, char * psz_format) {

	if(psz_name != NULL && strlen(psz_name) > 0)
		strcpy(adata->psz_name, psz_name);
	else
		strcpy(adata->psz_name, "");

	if(psz_format != NULL && strlen(psz_format) > 0)
		strcpy(adata->psz_format, psz_format);
	else
		strcpy(adata->psz_format, "");
}

int dc_audio_input_data_init(AudioInputData * p_aid, int i_channels, int i_samplerate,
		int i_maxcon, int mode) {

	int i;

	dc_producer_init(&p_aid->pro, AUDIO_CB_SIZE, "audio decoder");


	dc_circular_buffer_create(&p_aid->p_cb, AUDIO_CB_SIZE, mode,
			i_maxcon);

	for (i = 0; i < AUDIO_CB_SIZE; i++) {
		AudioDataNode * p_adn = gf_malloc(sizeof(AudioDataNode));
		p_aid->p_cb.p_list[i].p_data = (void *) p_adn;

		p_adn->i_abuf_size = MAX_AUDIO_PACKET_SIZE;
		p_adn->p_abuf = gf_malloc(p_adn->i_abuf_size * sizeof(uint8_t));

	}

	p_aid->p_aframe = avcodec_alloc_frame();

	if(p_aid->p_aframe == NULL) {
		fprintf(stderr, "Cannot initialize AudioInputData");
		return -1;
	}

	p_aid->i_channels = i_channels;
	p_aid->i_samplerate = i_samplerate;

	return 0;
}

void dc_audio_input_data_destroy(AudioInputData * p_aid) {

	int i;

	for (i = 0; i < AUDIO_CB_SIZE; i++) {
		AudioDataNode * p_adn = p_aid->p_cb.p_list[i].p_data;
		av_free(p_adn->p_abuf);
		gf_free(p_adn);
	}

	dc_circular_buffer_destroy(&p_aid->p_cb);

}

void dc_audio_inout_data_end_signal(AudioInputData * p_aid) {

	dc_producer_end_signal(&p_aid->pro, &p_aid->p_cb);
	dc_producer_end_signal_previous(&p_aid->pro, &p_aid->p_cb);

}
