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

#include "audio_encoder.h"


extern void build_dict(void *priv_data, const char *options);


int dc_audio_encoder_open(AudioOutputFile *audio_output_file, AudioDataConf *audio_data_conf)
{
	int osize;

	//AVCodec *audio_codec;
	//AVStream *audio_stream;

	audio_output_file->fifo = av_fifo_alloc(2 * MAX_AUDIO_PACKET_SIZE);
	audio_output_file->aframe = avcodec_alloc_frame();
	audio_output_file->adata_buf = (uint8_t*) av_malloc(2 * MAX_AUDIO_PACKET_SIZE);

	audio_output_file->codec = avcodec_find_encoder_by_name("mp2"/*FIXME: audio_data_conf->codec - Note: windows build doesn't seem to have AAC*/);
	if (audio_output_file->codec == NULL) {
		fprintf(stderr, "Output audio codec not found\n");
		return -1;
	}

	audio_output_file->codec_ctx = avcodec_alloc_context3(audio_output_file->codec);

//	/* Create new audio stream */
//	audio_stream = avformat_new_stream(audio_output_file->fmt, audio_codec);
//	if (!audio_stream) {
//		fprintf(stderr, "Cannot create output video stream\n");
//		return -1;
//	}
	
	audio_output_file->codec_ctx->codec_id = audio_output_file->codec->id;
	audio_output_file->codec_ctx->codec_type = AVMEDIA_TYPE_AUDIO;
	audio_output_file->codec_ctx->bit_rate = audio_data_conf->bitrate;
	audio_output_file->codec_ctx->sample_rate = audio_data_conf->samplerate;
	{
		AVRational time_base;
		time_base.num = 1;
		time_base.den = audio_data_conf->samplerate;
		audio_output_file->codec_ctx->time_base = time_base;
	}
	audio_output_file->codec_ctx->channels = audio_data_conf->channels;
	audio_output_file->codec_ctx->channel_layout = AV_CH_LAYOUT_STEREO; /*FIXME: depends on channels -> http://ffmpeg.org/doxygen/trunk/channel__layout_8c_source.html#l00074*/
	audio_output_file->codec_ctx->sample_fmt = AV_SAMPLE_FMT_S16;

//	if (audio_output_file->fmt->oformat->flags & AVFMT_GLOBALHEADER)
//		audio_output_file->codec_ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;
	
//	audio_stream->codec->codec_id = audio_codec->id;
//	audio_stream->codec->codec_type = AVMEDIA_TYPE_AUDIO;
//	audio_stream->codec->bit_rate = audio_output_file->audio_data_conf->bitrate;
//	audio_stream->codec->sample_rate = audio_output_file->audio_data_conf->samplerate;
//	audio_stream->codec->channels = audio_output_file->audio_data_conf->channels;
//	audio_stream->codec->sample_fmt = AV_SAMPLE_FMT_S16;
//
//	/*
//	 audio_stream->codec->delay = 0;
//	 audio_stream->codec->thread_count = 1;
//	 audio_stream->codec->max_b_frames = 0;
//	 */

	if (audio_data_conf->custom) {
		build_dict(audio_output_file->codec_ctx->priv_data, audio_data_conf->custom);
		gf_free(audio_data_conf->custom);
		audio_data_conf->custom = NULL;
	}

//	if (audio_output_file->fmt->oformat->flags & AVFMT_GLOBALHEADER)
//		audio_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;

	audio_output_file->astream_idx = 0;

	/* open the audio codec */
	if (avcodec_open2(audio_output_file->codec_ctx, audio_output_file->codec, NULL) < 0) {
		/*FIXME: if we enter here (set "mp2" as a codec and "200000" as a bitrate -> deadlock*/
		fprintf(stderr, "Cannot open output audio codec\n");
		return -1;
	}

	osize = av_get_bytes_per_sample(audio_output_file->codec_ctx->sample_fmt);
	audio_output_file->frame_bytes = audio_output_file->codec_ctx->frame_size * osize * audio_output_file->codec_ctx->channels;
	avcodec_get_frame_defaults(audio_output_file->aframe);
	audio_output_file->aframe->nb_samples = audio_output_file->frame_bytes
					/ (audio_output_file->codec_ctx->channels * av_get_bytes_per_sample(audio_output_file->codec_ctx->sample_fmt));

	if (avcodec_fill_audio_frame(audio_output_file->aframe,
			audio_output_file->codec_ctx->channels, audio_output_file->codec_ctx->sample_fmt,
			audio_output_file->adata_buf, audio_output_file->frame_bytes, 1) < 0) {
		fprintf(stderr, "Fill audio frame failed\n");
		return -1;
	}

	//audio_output_file->acc_samples = 0;
	audio_output_file->frame_size = audio_output_file->codec_ctx->frame_size;

	return 0;
}

