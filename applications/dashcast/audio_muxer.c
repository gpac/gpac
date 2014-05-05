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

#include "audio_muxer.h"
#include "libavformat/avio.h"


int dc_gpac_audio_moov_create(AudioOutputFile *audio_output_file, char *filename)
{
	GF_Err ret;
	u32 di, track;
	u8 bpsample;
	GF_ESD *esd;
#ifndef GPAC_DISABLE_AV_PARSERS
	GF_M4ADecSpecInfo acfg;
#endif
	AVCodecContext *audio_codec_ctx = audio_output_file->codec_ctx;

	audio_output_file->isof = gf_isom_open(filename, GF_ISOM_OPEN_WRITE, NULL);
	if (!audio_output_file->isof) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot open iso file %s\n", filename));
		return -1;
	}

	esd = gf_odf_desc_esd_new(2);
	if (!esd) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot create GF_ESD\n"));
		return -1;
	}

	esd->decoderConfig = (GF_DecoderConfig *) gf_odf_desc_new(GF_ODF_DCD_TAG);
	esd->slConfig = (GF_SLConfig *) gf_odf_desc_new(GF_ODF_SLC_TAG);
	esd->decoderConfig->streamType = GF_STREAM_AUDIO;
	esd->decoderConfig->objectTypeIndication = GPAC_OTI_AUDIO_MPEG1;
	esd->decoderConfig->bufferSizeDB = 20;
	esd->slConfig->timestampResolution = audio_codec_ctx->sample_rate;
	esd->decoderConfig->decoderSpecificInfo = (GF_DefaultDescriptor *) gf_odf_desc_new(GF_ODF_DSI_TAG);
	esd->ESID = 1;

#ifndef GPAC_DISABLE_AV_PARSERS
	memset(&acfg, 0, sizeof(GF_M4ADecSpecInfo));
	acfg.base_object_type = GF_M4A_LAYER2;
	acfg.base_sr = audio_codec_ctx->sample_rate;
	acfg.nb_chan = audio_codec_ctx->channels;
	acfg.sbr_object_type = 0;
	acfg.audioPL = gf_m4a_get_profile(&acfg);

	ret = gf_m4a_write_config(&acfg, &esd->decoderConfig->decoderSpecificInfo->data, &esd->decoderConfig->decoderSpecificInfo->dataLength);
	assert(ret == GF_OK);
#endif

	//gf_isom_store_movie_config(video_output_file->isof, 0);
	track = gf_isom_new_track(audio_output_file->isof, esd->ESID, GF_ISOM_MEDIA_AUDIO, audio_codec_ctx->sample_rate);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("TimeScale: %d \n", audio_codec_ctx->time_base.den));
	if (!track) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot create new track\n"));
		return -1;
	}

	ret = gf_isom_set_track_enabled(audio_output_file->isof, track, 1);
	if (ret != GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("%s: gf_isom_set_track_enabled\n", gf_error_to_string(ret)));
		return -1;
	}

//	if (!esd->ESID) esd->ESID = gf_isom_get_track_id(audio_output_file->isof, track);

	ret = gf_isom_new_mpeg4_description(audio_output_file->isof, track, esd, NULL, NULL, &di);
	if (ret != GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("%s: gf_isom_new_mpeg4_description\n", gf_error_to_string(ret)));
		return -1;
	}

	gf_odf_desc_del((GF_Descriptor *) esd);
	esd = NULL;

	bpsample = av_get_bytes_per_sample(audio_output_file->codec_ctx->sample_fmt) * 8;

	ret = gf_isom_set_audio_info(audio_output_file->isof, track, di,
	                             audio_codec_ctx->sample_rate, audio_output_file->codec_ctx->channels, bpsample);
	if (ret != GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("%s: gf_isom_set_audio_info\n", gf_error_to_string(ret)));
		return -1;
	}

#ifndef GPAC_DISABLE_AV_PARSERS
	ret = gf_isom_set_pl_indication(audio_output_file->isof, GF_ISOM_PL_AUDIO, acfg.audioPL);
	if (ret != GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("%s: gf_isom_set_pl_indication\n", gf_error_to_string(ret)));
		return -1;
	}
