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

int dc_audio_encoder_open(AudioOutputFile * p_aout, AudioData * p_adata) {

	int i_osize;

	//AVCodec * p_audio_codec;
	//AVStream * p_audio_stream;

	p_aout->p_fifo = av_fifo_alloc(2 * MAX_AUDIO_PACKET_SIZE);
	p_aout->p_aframe = avcodec_alloc_frame();
	p_aout->p_adata_buf = (uint8_t*) av_malloc(2 * MAX_AUDIO_PACKET_SIZE);

	p_aout->p_codec = avcodec_find_encoder_by_name("aac"/*p_adata->psz_codec*/);
	if (p_aout->p_codec == NULL) {
		fprintf(stderr, "Output audio codec not found\n");
		return -1;
	}

	p_aout->p_codec_ctx = avcodec_alloc_context3(p_aout->p_codec);

//	/* Create new audio stream */
//	p_audio_stream = avformat_new_stream(p_aout->p_fmt, p_audio_codec);
//	if (!p_audio_stream) {
//		fprintf(stderr, "Cannot create output video stream\n");
//		return -1;
//	}


	p_aout->p_codec_ctx->codec_id = p_aout->p_codec->id;
	p_aout->p_codec_ctx->codec_type = AVMEDIA_TYPE_AUDIO;
	p_aout->p_codec_ctx->bit_rate = p_adata->i_bitrate;
	p_aout->p_codec_ctx->sample_rate = p_adata->i_samplerate;
	p_aout->p_codec_ctx->channels = p_adata->i_channels;
	p_aout->p_codec_ctx->sample_fmt = AV_SAMPLE_FMT_S16;

//	if (p_aout->p_fmt->oformat->flags & AVFMT_GLOBALHEADER)
//		p_aout->p_codec_ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;


//	p_audio_stream->codec->codec_id = p_audio_codec->id;
//	p_audio_stream->codec->codec_type = AVMEDIA_TYPE_AUDIO;
//	p_audio_stream->codec->bit_rate = p_aout->p_adata->i_bitrate;
//	p_audio_stream->codec->sample_rate = p_aout->p_adata->i_samplerate;
//	p_audio_stream->codec->channels = p_aout->p_adata->i_channels;
//	p_audio_stream->codec->sample_fmt = AV_SAMPLE_FMT_S16;
//
//	/*
//	 p_audio_stream->codec->delay = 0;
//	 p_audio_stream->codec->thread_count = 1;
//	 p_audio_stream->codec->max_b_frames = 0;
//	 */
//
//	if (p_aout->p_fmt->oformat->flags & AVFMT_GLOBALHEADER)
//		p_audio_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;

	p_aout->i_astream_idx = 0;

	/* open the audio codec */
	if (avcodec_open2(p_aout->p_codec_ctx, p_aout->p_codec, NULL) < 0) {
		fprintf(stderr, "Cannot open output audio codec\n");
		return -1;
	}

	i_osize = av_get_bytes_per_sample(p_aout->p_codec_ctx->sample_fmt);

	p_aout->i_frame_bytes = p_aout->p_codec_ctx->frame_size * i_osize
			* p_aout->p_codec_ctx->channels;

	avcodec_get_frame_defaults(p_aout->p_aframe);

	p_aout->p_aframe->nb_samples =
			p_aout->i_frame_bytes
					/ (p_aout->p_codec_ctx->channels
							* av_get_bytes_per_sample(
									p_aout->p_codec_ctx->sample_fmt));

	if (avcodec_fill_audio_frame(p_aout->p_aframe,
			p_aout->p_codec_ctx->channels, p_aout->p_codec_ctx->sample_fmt,
			p_aout->p_adata_buf, p_aout->i_frame_bytes, 1) < 0) {
		fprintf(stderr, "Fill audio frame failed\n");
		return -1;
	}

	//p_aout->acc_samples = 0;
	p_aout->i_frame_size = p_aout->p_codec_ctx->frame_size;

	return 0;
}

