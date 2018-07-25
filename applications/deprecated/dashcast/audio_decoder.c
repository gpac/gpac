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

int dc_audio_decoder_open(AudioInputFile *audio_input_file, AudioDataConf *audio_data_conf, int mode, int no_loop, int video_framerate)
{
	u32 i;
	AVCodecContext *codec_ctx;
	AVCodec *codec;
	AVInputFormat *in_fmt = NULL;

	if (!audio_data_conf) return -1;
	
	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DashCast] Audio Decoder enter setup at UTC "LLU"\n", gf_net_get_utc() ));
	
	if (strcmp(audio_data_conf->format,"") != 0) {
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
		s32 ret;
		AVDictionary *options = NULL;
		//we may need to set the framerate when the default one used by ffmpeg is not supported
		if (video_framerate > 0) {
			char vfr[16];
			snprintf(vfr, sizeof(vfr), "%d", video_framerate);
			ret = av_dict_set(&options, "framerate", vfr, 0);
			if (ret < 0) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Could not set video framerate %s.\n", vfr));
				return -1;
			}
		}
		
		ret = avformat_open_input(&audio_input_file->av_fmt_ctx, audio_data_conf->filename, in_fmt, options ? &options : NULL);

		if (ret != 0) {
			if (options) {
				av_dict_free(&options);
				options = NULL;
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Error %d opening input - retrying without options\n", ret));
				ret = avformat_open_input(&audio_input_file->av_fmt_ctx, audio_data_conf->filename, in_fmt, NULL);
			}

			if (ret != 0) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot open file: %s\n", audio_data_conf->filename));
				return -1;
			}
		}

		if (options) av_dict_free(&options);

		/*
		* Retrieve stream information
		*/
		if (avformat_find_stream_info(audio_input_file->av_fmt_ctx, NULL) < 0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot find stream information\n"));
			return -1;
		}

		av_dump_format(audio_input_file->av_fmt_ctx, 0, audio_data_conf->filename, 0);
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DashCast] Audio capture open at UTC "LLU"\n", gf_net_get_utc() ));

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

#ifdef DC_AUDIO_RESAMPLER
	audio_input_file->aresampler = NULL;
#endif
	audio_input_file->fifo = av_fifo_alloc(2 * MAX_AUDIO_PACKET_SIZE);

	audio_data_conf->channels = codec_ctx->channels;
	audio_data_conf->samplerate = codec_ctx->sample_rate;

	audio_input_file->mode = mode;
	audio_input_file->no_loop = no_loop;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DashCast] Audio Decoder open at UTC "LLU"\n", gf_net_get_utc() ));
	
	return 0;
}

#ifdef DC_AUDIO_RESAMPLER
static int ensure_resampler(AudioInputFile *audio_input_file, int sample_rate, int num_channels, u64 channel_layout, enum AVSampleFormat sample_format)
{
	if (!audio_input_file->aresampler) {
		audio_input_file->aresampler = swr_alloc();
		if (!audio_input_file->aresampler) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot allocate the audio resampler. Aborting.\n"));
			return -1;
		}
		av_opt_set_channel_layout(audio_input_file->aresampler, "in_channel_layout", channel_layout, 0);
		av_opt_set_channel_layout(audio_input_file->aresampler, "out_channel_layout", DC_AUDIO_CHANNEL_LAYOUT, 0);
		av_opt_set_sample_fmt(audio_input_file->aresampler, "in_sample_fmt", sample_format, 0);
		av_opt_set_sample_fmt(audio_input_file->aresampler, "out_sample_fmt", DC_AUDIO_SAMPLE_FORMAT, 0);
		av_opt_set_int(audio_input_file->aresampler, "in_sample_rate", sample_rate, 0);
		av_opt_set_int(audio_input_file->aresampler, "out_sample_rate", DC_AUDIO_SAMPLE_RATE, 0);
		av_opt_set_int(audio_input_file->aresampler, "in_channels", num_channels, 0);
		av_opt_set_int(audio_input_file->aresampler, "out_channels", DC_AUDIO_NUM_CHANNELS, 0);

		if (swr_init(audio_input_file->aresampler)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Could not open the audio resampler. Aborting.\n"));
			return -1;
		}
	}

	return 0;
}

