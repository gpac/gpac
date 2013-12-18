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

int dc_audio_decoder_open(AudioInputFile *audio_input_file, AudioDataConf *audio_data_conf, int mode, int no_loop)
{
	u32 i;
	AVCodecContext *codec_ctx;
	AVCodec *codec;
	AVInputFormat *in_fmt = NULL;

	if (audio_data_conf->format && strcmp(audio_data_conf->format,"") != 0) {
		in_fmt = av_find_input_format(audio_data_conf->format);
		if (in_fmt == NULL) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot find the format %s.\n", audio_data_conf->format));
			return -1;
		}
	}

	/*
	 * Open audio (may already be opened when shared with the video input).
	 */
	if (!audio_input_file->av_fmt_ctx) {
		if (avformat_open_input(&audio_input_file->av_fmt_ctx, audio_data_conf->filename, in_fmt, NULL) != 0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot open file: %s\n", audio_data_conf->filename));
			return -1;
		}

		/*
		* Retrieve stream information
		*/
		if (avformat_find_stream_info(audio_input_file->av_fmt_ctx, NULL) < 0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot find stream information\n"));
			return -1;
		}

		av_dump_format(audio_input_file->av_fmt_ctx, 0, audio_data_conf->filename, 0);
	}

	/*
	 * Find the first audio stream
	 */
	audio_input_file->astream_idx = -1;
	for (i=0; i<audio_input_file->av_fmt_ctx->nb_streams; i++) {
		if (audio_input_file->av_fmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
			audio_input_file->astream_idx = i;
			break;
		}
	}
	if (audio_input_file->astream_idx == -1) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot find a audio stream\n"));
		return -1;
	}

	/*
	 * Get a pointer to the codec context for the audio stream
	 */
	codec_ctx = audio_input_file->av_fmt_ctx->streams[audio_input_file->astream_idx]->codec;

	/*
	 * Find the decoder for the audio stream
	 */
	codec = avcodec_find_decoder(codec_ctx->codec_id);
	if (codec == NULL) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Input audio codec is not supported.\n"));
		avformat_close_input(&audio_input_file->av_fmt_ctx);
		return -1;
	}

	/*
	 * Open codec
	 */
	if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot open input audio codec.\n"));
		avformat_close_input(&audio_input_file->av_fmt_ctx);
		return -1;
	}

	audio_input_file->fifo = av_fifo_alloc(2 * MAX_AUDIO_PACKET_SIZE);

	audio_data_conf->channels = codec_ctx->channels;
	audio_data_conf->samplerate = codec_ctx->sample_rate;

	audio_input_file->mode = mode;
	audio_input_file->no_loop = no_loop;

	return 0;
}

