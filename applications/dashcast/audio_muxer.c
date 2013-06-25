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

int dc_gpac_audio_moov_create(AudioOutputFile * p_aoutf, char * psz_name) {

	GF_Err ret;

	AVCodecContext * p_audio_codec_ctx = p_aoutf->p_codec_ctx;
	u32 di;
	u32 track;
	u8 bpsample;
	GF_ESD * p_esd;
	GF_M4ADecSpecInfo acfg;

	p_aoutf->p_isof = gf_isom_open(psz_name,
			GF_ISOM_OPEN_WRITE, NULL);

	if (!p_aoutf->p_isof) {
		fprintf(stderr, "Cannot open iso file %s\n",
				psz_name);
		return -1;
	}

	memset(&acfg, 0, sizeof(GF_M4ADecSpecInfo));
	acfg.base_object_type = GF_M4A_AAC_LC;
	acfg.base_sr = p_audio_codec_ctx->sample_rate;
	acfg.nb_chan = p_audio_codec_ctx->channels;
	acfg.sbr_object_type = 0;

	acfg.audioPL = gf_m4a_get_profile(&acfg);

	p_esd = gf_odf_desc_esd_new(2);

	if (!p_esd) {
		fprintf(stderr, "Cannot create GF_ESD\n");
		return -1;
	}

	p_esd->decoderConfig = (GF_DecoderConfig *) gf_odf_desc_new(GF_ODF_DCD_TAG);
	p_esd->slConfig = (GF_SLConfig *) gf_odf_desc_new(GF_ODF_SLC_TAG);
	p_esd->decoderConfig->streamType = GF_STREAM_AUDIO;
	p_esd->decoderConfig->objectTypeIndication = GPAC_OTI_AUDIO_AAC_MPEG4;
	p_esd->decoderConfig->bufferSizeDB = 20;
	p_esd->slConfig->timestampResolution = p_audio_codec_ctx->sample_rate;
	p_esd->decoderConfig->decoderSpecificInfo = (GF_DefaultDescriptor *) gf_odf_desc_new(GF_ODF_DSI_TAG);
	p_esd->ESID = 1;

	ret = gf_m4a_write_config(&acfg, &p_esd->decoderConfig->decoderSpecificInfo->data, &p_esd->decoderConfig->decoderSpecificInfo->dataLength);

	//gf_isom_store_movie_config(p_voutf->p_isof, 0);
	track = gf_isom_new_track(p_aoutf->p_isof, p_esd->ESID,
			GF_ISOM_MEDIA_AUDIO, p_audio_codec_ctx->sample_rate);

	//printf("TimeScale: %d \n", p_video_codec_ctx->time_base.den);
	if (!track) {
		fprintf(stderr, "Cannot create new track\n");
		return -1;
	}

	ret = gf_isom_set_track_enabled(p_aoutf->p_isof, track, 1);
	if (ret != GF_OK) {
		fprintf(stderr, "%s: gf_isom_set_track_enabled\n",
				gf_error_to_string(ret));
		return -1;
	}

//	if (!p_esd->ESID) p_esd->ESID = gf_isom_get_track_id(p_aoutf->p_isof, track);

	ret = gf_isom_new_mpeg4_description(p_aoutf->p_isof, track, p_esd, NULL,
			NULL, &di);
	if (ret != GF_OK) {
		fprintf(stderr, "%s: gf_isom_new_mpeg4_description\n",
				gf_error_to_string(ret));
		return -1;
	}

	gf_odf_desc_del((GF_Descriptor *) p_esd);
	p_esd = NULL;

	bpsample = av_get_bytes_per_sample(p_aoutf->p_codec_ctx->sample_fmt) * 8;

	ret = gf_isom_set_audio_info(p_aoutf->p_isof, track, di,
			p_audio_codec_ctx->sample_rate, p_aoutf->p_codec_ctx->channels,
			bpsample);

	if (ret != GF_OK) {
		fprintf(stderr, "%s: gf_isom_set_audio_info\n",
				gf_error_to_string(ret));
		return -1;
	}

	ret = gf_isom_set_pl_indication(p_aoutf->p_isof, GF_ISOM_PL_AUDIO, acfg.audioPL);

	if (ret != GF_OK) {
		fprintf(stderr, "%s: gf_isom_set_pl_indication\n",
				gf_error_to_string(ret));
		return -1;
	}

	//printf("time scale: %d  sample dur: %d \n",
	//		p_video_codec_ctx->time_base.den, p_aoutf->p_codec_ctx->frame_size);

	ret = gf_isom_setup_track_fragment(p_aoutf->p_isof, track, 1,
			p_aoutf->p_codec_ctx->frame_size, 0, 0, 0, 0);
	if (ret != GF_OK) {
		fprintf(stderr, "%s: gf_isom_setup_track_fragment\n",
				gf_error_to_string(ret));
		return -1;
	}

	//gf_isom_add_track_to_root_od(p_voutf->p_isof,1);

	ret = gf_isom_finalize_for_fragment(p_aoutf->p_isof, 1);
	if (ret != GF_OK) {
		fprintf(stderr, "%s: gf_isom_finalize_for_fragment\n",
				gf_error_to_string(ret));
		return -1;
	}

	return 0;
}