int dc_audio_encoder_read(AudioOutputFile *audio_output_file, AudioInputData *audio_input_data)
{
	int ret;
	AudioDataNode *audio_data_node;

	ret = dc_consumer_lock(&audio_output_file->consumer, &audio_input_data->circular_buf);
	if (ret < 0) {
#ifdef DEBUG
		fprintf(stderr, "Audio encoder got an end of buffer!\n");
#endif
		return -2;
	}

	dc_consumer_unlock_previous(&audio_output_file->consumer, &audio_input_data->circular_buf);

	audio_data_node = (AudioDataNode *) dc_consumer_consume(&audio_output_file->consumer, &audio_input_data->circular_buf);

	/* Write audio sample on fifo */
//	av_fifo_generic_write(audio_output_file->fifo, audio_data_node->aframe->data[0],
//			audio_data_node->aframe->linesize[0], NULL);
	av_fifo_generic_write(audio_output_file->fifo, audio_data_node->abuf, audio_data_node->abuf_size, NULL);

	dc_consumer_advance(&audio_output_file->consumer);

	return 0;
}

int dc_audio_encoder_flush(AudioOutputFile *audio_output_file, AudioInputData *audio_input_data)
{
	int got_pkt;
	//AVStream *audio_stream = audio_output_file->fmt->streams[audio_output_file->astream_idx];
	//AVCodecContext *audio_codec_ctx = audio_stream->codec;
	AVCodecContext *audio_codec_ctx = audio_output_file->codec_ctx;

	av_init_packet(&audio_output_file->packet);
	audio_output_file->packet.data = NULL;
	audio_output_file->packet.size = 0;

	/* Set PTS (method 1) */
	audio_output_file->aframe->pts = audio_input_data->next_pts;
	/* Encode audio */
	if (avcodec_encode_audio2(audio_codec_ctx, &audio_output_file->packet, NULL, &got_pkt) != 0) {
		fprintf(stderr, "Error while encoding audio.\n");
		return -1;
	}
	if (got_pkt) {
		//audio_output_file->acc_samples += audio_output_file->aframe->nb_samples;
		return 0;
	}
	av_free_packet(&audio_output_file->packet);
	return 1;
}

int dc_audio_encoder_encode(AudioOutputFile *audio_output_file, AudioInputData *audio_input_data)
{
	int got_pkt;
	//AVStream *audio_stream = audio_output_file->fmt->streams[audio_output_file->astream_idx];
	//AVCodecContext *audio_codec_ctx = audio_stream->codec;
	AVCodecContext *audio_codec_ctx = audio_output_file->codec_ctx;

	while (av_fifo_size(audio_output_file->fifo) >= audio_output_file->frame_bytes) {
		av_fifo_generic_read(audio_output_file->fifo, audio_output_file->adata_buf, audio_output_file->frame_bytes, NULL);

		audio_output_file->aframe->data[0] = audio_output_file->adata_buf;
		audio_output_file->aframe->linesize[0] = audio_output_file->frame_bytes;

		av_init_packet(&audio_output_file->packet);
		audio_output_file->packet.data = NULL;
		audio_output_file->packet.size = 0;

		/* 
		 * Set PTS (method 1)
		 */
		//audio_output_file->aframe->pts = audio_input_data->next_pts;

		/*
		 * Set PTS (method 2)
		 */
		//{
		//	int64_t now = av_gettime();
		//	AVRational avr;
		//	avr.num = 1;
		//	avr.den = AV_TIME_BASE;
		//	audio_output_file->aframe->pts = av_rescale_q(now, avr, audio_codec_ctx->time_base);
		//}

		/* Encode audio */
		if (avcodec_encode_audio2(audio_codec_ctx, &audio_output_file->packet, audio_output_file->aframe, &got_pkt) != 0) {
			fprintf(stderr, "Error while encoding audio.\n");
			return -1;
		}

		if (got_pkt) {
			//audio_output_file->acc_samples += audio_output_file->aframe->nb_samples;
			return 0;
		}

		av_free_packet(&audio_output_file->packet);
	}

	return 1;
}

void dc_audio_encoder_close(AudioOutputFile *audio_output_file)
{
//	int i;
//
//	/* free the streams */
//	for (i = 0; i < audio_output_file->fmt->nb_streams; i++) {
//		avcodec_close(audio_output_file->fmt->streams[i]->codec);
//		av_freep(&audio_output_file->fmt->streams[i]->info);
//	}

	av_fifo_free(audio_output_file->fifo);

	av_free(audio_output_file->adata_buf);
	av_free(audio_output_file->aframe);

	avcodec_close(audio_output_file->codec_ctx);
	av_free(audio_output_file->codec_ctx);
}