int dc_audio_decoder_read(AudioInputFile *audio_input_file, AudioInputData *audio_input_data)
{
	int ret;
	AVPacket packet;
	int got_frame = 0;
	//int locked_already = 0;
	AVCodecContext *codec_ctx;
	AudioDataNode *audio_data_node;

	/* Get a pointer to the codec context for the audio stream */
	codec_ctx = audio_input_file->av_fmt_ctx->streams[audio_input_file->astream_idx]->codec;

	/* Read frames */
	while (1) {
		if (audio_input_file->av_pkt_list) {
			if (gf_list_count(audio_input_file->av_pkt_list)) {
				AVPacket *packet_copy;
				assert(audio_input_file->av_pkt_list);
				gf_mx_p(audio_input_file->av_pkt_list_mutex);
				packet_copy = gf_list_pop_front(audio_input_file->av_pkt_list);
				gf_mx_v(audio_input_file->av_pkt_list_mutex);

				if (packet_copy == NULL) {
					ret = AVERROR_EOF;
				} else {
					memcpy(&packet, packet_copy, sizeof(AVPacket));
					gf_free(packet_copy);
					ret = 0;
				}
			} else {
				gf_sleep(1);
				continue;
			}
		} else {
			ret = av_read_frame(audio_input_file->av_fmt_ctx, &packet);
		}
		if (ret == AVERROR_EOF) {
			if (audio_input_file->mode == LIVE_MEDIA && audio_input_file->no_loop == 0) {
				av_seek_frame(audio_input_file->av_fmt_ctx, audio_input_file->astream_idx, 0, 0);
				continue;
			}

			/* Flush decoder */
			packet.data = NULL;
			packet.size = 0;
			avcodec_get_frame_defaults(audio_input_data->aframe);
			avcodec_decode_audio4(codec_ctx, audio_input_data->aframe, &got_frame, &packet);

			if (got_frame) {
				dc_producer_lock(&audio_input_data->producer, &audio_input_data->circular_buf);
				dc_producer_unlock_previous(&audio_input_data->producer, &audio_input_data->circular_buf);
				audio_data_node = (AudioDataNode*)dc_producer_produce(&audio_input_data->producer, &audio_input_data->circular_buf);
				
				audio_data_node->abuf_size = audio_input_data->aframe->linesize[0];
				memcpy(audio_data_node->abuf, audio_input_data->aframe->data[0], audio_data_node->abuf_size);

				dc_producer_advance(&audio_input_data->producer, &audio_input_data->circular_buf);
				return 0;
			}

			dc_producer_end_signal(&audio_input_data->producer, &audio_input_data->circular_buf);
			dc_producer_unlock_previous(&audio_input_data->producer, &audio_input_data->circular_buf);

			return -2;
		}
		else if (ret < 0)
		{
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot read audio frame.\n"));
			continue;
		}

		/* Is this a packet from the audio stream? */
		if (packet.stream_index == audio_input_file->astream_idx) {
			/* Set audio frame to default */
			avcodec_get_frame_defaults(audio_input_data->aframe);

			/* Decode audio frame */
			if (avcodec_decode_audio4(codec_ctx, audio_input_data->aframe, &got_frame, &packet) < 0) {
				av_free_packet(&packet);
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Error while decoding audio.\n"));
				dc_producer_end_signal(&audio_input_data->producer, &audio_input_data->circular_buf);
				dc_producer_unlock_previous(&audio_input_data->producer, &audio_input_data->circular_buf);
				return -1;
			}

			if (audio_input_data->aframe->pts != AV_NOPTS_VALUE)
				audio_input_data->next_pts = audio_input_data->aframe->pts;

			audio_input_data->next_pts += ((int64_t)AV_TIME_BASE * audio_input_data->aframe->nb_samples) / codec_ctx->sample_rate;

			/* Did we get a video frame? */
			if (got_frame) {
				av_fifo_generic_write(audio_input_file->fifo, audio_input_data->aframe->data[0], audio_input_data->aframe->linesize[0], NULL);

				if (/*audio_input_file->circular_buf.mode == OFFLINE*/audio_input_file->mode == ON_DEMAND || audio_input_file->mode == LIVE_MEDIA) {
					dc_producer_lock(&audio_input_data->producer, &audio_input_data->circular_buf);

					/* Unlock the previous node in the circular buffer. */
					dc_producer_unlock_previous(&audio_input_data->producer, &audio_input_data->circular_buf);

					/* Get the pointer of the current node in circular buffer. */
					audio_data_node = (AudioDataNode *) dc_producer_produce(&audio_input_data->producer, &audio_input_data->circular_buf);
#ifdef GPAC_USE_LIBAV
					audio_data_node->channels = codec_ctx->channels;
					audio_data_node->channel_layout = codec_ctx->channel_layout;
					audio_data_node->sample_rate = codec_ctx->sample_rate;
					audio_data_node->format = codec_ctx->sample_fmt;
#else

					audio_data_node->channels = audio_input_data->aframe->channels;
					audio_data_node->channel_layout = audio_input_data->aframe->channel_layout;
					audio_data_node->sample_rate = audio_input_data->aframe->sample_rate;
					audio_data_node->format = audio_input_data->aframe->format;
#endif
					audio_data_node->abuf_size = audio_input_data->aframe->linesize[0];
					av_fifo_generic_read(audio_input_file->fifo, audio_data_node->abuf, audio_data_node->abuf_size , NULL);

					dc_producer_advance(&audio_input_data->producer, &audio_input_data->circular_buf);
				} else {
					while (av_fifo_size(audio_input_file->fifo) >= LIVE_FRAME_SIZE) {
						/* Lock the current node in the circular buffer. */
						if (dc_producer_lock(&audio_input_data->producer, &audio_input_data->circular_buf) < 0) {
							GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[dashcast] Live system dropped an audio frame\n"));
							continue;
						}

						/* Unlock the previous node in the circular buffer. */
						dc_producer_unlock_previous(&audio_input_data->producer, &audio_input_data->circular_buf);

						/* Get the pointer of the current node in circular buffer. */
						audio_data_node = (AudioDataNode *) dc_producer_produce( &audio_input_data->producer, &audio_input_data->circular_buf);

						audio_data_node->abuf_size = LIVE_FRAME_SIZE;
						av_fifo_generic_read(audio_input_file->fifo, audio_data_node->abuf, audio_data_node->abuf_size, NULL);

						dc_producer_advance(&audio_input_data->producer, &audio_input_data->circular_buf);
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

	GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Unknown error while reading audio frame.\n"));
	return -1;
}

void dc_audio_decoder_close(AudioInputFile *audio_input_file)
{
	/*
	 * Close the audio format context
	 */
	avformat_close_input(&audio_input_file->av_fmt_ctx);

	if (audio_input_file->av_pkt_list_mutex) {
		gf_mx_p(audio_input_file->av_pkt_list_mutex);
		while (gf_list_count(audio_input_file->av_pkt_list)) {
			AVPacket *pkt = gf_list_last(audio_input_file->av_pkt_list);
			av_free_packet(pkt);
			gf_list_rem_last(audio_input_file->av_pkt_list);
		}
		gf_mx_v(audio_input_file->av_pkt_list_mutex);
		gf_mx_del(audio_input_file->av_pkt_list_mutex);
	}

	av_fifo_free(audio_input_file->fifo);
}
