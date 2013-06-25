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

#include "audio_decoder.h"

int dc_audio_decoder_open(AudioInputFile * p_ain, AudioData * p_adata, int i_mode, int i_no_loop) {

	u32 i;
	AVInputFormat * p_in_fmt = NULL;
	AVCodecContext * p_codec_ctx;
	AVCodec * p_codec;

	if (p_adata->psz_format && strcmp(p_adata->psz_format,"") != 0) {

		p_in_fmt = av_find_input_format(p_adata->psz_format);
		if (p_in_fmt == NULL) {
			fprintf(stderr, "Cannot find the format %s.\n", p_adata->psz_format);
			return -1;
		}

	}

	p_ain->p_fmt = NULL;

	/*
	 * Open audio
	 */
	if (avformat_open_input(&p_ain->p_fmt, p_adata->psz_name, p_in_fmt, NULL) != 0) {
		fprintf(stderr, "Cannot open file: %s\n", p_adata->psz_name);
		return -1;
	}

	/*
	 * Retrieve stream information
	 */
	if (avformat_find_stream_info(p_ain->p_fmt, NULL) < 0) {
		fprintf(stderr, "Cannot find stream information\n");
		return -1;
	}

	av_dump_format(p_ain->p_fmt, 0, p_adata->psz_name, 0);

	/*
	 * Find the first audio stream
	 */
	p_ain->i_astream_idx = -1;
	for (i = 0; i < p_ain->p_fmt->nb_streams; i++) {
		if (p_ain->p_fmt->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
			p_ain->i_astream_idx = i;
			break;
		}
	}
	if (p_ain->i_astream_idx == -1) {
		fprintf(stderr, "Cannot find a audio stream\n");
		return -1;
	}

	/*
	 * Get a pointer to the codec context for the audio stream
	 */
	p_codec_ctx = p_ain->p_fmt->streams[p_ain->i_astream_idx]->codec;

	/*
	 * Find the decoder for the audio stream
	 */
	p_codec = avcodec_find_decoder(p_codec_ctx->codec_id);
	if (p_codec == NULL) {
		fprintf(stderr, "Input audio codec is not supported.\n");
		avformat_close_input(&p_ain->p_fmt);
		return -1;
	}

	/*
	 * Open codec
	 */
	if (avcodec_open2(p_codec_ctx, p_codec, NULL) < 0) {
		fprintf(stderr, "Cannot open input audio codec.\n");
		avformat_close_input(&p_ain->p_fmt);
		return -1;
	}

	p_ain->p_fifo = av_fifo_alloc(2 * MAX_AUDIO_PACKET_SIZE);

	p_adata->i_channels = p_codec_ctx->channels;
	p_adata->i_samplerate = p_codec_ctx->sample_rate;

	p_ain->i_mode = i_mode;
	p_ain->i_no_loop = i_no_loop;

	return 0;
}