//resample - see http://ffmpeg.org/pipermail/libav-user/2012-June/002164.html
static int resample_audio(AudioInputFile *audio_input_file, AudioInputData *audio_input_data, AVCodecContext *audio_codec_ctx, uint8_t ***output, int *num_planes_out, int num_channels, enum AVSampleFormat sample_format)
{
	int i;
	*num_planes_out = av_sample_fmt_is_planar(DC_AUDIO_SAMPLE_FORMAT) ? DC_AUDIO_NUM_CHANNELS : 1;
	*output = (uint8_t**)av_malloc(*num_planes_out*sizeof(uint8_t*));
	for (i=0; i<*num_planes_out; i++) {
		(*output) [i] = (uint8_t*)av_malloc(DC_AUDIO_MAX_CHUNCK_SIZE); //FIXME: fix using size below av_samples_get_buffer_size()
	}

	i = swr_convert(audio_input_file->aresampler, *output, audio_input_data->aframe->nb_samples, audio_input_data->aframe->extended_data, audio_input_data->aframe->nb_samples);
	if (i < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Could not resample audio frame. Aborting.\n"));
		return -1;
	}

	return i;
}
#endif

int dc_audio_decoder_read(AudioInputFile *audio_input_file, AudioInputData *audio_input_data)
{
	int ret;
	AVPacket packet;
	int got_frame = 0;
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
				packet_copy = (AVPacket*)gf_list_pop_front(audio_input_file->av_pkt_list);
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

#ifndef FF_API_AVFRAME_LAVC
			avcodec_get_frame_defaults(audio_input_data->aframe);
#else
			av_frame_unref(audio_input_data->aframe);
#endif

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

#ifndef FF_API_AVFRAME_LAVC
			avcodec_get_frame_defaults(audio_input_data->aframe);
#else
			av_frame_unref(audio_input_data->aframe);
#endif

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

			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DashCast] Decode audio frame pts %d at UTC "LLU"\n", audio_input_data->next_pts, gf_net_get_utc() ));
			
			/* Did we get an audio frame? */
			if (got_frame) {
				uint8_t **data;
				int data_size;
				enum AVSampleFormat sample_format;
				Bool resample;
#ifdef DC_AUDIO_RESAMPLER
				int num_planes_out=0;
#endif
#ifdef GPAC_USE_LIBAV
				int sample_rate = codec_ctx->sample_rate;
				int num_channels = codec_ctx->channels;
				u64 channel_layout = codec_ctx->channel_layout;
#else
				int sample_rate = audio_input_data->aframe->sample_rate;
				int num_channels = audio_input_data->aframe->channels;
				u64 channel_layout;
				if (!audio_input_data->aframe->channel_layout) {
					if (audio_input_data->aframe->channels == 2) {
						audio_input_data->aframe->channel_layout = AV_CH_LAYOUT_STEREO;
					} else if (audio_input_data->aframe->channels == 1) {
						audio_input_data->aframe->channel_layout = AV_CH_LAYOUT_MONO;
					} else {
						GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Unknown input channel layout for %d channels. Aborting.\n", audio_input_data->aframe->channels));
						exit(1);
					}
				}
				channel_layout = audio_input_data->aframe->channel_layout;
#endif
				sample_format = (enum AVSampleFormat)audio_input_data->aframe->format;
				resample = (sample_rate    != DC_AUDIO_SAMPLE_RATE
				                 || num_channels   != DC_AUDIO_NUM_CHANNELS
				                 || channel_layout != DC_AUDIO_CHANNEL_LAYOUT
				                 || sample_format  != DC_AUDIO_SAMPLE_FORMAT);

				/* Resample if needed */
				if (resample) {
#ifndef DC_AUDIO_RESAMPLER
					GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Audio resampling is needed at the decoding stage, but not supported by your version of DashCast. Aborting.\n"));
					exit(1);
#else
					uint8_t **output;
					int nb_samp;
					if (ensure_resampler(audio_input_file, sample_rate, num_channels, channel_layout, sample_format)) {
						return -1;
					}

					nb_samp = resample_audio(audio_input_file, audio_input_data, codec_ctx, &output, &num_planes_out, num_channels, sample_format);
					if (nb_samp<0) {
						return -1;
					}

					av_samples_get_buffer_size(&data_size, DC_AUDIO_NUM_CHANNELS, nb_samp, DC_AUDIO_SAMPLE_FORMAT, 0);
					data = output;
#endif
				} else {
					/*no resampling needed: read data from the AVFrame*/
					data = audio_input_data->aframe->extended_data;
					data_size = audio_input_data->aframe->linesize[0];
				}

				assert(!av_sample_fmt_is_planar(DC_AUDIO_SAMPLE_FORMAT));
				av_fifo_generic_write(audio_input_file->fifo, data[0], data_size, NULL);

				if (/*audio_input_file->circular_buf.mode == OFFLINE*/audio_input_file->mode == ON_DEMAND || audio_input_file->mode == LIVE_MEDIA) {
					dc_producer_lock(&audio_input_data->producer, &audio_input_data->circular_buf);

					/* Unlock the previous node in the circular buffer. */
					dc_producer_unlock_previous(&audio_input_data->producer, &audio_input_data->circular_buf);

					/* Get the pointer of the current node in circular buffer. */
					audio_data_node = (AudioDataNode *) dc_producer_produce(&audio_input_data->producer, &audio_input_data->circular_buf);
					audio_data_node->channels = DC_AUDIO_NUM_CHANNELS;
					audio_data_node->channel_layout = DC_AUDIO_CHANNEL_LAYOUT;
					audio_data_node->sample_rate = DC_AUDIO_SAMPLE_RATE;
					audio_data_node->format = DC_AUDIO_SAMPLE_FORMAT;
					audio_data_node->abuf_size = data_size;
					av_fifo_generic_read(audio_input_file->fifo, audio_data_node->abuf, audio_data_node->abuf_size, NULL);

					dc_producer_advance(&audio_input_data->producer, &audio_input_data->circular_buf);
				} else {
					while (av_fifo_size(audio_input_file->fifo) >= LIVE_FRAME_SIZE) {
						/* Lock the current node in the circular buffer. */
						if (dc_producer_lock(&audio_input_data->producer, &audio_input_data->circular_buf) < 0) {
							continue;
						}

						/* Unlock the previous node in the circular buffer. */
						dc_producer_unlock_previous(&audio_input_data->producer, &audio_input_data->circular_buf);

						/* Get the pointer of the current node in circular buffer. */
						audio_data_node = (AudioDataNode *) dc_producer_produce(&audio_input_data->producer, &audio_input_data->circular_buf);

						audio_data_node->abuf_size = LIVE_FRAME_SIZE;
						av_fifo_generic_read(audio_input_file->fifo, audio_data_node->abuf, audio_data_node->abuf_size, NULL);

						dc_producer_advance(&audio_input_data->producer, &audio_input_data->circular_buf);
					}
				}

#ifdef DC_AUDIO_RESAMPLER
				if (resample) {
					int i;
					for (i=0; i<num_planes_out; ++i) {
						av_free(data[i]);
					}
					av_free(data);
				}
#endif

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
			AVPacket *pkt = (AVPacket*)gf_list_last(audio_input_file->av_pkt_list);
			av_free_packet(pkt);
			gf_list_rem_last(audio_input_file->av_pkt_list);
		}
		gf_list_del(audio_input_file->av_pkt_list);
		gf_mx_v(audio_input_file->av_pkt_list_mutex);
		gf_mx_del(audio_input_file->av_pkt_list_mutex);
	}

	av_fifo_free(audio_input_file->fifo);

#ifdef DC_AUDIO_RESAMPLER
	swr_free(&audio_input_file->aresampler);
#endif
}