int dc_gpac_audio_isom_open_seg(AudioOutputFile * p_aoutf, char * psz_name) {

	GF_Err ret;

	ret = gf_isom_start_segment(p_aoutf->p_isof, psz_name);
	if (ret != GF_OK) {
		fprintf(stderr, "%s: gf_isom_start_segment\n", gf_error_to_string(ret));
		return -1;
	}

//	ret = gf_isom_start_fragment(p_aoutf->p_isof, 1);
//	if (ret != GF_OK) {
//		fprintf(stderr, "%s: gf_isom_start_fragment\n",
//				gf_error_to_string(ret));
//		return -1;
//	}

	p_aoutf->dts = 0;

	return 0;
}

int dc_gpac_audio_isom_write(AudioOutputFile * p_aoutf) {

	GF_Err ret;
	//AVStream * p_video_stream =
	//		p_voutf->p_fmt->streams[p_voutf->i_vstream_idx];
	//AVCodecContext * p_video_codec_ctx = p_video_stream->codec;
	//AVCodecContext * p_audio_codec_ctx = p_aoutf->p_codec_ctx;

	p_aoutf->p_sample->data = (char *) p_aoutf->packet.data;
	p_aoutf->p_sample->dataLength = p_aoutf->packet.size;

	p_aoutf->p_sample->DTS = p_aoutf->dts; //p_aoutf->p_aframe->pts;
	p_aoutf->p_sample->IsRAP = 1; //p_aoutf->p_aframe->key_frame;//p_audio_codec_ctx->coded_frame->key_frame;
	//printf("RAP %d , DTS %ld \n", p_aoutf->p_sample->IsRAP, p_aoutf->p_sample->DTS);

	ret = gf_isom_fragment_add_sample(p_aoutf->p_isof, 1, p_aoutf->p_sample, 1,
			p_aoutf->p_codec_ctx->frame_size, 0, 0, 0);

	p_aoutf->dts += p_aoutf->p_codec_ctx->frame_size;
	if (ret != GF_OK) {
		fprintf(stderr, "%s: gf_isom_fragment_add_sample\n",
				gf_error_to_string(ret));
		return -1;
	}

//	ret = gf_isom_flush_fragments(p_voutf->p_isof, 1);
//	if (ret != GF_OK) {
//		fprintf(stderr, "%s: gf_isom_flush_fragments\n",
//				gf_error_to_string(ret));
//		return -1;
//	}

	return 0;
}

int dc_gpac_audio_isom_close_seg(AudioOutputFile * p_aoutf) {

	GF_Err ret;

	ret = gf_isom_close_segment(p_aoutf->p_isof, 0, 0, 0, 0, 0, 1, p_aoutf->i_seg_marker, NULL,
			NULL);
	if (ret != GF_OK) {
		fprintf(stderr, "%s: gf_isom_close_segment\n", gf_error_to_string(ret));
		return -1;
	}

	//p_aoutf->acc_samples = 0;

	return 0;
}

int dc_gpac_audio_isom_close(AudioOutputFile * p_aoutf) {

	GF_Err ret;

	ret = gf_isom_close(p_aoutf->p_isof);
	if (ret != GF_OK) {
		fprintf(stderr, "%s: gf_isom_close\n", gf_error_to_string(ret));
		return -1;
	}

	//p_aoutf->acc_samples = 0;

	return 0;
}