#endif

	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("time scale: %d  sample dur: %d \n", audio_codec_ctx->time_base.den, audio_output_file->codec_ctx->frame_size));

	ret = gf_isom_setup_track_fragment(audio_output_file->isof, track, 1, audio_output_file->codec_ctx->frame_size, 0, 0, 0, 0);
	if (ret != GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("%s: gf_isom_setutrack_fragment\n", gf_error_to_string(ret)));
		return -1;
	}

	//gf_isom_add_track_to_root_od(video_output_file->isof,1);

	ret = gf_isom_finalize_for_fragment(audio_output_file->isof, 1);
	if (ret != GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("%s: gf_isom_finalize_for_fragment\n", gf_error_to_string(ret)));
		return -1;
	}

	return 0;
}

int dc_gpac_audio_isom_open_seg(AudioOutputFile *audio_output_file, char *filename)
{
	GF_Err ret;
	ret = gf_isom_start_segment(audio_output_file->isof, filename, GF_TRUE);
	if (ret != GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("%s: gf_isom_start_segment\n", gf_error_to_string(ret)));
		return -1;
	}

	audio_output_file->dts = 0;

	return 0;
}

int dc_gpac_audio_isom_write(AudioOutputFile *audio_output_file)
{
	GF_Err ret;
	//AVStream *video_stream = video_output_file->av_fmt_ctx->streams[video_output_file->vstream_idx];
	//AVCodecContext *video_codec_ctx = video_stream->codec;
	//AVCodecContext *audio_codec_ctx = audio_output_file->codec_ctx;

	audio_output_file->sample->data = (char *) audio_output_file->packet.data;
	audio_output_file->sample->dataLength = audio_output_file->packet.size;

	audio_output_file->sample->DTS = audio_output_file->dts; //audio_output_file->aframe->pts;
	audio_output_file->sample->IsRAP = 1; //audio_output_file->aframe->key_frame;//audio_codec_ctx->coded_frame->key_frame;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("RAP %d , DTS %ld \n", audio_output_file->sample->IsRAP, audio_output_file->sample->DTS));

	ret = gf_isom_fragment_add_sample(audio_output_file->isof, 1, audio_output_file->sample, 1, audio_output_file->codec_ctx->frame_size, 0, 0, 0);
	audio_output_file->dts += audio_output_file->codec_ctx->frame_size;
	if (ret != GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("%s: gf_isom_fragment_add_sample\n", gf_error_to_string(ret)));
		return -1;
	}

//	ret = gf_isom_flush_fragments(video_output_file->isof, 1);
//	if (ret != GF_OK) {
//		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("%s: gf_isom_flush_fragments\n", gf_error_to_string(ret)));
//		return -1;
//	}

	return 0;
}

int dc_gpac_audio_isom_close_seg(AudioOutputFile *audio_output_file)
{
	GF_Err ret;
	ret = gf_isom_close_segment(audio_output_file->isof, 0, 0,0, 0, 0, 0, 1, audio_output_file->seg_marker, NULL, NULL);
	if (ret != GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("%s: gf_isom_close_segment\n", gf_error_to_string(ret)));
		return -1;
	}

	//audio_output_file->acc_samples = 0;

	return 0;
}

int dc_gpac_audio_isom_close(AudioOutputFile *audio_output_file)
{
	GF_Err ret;
	ret = gf_isom_close(audio_output_file->isof);
	if (ret != GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("%s: gf_isom_close\n", gf_error_to_string(ret)));
		return -1;
	}

	//audio_output_file->acc_samples = 0;

	return 0;
}

