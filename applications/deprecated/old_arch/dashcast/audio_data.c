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


void dc_audio_data_set_default(AudioDataConf *audio_data_conf)
{
	strcpy(audio_data_conf->filename, "");
	strcpy(audio_data_conf->format, "");
	strcpy(audio_data_conf->codec, "");
	audio_data_conf->bitrate = -1;
	audio_data_conf->channels= -1;
	audio_data_conf->samplerate = -1;
}

void dc_audio_data_init(AudioDataConf *audio_data_conf, char *filename, char *format)
{
	if (filename != NULL && strlen(filename) > 0)
		strcpy(audio_data_conf->filename, filename);
	else
		strcpy(audio_data_conf->filename, "");

	if (format != NULL && strlen(format) > 0)
		strcpy(audio_data_conf->format, format);
	else
		strcpy(audio_data_conf->format, "");
}

int dc_audio_input_data_init(AudioInputData *audio_input_data, int channels, int samplerate, int num_consumers, int mode)
{
	int i;

	dc_producer_init(&audio_input_data->producer, AUDIO_CB_SIZE, "audio decoder");
	dc_circular_buffer_create(&audio_input_data->circular_buf, AUDIO_CB_SIZE, mode, num_consumers);

	for (i = 0; i < AUDIO_CB_SIZE; i++) {
		AudioDataNode *audio_data_node = (AudioDataNode*)gf_malloc(sizeof(AudioDataNode));
		audio_input_data->circular_buf.list[i].data = (void *) audio_data_node;

		audio_data_node->abuf_size = MAX_AUDIO_PACKET_SIZE;
		audio_data_node->abuf = (uint8_t*)gf_malloc(audio_data_node->abuf_size * sizeof(uint8_t));
	}

	audio_input_data->aframe = FF_ALLOC_FRAME();
	if (audio_input_data->aframe == NULL) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot initialize AudioInputData"));
		return -1;
	}

	audio_input_data->channels = channels;
	audio_input_data->samplerate = samplerate;

	return 0;
}

void dc_audio_input_data_destroy(AudioInputData *audio_input_data)
{
	int i;
	if (audio_input_data->circular_buf.list) {
		for (i = 0; i < AUDIO_CB_SIZE; i++) {
			AudioDataNode *audio_data_node = (AudioDataNode*)audio_input_data->circular_buf.list[i].data;
			gf_free(audio_data_node->abuf);
			gf_free(audio_data_node);
		}
	}

	dc_circular_buffer_destroy(&audio_input_data->circular_buf);
}

void dc_audio_inout_data_end_signal(AudioInputData *audio_input_data)
{
	dc_producer_end_signal(&audio_input_data->producer, &audio_input_data->circular_buf);
	dc_producer_end_signal_previous(&audio_input_data->producer, &audio_input_data->circular_buf);
}