int dc_audio_encoder_read(AudioOutputFile * p_aout, AudioInputData * p_aind) {

	int ret;
	AudioDataNode * p_adn;

	ret = dc_consumer_lock(&p_aout->acon, &p_aind->p_cb);

	if (ret < 0) {
#ifdef DEBUG
		printf("Audio encoder got to end of buffer!\n");
#endif

		return -2;
	}

	dc_consumer_unlock_previous(&p_aout->acon, &p_aind->p_cb);

	p_adn = (AudioDataNode *) dc_consumer_consume(&p_aout->acon, &p_aind->p_cb);

	/* Write audio sample on fifo */
//	av_fifo_generic_write(p_aout->p_fifo, p_adn->p_aframe->data[0],
//			p_adn->p_aframe->linesize[0], NULL);
	av_fifo_generic_write(p_aout->p_fifo, p_adn->p_abuf, p_adn->i_abuf_size,
			NULL);

	dc_consumer_advance(&p_aout->acon);

	return 0;
}

int dc_audio_encoder_flush(AudioOutputFile * p_aout, AudioInputData * p_aind) {

	int i_got_pkt;

	//AVStream * p_audio_stream = p_aout->p_fmt->streams[p_aout->i_astream_idx];
	//AVCodecContext * p_audio_codec_ctx = p_audio_stream->codec;

	AVCodecContext * p_audio_codec_ctx = p_aout->p_codec_ctx;

	av_init_packet(&p_aout->packet);
	/* Set PTS (method 1) */
	p_aout->p_aframe->pts = p_aind->next_pts;
	/* Encode audio */
	if (avcodec_encode_audio2(p_audio_codec_ctx, &p_aout->packet, NULL,
			&i_got_pkt) != 0) {
		fprintf(stderr, "Error while encoding audio.\n");
		return -1;
	}
	if (i_got_pkt) {
		//p_aout->acc_samples += p_aout->p_aframe->nb_samples;
		return 0;
	}
	av_free_packet(&p_aout->packet);
	return 1;
}

int dc_audio_encoder_encode(AudioOutputFile * p_aout, AudioInputData * p_aind) {

	int i_got_pkt;

	//AVStream * p_audio_stream = p_aout->p_fmt->streams[p_aout->i_astream_idx];
	//AVCodecContext * p_audio_codec_ctx = p_audio_stream->codec;

	AVCodecContext * p_audio_codec_ctx = p_aout->p_codec_ctx;

	while (av_fifo_size(p_aout->p_fifo) >= p_aout->i_frame_bytes) {

		av_fifo_generic_read(p_aout->p_fifo, p_aout->p_adata_buf,
				p_aout->i_frame_bytes, NULL);


		p_aout->p_aframe->data[0] = p_aout->p_adata_buf;
		p_aout->p_aframe->linesize[0] = p_aout->i_frame_bytes;

		av_init_packet(&p_aout->packet);

		/* 
		 * Set PTS (method 1)
		 */

		//p_aout->p_aframe->pts = p_aind->next_pts;

		/*
		 * Set PTS (method 2)
		 */
		// int64_t now = av_gettime();
		// p_aout->p_aframe->pts = av_rescale_q(now,AV_TIME_BASE_Q,
		//		 p_audio_codec_ctx->time_base);


		/* Encode audio */
		if (avcodec_encode_audio2(p_audio_codec_ctx, &p_aout->packet,
				p_aout->p_aframe, &i_got_pkt) != 0) {

			fprintf(stderr, "Error while encoding audio.\n");
			return -1;
		}

		if (i_got_pkt) {

			//p_aout->acc_samples += p_aout->p_aframe->nb_samples;
			return 0;

		}

		av_free_packet(&p_aout->packet);
	}

	return 1;
}

void dc_audio_encoder_close(AudioOutputFile * p_aoutf) {

//	int i;
//
//	/* free the streams */
//	for (i = 0; i < p_aout->p_fmt->nb_streams; i++) {
//		avcodec_close(p_aout->p_fmt->streams[i]->codec);
//		av_freep(&p_aout->p_fmt->streams[i]->info);
//	}

	av_fifo_free(p_aoutf->p_fifo);

	av_free(p_aoutf->p_adata_buf);
	av_free(p_aoutf->p_aframe);

	avcodec_close(p_aoutf->p_codec_ctx);
	av_free(p_aoutf->p_codec_ctx);

}