int dc_ffmpeg_audio_muxer_open(AudioOutputFile *audio_output_file, char *filename)
{
	AVStream *audio_stream;
	AVOutputFormat *output_fmt;

	AVCodecContext *audio_codec_ctx = audio_output_file->codec_ctx;
	audio_output_file->av_fmt_ctx = NULL;

//	strcpy(audio_output_file->filename, audio_data_conf->filename);
//	audio_output_file->abr = audio_data_conf->bitrate;
//	audio_output_file->asr = audio_data_conf->samplerate;
//	audio_output_file->ach = audio_data_conf->channels;
//	strcpy(audio_output_file->codec, audio_data_conf->codec);

	/* Find output format */
	output_fmt = av_guess_format(NULL, filename, NULL);
	if (!output_fmt) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot find suitable output format\n"));
		return -1;
	}

	audio_output_file->av_fmt_ctx = avformat_alloc_context();
	if (!audio_output_file->av_fmt_ctx) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot allocate memory for pOutVideoFormatCtx\n"));
		return -1;
	}

	audio_output_file->av_fmt_ctx->oformat = output_fmt;
	strcpy(audio_output_file->av_fmt_ctx->filename, filename);

	/* Open the output file */
	if (!(output_fmt->flags & AVFMT_NOFILE)) {
		if (avio_open(&audio_output_file->av_fmt_ctx->pb, filename, URL_WRONLY) < 0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot not open '%s'\n", filename));
			return -1;
		}
	}

	audio_stream = avformat_new_stream(audio_output_file->av_fmt_ctx, audio_output_file->codec);
	if (!audio_stream) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot create output video stream\n"));
		return -1;
	}

	audio_stream->codec->codec_id = audio_output_file->codec->id;
	audio_stream->codec->codec_type = AVMEDIA_TYPE_AUDIO;
	audio_stream->codec->bit_rate = audio_codec_ctx->bit_rate;//audio_output_file->audio_data_conf->bitrate;
	audio_stream->codec->sample_rate = audio_codec_ctx->sample_rate;//audio_output_file->audio_data_conf->samplerate;
	audio_stream->codec->channels = audio_codec_ctx->channels;//audio_output_file->audio_data_conf->channels;
	audio_stream->codec->sample_fmt = AV_SAMPLE_FMT_S16;

//	if (audio_output_file->av_fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
//		audio_output_file->codec_ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;

	//video_stream->codec = video_output_file->codec_ctx;

	/* open the video codec */
	if (avcodec_open2(audio_stream->codec, audio_output_file->codec, NULL) < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot open output video codec\n"));
		return -1;
	}

	avformat_write_header(audio_output_file->av_fmt_ctx, NULL);

	return 0;
}

int dc_ffmpeg_audio_muxer_write(AudioOutputFile *audio_output_file)
{
	AVStream *audio_stream = audio_output_file->av_fmt_ctx->streams[audio_output_file->astream_idx];
	AVCodecContext *audio_codec_ctx = audio_stream->codec;

	audio_output_file->packet.stream_index = audio_stream->index;

	if (audio_output_file->packet.pts != AV_NOPTS_VALUE)
		audio_output_file->packet.pts = av_rescale_q(audio_output_file->packet.pts, audio_codec_ctx->time_base, audio_stream->time_base);

	if (audio_output_file->packet.duration > 0)
		audio_output_file->packet.duration = (int)av_rescale_q(audio_output_file->packet.duration, audio_codec_ctx->time_base, audio_stream->time_base);
	/*
	 * if (pkt.pts != AV_NOPTS_VALUE)
	 * pkt.pts = av_rescale_q(pkt.pts, audioEncCtx->time_base, audioStream->time_base);
	 */

	audio_output_file->packet.flags |= AV_PKT_FLAG_KEY;

	if (av_interleaved_write_frame(audio_output_file->av_fmt_ctx, &audio_output_file->packet) != 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Writing frame is not successful\n"));
		av_free_packet(&audio_output_file->packet);
		return -1;
	}

	av_free_packet(&audio_output_file->packet);

	return 0;
}

int dc_ffmpeg_audio_muxer_close(AudioOutputFile *audio_output_file)
{
	u32 i;

	av_write_trailer(audio_output_file->av_fmt_ctx);
	avio_close(audio_output_file->av_fmt_ctx->pb);

	// free the streams
	for (i = 0; i < audio_output_file->av_fmt_ctx->nb_streams; i++) {
		avcodec_close(audio_output_file->av_fmt_ctx->streams[i]->codec);
		av_freep(&audio_output_file->av_fmt_ctx->streams[i]->info);
	}

	//video_output_file->av_fmt_ctx->streams[video_output_file->vstream_idx]->codec = NULL;
	avformat_free_context(audio_output_file->av_fmt_ctx);

	//audio_output_file->acc_samples = 0;

	return 0;

}