int dc_ffmpeg_audio_muxer_open(AudioOutputFile * p_aoutf, char * psz_name) {

	AVStream * p_audio_stream;
	AVOutputFormat * p_output_fmt;

	AVCodecContext * p_audio_codec_ctx = p_aoutf->p_codec_ctx;
	p_aoutf->p_fmt = NULL;

//	strcpy(p_aoutf->psz_name, p_aconf->psz_name);
//	p_aoutf->i_abr = p_aconf->i_bitrate;
//	p_aoutf->i_asr = p_aconf->i_samplerate;
//	p_aoutf->i_ach = p_aconf->i_channels;
//	strcpy(p_aoutf->psz_codec, p_aconf->psz_codec);

	/*  Find output format  */
	p_output_fmt = av_guess_format(NULL, psz_name, NULL);
	if (!p_output_fmt) {
		fprintf(stderr, "Cannot find suitable output format\n");
		return -1;
	}

	p_aoutf->p_fmt = avformat_alloc_context();
	if (!p_aoutf->p_fmt) {
		fprintf(stderr, "Cannot allocate memory for pOutVideoFormatCtx\n");
		return -1;
	}

	p_aoutf->p_fmt->oformat = p_output_fmt;
	strcpy(p_aoutf->p_fmt->filename, psz_name);

	/*  Open the output file  */
	if (!(p_output_fmt->flags & AVFMT_NOFILE)) {
		if (avio_open(&p_aoutf->p_fmt->pb, psz_name,
				URL_WRONLY) < 0) {
			fprintf(stderr, "Cannot not open '%s'\n",
					psz_name);
			return -1;
		}
	}

	p_audio_stream = avformat_new_stream(p_aoutf->p_fmt,
			p_aoutf->p_codec);
	if (!p_audio_stream) {
		fprintf(stderr, "Cannot create output video stream\n");
		return -1;
	}

	p_audio_stream->codec->codec_id = p_aoutf->p_codec->id;
	p_audio_stream->codec->codec_type = AVMEDIA_TYPE_AUDIO;
	p_audio_stream->codec->bit_rate = p_audio_codec_ctx->bit_rate;//p_aoutf->p_adata->i_bitrate;
	p_audio_stream->codec->sample_rate = p_audio_codec_ctx->sample_rate;//p_aoutf->p_adata->i_samplerate;
	p_audio_stream->codec->channels = p_audio_codec_ctx->channels;//p_aoutf->p_adata->i_channels;
	p_audio_stream->codec->sample_fmt = AV_SAMPLE_FMT_S16;

//	if (p_aoutf->p_fmt->oformat->flags & AVFMT_GLOBALHEADER)
//		p_aoutf->p_codec_ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;

	//p_video_stream->codec = p_voutf->p_codec_ctx;

	/* open the video codec */
	if (avcodec_open2(p_audio_stream->codec, p_aoutf->p_codec, NULL) < 0) {
		fprintf(stderr, "Cannot open output video codec\n");
		return -1;
	}

	avformat_write_header(p_aoutf->p_fmt, NULL);

	return 0;
}

int dc_ffmpeg_audio_muxer_write(AudioOutputFile * p_aout) {

	AVStream * p_audio_stream = p_aout->p_fmt->streams[p_aout->i_astream_idx];
	AVCodecContext * p_audio_codec_ctx = p_audio_stream->codec;

	p_aout->packet.stream_index = p_audio_stream->index;

	if (p_aout->packet.pts != AV_NOPTS_VALUE)
		p_aout->packet.pts = av_rescale_q(p_aout->packet.pts,
				p_audio_codec_ctx->time_base, p_audio_stream->time_base);

	if (p_aout->packet.duration > 0)
		p_aout->packet.duration = (int)av_rescale_q(p_aout->packet.duration,
				p_audio_codec_ctx->time_base, p_audio_stream->time_base);
	/*
	 * if (pkt.pts != AV_NOPTS_VALUE)
	 * pkt.pts = av_rescale_q(pkt.pts, audioEncCtx->time_base,
	 * audioStream->time_base);
	 */

	p_aout->packet.flags |= AV_PKT_FLAG_KEY;

	if (av_interleaved_write_frame(p_aout->p_fmt, &p_aout->packet) != 0) {

		fprintf(stderr, "Writing frame is not successful\n");
		av_free_packet(&p_aout->packet);
		return -1;
	}

	av_free_packet(&p_aout->packet);

	return 0;
}

int dc_ffmpeg_audio_muxer_close(AudioOutputFile * p_aoutf) {

	u32 i;

	av_write_trailer(p_aoutf->p_fmt);

	avio_close(p_aoutf->p_fmt->pb);

	// free the streams
	for (i = 0; i < p_aoutf->p_fmt->nb_streams; i++) {
		avcodec_close(p_aoutf->p_fmt->streams[i]->codec);
		av_freep(&p_aoutf->p_fmt->streams[i]->info);
	}

	//p_voutf->p_fmt->streams[p_voutf->i_vstream_idx]->codec = NULL;
	avformat_free_context(p_aoutf->p_fmt);

	//p_aoutf->acc_samples = 0;

	return 0;

}