int dc_audio_decoder_read(AudioInputFile * p_ain, AudioInputData * p_ad) {

	int ret;
	AVPacket packet;
	int i_got_frame;
	//int locked_already = 0;
	AVCodecContext * p_codec_ctx;
	AudioDataNode * p_adn;

	/* Get a pointer to the codec context for the audio stream */
	p_codec_ctx = p_ain->p_fmt->streams[p_ain->i_astream_idx]->codec;

	/* Read frames */
	while (1) {

		ret = av_read_frame(p_ain->p_fmt, &packet);

		if (ret == AVERROR_EOF) {

			if(p_ain->i_mode == LIVE_MEDIA && p_ain->i_no_loop == 0) {
				av_seek_frame(p_ain->p_fmt, p_ain->i_astream_idx, 0, 0);
				continue;
			}

			/* Flush decoder */
			packet.data = NULL;
			packet.size = 0;
			avcodec_get_frame_defaults(p_ad->p_aframe);
			avcodec_decode_video2(p_codec_ctx, p_ad->p_aframe,
					&i_got_frame, &packet);

			if (i_got_frame) {

				dc_producer_lock(&p_ad->pro, &p_ad->p_cb);
				dc_producer_unlock_previous(&p_ad->pro, &p_ad->p_cb);
				p_adn = (AudioDataNode *) dc_producer_produce(&p_ad->pro, &p_ad->p_cb);

				p_adn->i_abuf_size = p_ad->p_aframe->linesize[0];
				memcpy(p_adn->p_abuf, p_ad->p_aframe->data[0], p_adn->i_abuf_size);

				dc_producer_advance(&p_ad->pro);
				return 0;
			}

			dc_producer_end_signal(&p_ad->pro, &p_ad->p_cb);
			dc_producer_unlock_previous(&p_ad->pro, &p_ad->p_cb);
			return -2;
		}

		else if (ret < 0) {
			fprintf(stderr, "Cannot read audio frame.\n");
			continue;
		}

		/* Is this a packet from the audio stream? */
		if (packet.stream_index == p_ain->i_astream_idx) {

			/* Set audio frame to default */
			avcodec_get_frame_defaults(p_ad->p_aframe);

			/* Decode audio frame */
			if (avcodec_decode_audio4(p_codec_ctx, p_ad->p_aframe, &i_got_frame,
					&packet) < 0) {
				av_free_packet(&packet);
				fprintf(stderr, "Error while decoding audio.\n");
				dc_producer_end_signal(&p_ad->pro, &p_ad->p_cb);
				dc_producer_unlock_previous(&p_ad->pro, &p_ad->p_cb);
				return -1;
			}

			if (p_ad->p_aframe->pts != AV_NOPTS_VALUE)
				p_ad->next_pts = p_ad->p_aframe->pts;

			p_ad->next_pts += ((int64_t) AV_TIME_BASE
					* p_ad->p_aframe->nb_samples) / p_codec_ctx->sample_rate;

			/* Did we get a video frame? */
			if (i_got_frame) {

				av_fifo_generic_write(p_ain->p_fifo, p_ad->p_aframe->data[0],
						p_ad->p_aframe->linesize[0], NULL);

				if (/*p_ad->p_cb.mode == OFFLINE*/
						p_ain->i_mode == ON_DEMAND || p_ain->i_mode == LIVE_MEDIA) {

					dc_producer_lock(&p_ad->pro, &p_ad->p_cb);

					/* Unlock the previous node in the circular buffer. */
					dc_producer_unlock_previous(&p_ad->pro, &p_ad->p_cb);

					/* Get the pointer of the current node in circular buffer. */
					p_adn = (AudioDataNode *) dc_producer_produce(&p_ad->pro,
							&p_ad->p_cb);

					p_adn->i_abuf_size = p_ad->p_aframe->linesize[0];
					av_fifo_generic_read(p_ain->p_fifo, p_adn->p_abuf,
							p_adn->i_abuf_size , NULL);

					dc_producer_advance(&p_ad->pro);
				} else {

					while (av_fifo_size(p_ain->p_fifo) >= LIVE_FRAME_SIZE) {

						/* Lock the current node in the circular buffer. */
						if (dc_producer_lock(&p_ad->pro, &p_ad->p_cb) < 0) {
							printf(
									"[dashcast] Live system dropped an audio frame\n");
							continue;
						}

						/* Unlock the previous node in the circular buffer. */
						dc_producer_unlock_previous(&p_ad->pro, &p_ad->p_cb);

						/* Get the pointer of the current node in circular buffer. */
						p_adn = (AudioDataNode *) dc_producer_produce(
								&p_ad->pro, &p_ad->p_cb);

						p_adn->i_abuf_size = LIVE_FRAME_SIZE;
						av_fifo_generic_read(p_ain->p_fifo, p_adn->p_abuf,
								p_adn->i_abuf_size, NULL);

						dc_producer_advance(&p_ad->pro);
					}

				}

				return 0;

			}
		}

		/*
		 * Free the packet that was allocated by av_read_frame
		 */
		av_free_packet(&packet);

	}

	fprintf(stderr, "Unknown error while reading audio frame.\n");
	return -1;

}

void dc_audio_decoder_close(AudioInputFile * p_ain) {

	/*
	 * Close the audio format context
	 */
	avformat_close_input(&p_ain->p_fmt);

	av_fifo_free(p_ain->p_fifo);

}