int dc_audio_muxer_init(AudioOutputFile *audio_output_file, AudioDataConf *audio_data_conf, AudioMuxerType muxer_type, int frame_per_seg, int frame_per_frag, u32 seg_marker)
{
	char name[GF_MAX_PATH];
	snprintf(name, sizeof(name), "audio encoder %s", audio_data_conf->filename);
	dc_consumer_init(&audio_output_file->consumer, AUDIO_CB_SIZE, name);

	audio_output_file->sample = gf_isom_sample_new();
	audio_output_file->isof = NULL;
	audio_output_file->muxer_type = muxer_type;

	audio_output_file->frame_per_seg = frame_per_seg;
	audio_output_file->frame_per_frag = frame_per_frag;

	audio_output_file->seg_marker = seg_marker;

	return 0;
}

void dc_audio_muxer_free(AudioOutputFile *audio_output_file)
{
	if (audio_output_file->isof != NULL) {
		gf_isom_close(audio_output_file->isof);
	}

	//gf_isom_sample_del(&audio_output_file->sample);
}

GF_Err dc_audio_muxer_open(AudioOutputFile *audio_output_file, char *directory, char *id_name, int seg)
{
	GF_Err ret;
	char name[GF_MAX_PATH];

	switch (audio_output_file->muxer_type) {
	case FFMPEG_AUDIO_MUXER:
		snprintf(name, sizeof(name), "%s/%s_%d_ffmpeg.mp4", directory, id_name, seg);
		return dc_ffmpeg_audio_muxer_open(audio_output_file, name);
	case GPAC_AUDIO_MUXER:
		snprintf(name, sizeof(name), "%s/%s_%d_gpac.mp4", directory, id_name, seg);
		dc_gpac_audio_moov_create(audio_output_file, name);
		return dc_gpac_audio_isom_open_seg(audio_output_file, NULL);
	case GPAC_INIT_AUDIO_MUXER:
		if (seg == 0) {
			snprintf(name, sizeof(name), "%s/%s_init_gpac.mp4", directory, id_name);
			dc_gpac_audio_moov_create(audio_output_file, name);
			audio_output_file->first_dts = 0;
		}
		snprintf(name, sizeof(name), "%s/%s_%d_gpac.m4s", directory, id_name, seg);
		ret = dc_gpac_audio_isom_open_seg(audio_output_file, name);
		return ret;
	default:
		return GF_BAD_PARAM;
	}

	return GF_BAD_PARAM;
}

int dc_audio_muxer_write(AudioOutputFile *audio_output_file, int frame_nb)
{
	switch (audio_output_file->muxer_type) {
	case FFMPEG_AUDIO_MUXER:
		return dc_ffmpeg_audio_muxer_write(audio_output_file);
	case GPAC_AUDIO_MUXER:
	case GPAC_INIT_AUDIO_MUXER:
		if (frame_nb % audio_output_file->frame_per_frag == 0) {
			gf_isom_start_fragment(audio_output_file->isof, 1);
			gf_isom_set_traf_base_media_decode_time(audio_output_file->isof, 1, audio_output_file->first_dts * audio_output_file->frame_size);
			audio_output_file->first_dts += audio_output_file->frame_per_frag;
		}
		dc_gpac_audio_isom_write(audio_output_file);
		if (frame_nb % audio_output_file->frame_per_frag == audio_output_file->frame_per_frag - 1) {
			gf_isom_flush_fragments(audio_output_file->isof, 1);
		}
		if (frame_nb + 1 == audio_output_file->frame_per_seg) {
			return 1;
		}
		return 0;
	default:
		return GF_BAD_PARAM;
	}

	return GF_BAD_PARAM;
}

int dc_audio_muxer_close(AudioOutputFile *audio_output_file)
{
	switch (audio_output_file->muxer_type) {
	case FFMPEG_AUDIO_MUXER:
		return dc_ffmpeg_audio_muxer_close(audio_output_file);
	case GPAC_AUDIO_MUXER:
		dc_gpac_audio_isom_close_seg(audio_output_file);
		return dc_gpac_audio_isom_close(audio_output_file);
	case GPAC_INIT_AUDIO_MUXER:
		return dc_gpac_audio_isom_close_seg(audio_output_file);
	default:
		return GF_BAD_PARAM;
	}

	return GF_BAD_PARAM;
}