int dc_audio_muxer_init(AudioOutputFile * p_aoutf, AudioData * p_adata,
		AudioMuxerType muxer_type, int frame_per_seg, int frame_per_frag, u32 seg_marker) {

	char name[256];
	sprintf(name, "audio encoder %s", p_adata->psz_name);
	dc_consumer_init(&p_aoutf->acon, AUDIO_CB_SIZE, name);

	p_aoutf->p_sample = gf_isom_sample_new();
	p_aoutf->p_isof = NULL;
	p_aoutf->muxer_type = muxer_type;

	p_aoutf->i_frame_per_seg = frame_per_seg;
	p_aoutf->i_frame_per_frag = frame_per_frag;

	p_aoutf->i_seg_marker = seg_marker;

	return 0;
}

void dc_audio_muxer_free(AudioOutputFile * p_aoutf) {

	if (p_aoutf->p_isof != NULL) {
		gf_isom_close(p_aoutf->p_isof);
	}

	gf_isom_sample_del(&p_aoutf->p_sample);
}

GF_Err dc_audio_muxer_open(AudioOutputFile * p_aoutf, char * psz_directory, char * psz_id, int i_seg) {

	GF_Err ret;
	char psz_name[256];

	switch (p_aoutf->muxer_type) {

	case FFMPEG_AUDIO_MUXER:
		sprintf(psz_name, "%s/%s_%d_ffmpeg.mp4",
				psz_directory, psz_id, i_seg);
		return dc_ffmpeg_audio_muxer_open(p_aoutf, psz_name);
	case GPAC_AUDIO_MUXER:
		sprintf(psz_name, "%s/%s_%d_gpac.mp4",
				psz_directory, psz_id, i_seg);
		dc_gpac_audio_moov_create(p_aoutf, psz_name);
		return dc_gpac_audio_isom_open_seg(p_aoutf, NULL);
	case GPAC_INIT_AUDIO_MUXER:
		if(i_seg == 0) {
			sprintf(psz_name, "%s/%s_init_gpac.mp4",
								psz_directory, psz_id);
			dc_gpac_audio_moov_create(p_aoutf, psz_name);
			p_aoutf->i_first_dts = 0;
		}
		sprintf(psz_name, "%s/%s_%d_gpac.m4s",
							psz_directory, psz_id, i_seg);

		ret = dc_gpac_audio_isom_open_seg(p_aoutf, psz_name);
		return ret;
	default:
		return GF_BAD_PARAM;
	};

	return GF_BAD_PARAM;
}

int dc_audio_muxer_write(AudioOutputFile * p_aoutf, int i_frame_nb) {

	//GF_Err ret;
	switch (p_aoutf->muxer_type) {

	case FFMPEG_AUDIO_MUXER:
		return dc_ffmpeg_audio_muxer_write(p_aoutf);
	case GPAC_AUDIO_MUXER:
	case GPAC_INIT_AUDIO_MUXER:
		if (i_frame_nb % p_aoutf->i_frame_per_frag == 0) {
			gf_isom_start_fragment(p_aoutf->p_isof, 1);

			gf_isom_set_traf_base_media_decode_time(p_aoutf->p_isof, 1,
					p_aoutf->i_first_dts * p_aoutf->i_frame_size);
			p_aoutf->i_first_dts += p_aoutf->i_frame_per_frag;
		}
		dc_gpac_audio_isom_write(p_aoutf);
		if (i_frame_nb % p_aoutf->i_frame_per_frag
				== p_aoutf->i_frame_per_frag - 1) {
			gf_isom_flush_fragments(p_aoutf->p_isof, 1);
		}
		if(i_frame_nb + 1 == p_aoutf->i_frame_per_seg)
			return 1;
		return 0;
	default:
		return GF_BAD_PARAM;
	};

	return GF_BAD_PARAM;
}

int dc_audio_muxer_close(AudioOutputFile * p_aoutf) {

	switch (p_aoutf->muxer_type) {

	case FFMPEG_AUDIO_MUXER:
		return dc_ffmpeg_audio_muxer_close(p_aoutf);
	case GPAC_AUDIO_MUXER:
		dc_gpac_audio_isom_close_seg(p_aoutf);
		return dc_gpac_audio_isom_close(p_aoutf);
	case GPAC_INIT_AUDIO_MUXER:
		return dc_gpac_audio_isom_close_seg(p_aoutf);
	default:
		return GF_BAD_PARAM;
	};

	return GF_BAD_PARAM;
}
