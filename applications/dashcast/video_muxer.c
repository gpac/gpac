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

#include "video_muxer.h"
#include "libavutil/opt.h"

int dc_gpac_video_moov_create(VideoOutputFile * p_voutf, char * psz_name) {

	GF_Err ret;
	//AVStream * p_video_stream =
	//		p_voutf->p_fmt->streams[p_voutf->i_vstream_idx];
	//AVCodecContext * p_video_codec_ctx = p_video_stream->codec;

	AVCodecContext * p_video_codec_ctx = p_voutf->p_codec_ctx;
	GF_AVCConfig *avccfg;
	u32 di;
	u32 track;

	// T0D0: For the moment it is fixed
	//u32 sample_dur = p_voutf->p_codec_ctx->time_base.den;

	avccfg = gf_odf_avc_cfg_new();

	if (!avccfg) {
		fprintf(stderr, "Cannot create AVCConfig\n");
		return -1;
	}

	//int64_t profile = 0;
	//av_opt_get_int(p_voutf->p_codec_ctx->priv_data, "level", AV_OPT_SEARCH_CHILDREN, &profile);

	avccfg->configurationVersion = 1; //0
	//avccfg->AVCProfileIndication = 66; //0
	//avccfg->profile_compatibility = 192; //0
	//avccfg->AVCLevelIndication = 30; //24
	//avccfg->nal_unit_size = 4;

	p_voutf->p_isof = gf_isom_open(psz_name, GF_ISOM_OPEN_WRITE, NULL);

	if (!p_voutf->p_isof) {
		fprintf(stderr, "Cannot open iso file %s\n", psz_name);
		return -1;
	}
	//gf_isom_store_movie_config(p_voutf->p_isof, 0);
	track = gf_isom_new_track(p_voutf->p_isof, 1, GF_ISOM_MEDIA_VISUAL,
			p_video_codec_ctx->time_base.den);

	if (!track) {
		fprintf(stderr, "Cannot create new track\n");
		return -1;
	}

	ret = gf_isom_set_track_enabled(p_voutf->p_isof, 1, 1);
	if (ret != GF_OK) {
		fprintf(stderr, "%s: gf_isom_set_track_enabled\n",
				gf_error_to_string(ret));
		return -1;
	}

	ret = gf_isom_avc_config_new(p_voutf->p_isof, 1, avccfg, NULL, NULL, &di);
	if (ret != GF_OK) {
		fprintf(stderr, "%s: gf_isom_avc_config_new\n",
				gf_error_to_string(ret));
		return -1;
	}

	gf_odf_avc_cfg_del(avccfg);
	//printf("time scale: %d \n",
	//		p_video_codec_ctx->time_base.den);

	ret = gf_isom_avc_set_inband_config(p_voutf->p_isof, track, 1);
	if (ret != GF_OK) {
		fprintf(stderr, "%s: gf_isom_avc_set_inband_config\n",
				gf_error_to_string(ret));
		return -1;
	}

	ret = gf_isom_setup_track_fragment(p_voutf->p_isof, 1, 1, 1, 0, 0, 0, 0);
	if (ret != GF_OK) {
		fprintf(stderr, "%s: gf_isom_setup_track_fragment\n",
				gf_error_to_string(ret));
		return -1;
	}

	//gf_isom_add_track_to_root_od(p_voutf->p_isof,1);

	ret = gf_isom_finalize_for_fragment(p_voutf->p_isof, 1);
	if (ret != GF_OK) {
		fprintf(stderr, "%s: gf_isom_finalize_for_fragment\n",
				gf_error_to_string(ret));
		return -1;
	}

	return 0;
}

int dc_gpac_video_isom_open_seg(VideoOutputFile * p_voutf, char * psz_name) {

	GF_Err ret;

	ret = gf_isom_start_segment(p_voutf->p_isof, psz_name);
	if (ret != GF_OK) {
		fprintf(stderr, "%s: gf_isom_start_segment\n", gf_error_to_string(ret));
		return -1;
	}

//	ret = gf_isom_set_traf_base_media_decode_time(p_voutf->p_isof, 1,
//			p_voutf->first_dts);
//	if (ret != GF_OK) {
//		fprintf(stderr, "%s: gf_isom_set_traf_base_media_decode_time\n",
//				gf_error_to_string(ret));
//		return -1;
//	}
//
//	p_voutf->first_dts += p_voutf->frame_per_segment;

	return 0;
}

int dc_gpac_video_isom_write(VideoOutputFile * p_voutf) {

	GF_Err ret;
	//AVStream * p_video_stream =
	//		p_voutf->p_fmt->streams[p_voutf->i_vstream_idx];
	//AVCodecContext * p_video_codec_ctx = p_video_stream->codec;

	AVCodecContext * p_video_codec_ctx = p_voutf->p_codec_ctx;

	u32 sc_size = 0;
	u32 nalu_size = 0;

	u32 buf_len = p_voutf->i_encoded_frame_size;
	u8 *buf_ptr = p_voutf->p_vbuf;

	GF_BitStream * out_bs = gf_bs_new(NULL, 2 * buf_len, GF_BITSTREAM_WRITE);

	nalu_size = gf_media_nalu_next_start_code(buf_ptr, buf_len, &sc_size);

	if (nalu_size != 0) {
		gf_bs_write_u32(out_bs, nalu_size);
		gf_bs_write_data(out_bs, (const char*) buf_ptr, nalu_size);
	}

	if (sc_size) {
		buf_ptr += (nalu_size + sc_size);
		buf_len -= (nalu_size + sc_size);
	}

	while (buf_len) {

		nalu_size = gf_media_nalu_next_start_code(buf_ptr, buf_len, &sc_size);

		if (nalu_size != 0) {
			gf_bs_write_u32(out_bs, nalu_size);
			gf_bs_write_data(out_bs, (const char*) buf_ptr, nalu_size);
		}

		buf_ptr += nalu_size;

		if (!sc_size || (buf_len < nalu_size + sc_size))
			break;
		buf_len -= nalu_size + sc_size;
		buf_ptr += sc_size;

	}

	gf_bs_get_content(out_bs, &p_voutf->p_sample->data,
			&p_voutf->p_sample->dataLength);
	//p_voutf->p_sample->data = //(char *) (p_voutf->p_vbuf + nalu_size + sc_size);
	//p_voutf->p_sample->dataLength = //p_voutf->i_encoded_frame_size - (sc_size + nalu_size);

	p_voutf->p_sample->DTS = p_video_codec_ctx->coded_frame->pts;
	p_voutf->p_sample->IsRAP = p_video_codec_ctx->coded_frame->key_frame;
	//printf("RAP %d , DTS %ld \n", p_voutf->p_sample->IsRAP, p_voutf->p_sample->DTS);

	ret = gf_isom_fragment_add_sample(p_voutf->p_isof, 1, p_voutf->p_sample, 1,
			1, 0, 0, 0);

	if (ret != GF_OK) {
		fprintf(stderr, "%s: gf_isom_fragment_add_sample\n",
				gf_error_to_string(ret));
		return -1;
	}

	return 0;
}

int dc_gpac_video_isom_close_seg(VideoOutputFile * p_voutf) {

	GF_Err ret;

	ret = gf_isom_close_segment(p_voutf->p_isof, 0, 0, 0, 0, 0, 1, p_voutf->i_seg_marker, NULL,
			NULL);
	if (ret != GF_OK) {
		fprintf(stderr, "%s: gf_isom_close_segment\n", gf_error_to_string(ret));
		return -1;
	}

	return 0;
}

int dc_gpac_video_isom_close(VideoOutputFile * p_voutf) {

	GF_Err ret;

	ret = gf_isom_close(p_voutf->p_isof);
	if (ret != GF_OK) {
		fprintf(stderr, "%s: gf_isom_close\n", gf_error_to_string(ret));
		return -1;
	}

	return 0;
}

int dc_raw_h264_open(VideoOutputFile * p_voutf, char * psz_name) {

	p_voutf->p_file = fopen(psz_name, "w");
	return 0;
}

int dc_raw_h264_write(VideoOutputFile * p_voutf) {

	fwrite(p_voutf->p_vbuf, p_voutf->i_encoded_frame_size, 1, p_voutf->p_file);
	return 0;
}

int dc_raw_h264_close(VideoOutputFile * p_voutf) {

	fclose(p_voutf->p_file);
	return 0;
}

int dc_ffmpeg_video_muxer_open(VideoOutputFile * p_voutf, char * psz_name) {

	AVStream * p_video_stream;
	AVOutputFormat * p_output_fmt;

	AVCodecContext * p_video_codec_ctx = p_voutf->p_codec_ctx;
	p_voutf->p_fmt = NULL;

//	p_voutf->i_vbr = p_vconf->i_bitrate;
//	p_voutf->i_vfr = p_vconf->i_framerate;
//	p_voutf->i_width = p_vconf->i_width;
//	p_voutf->i_height = p_vconf->i_height;
//	strcpy(p_voutf->psz_name, p_vconf->psz_name);
//	strcpy(p_voutf->psz_codec, p_vconf->psz_codec);

	/* Find output format */
	p_output_fmt = av_guess_format(NULL, psz_name, NULL);
	if (!p_output_fmt) {
		fprintf(stderr, "Cannot find suitable output format\n");
		return -1;
	}

	p_voutf->p_fmt = avformat_alloc_context();
	if (!p_voutf->p_fmt) {
		fprintf(stderr, "Cannot allocate memory for pOutVideoFormatCtx\n");
		return -1;
	}

	p_voutf->p_fmt->oformat = p_output_fmt;
	strcpy(p_voutf->p_fmt->filename, psz_name);

	/* Open the output file */
	if (!(p_output_fmt->flags & AVFMT_NOFILE)) {
		if (avio_open(&p_voutf->p_fmt->pb, psz_name, URL_WRONLY) < 0) {
			fprintf(stderr, "Cannot not open '%s'\n", psz_name);
			return -1;
		}
	}

	p_video_stream = avformat_new_stream(p_voutf->p_fmt,
			p_voutf->p_codec);
	if (!p_video_stream) {
		fprintf(stderr, "Cannot create output video stream\n");
		return -1;
	}

	//p_video_stream->codec = p_voutf->p_codec_ctx;

	p_video_stream->codec->codec_id = p_voutf->p_codec->id;
	p_video_stream->codec->codec_type = AVMEDIA_TYPE_VIDEO;
	p_video_stream->codec->bit_rate = p_video_codec_ctx->bit_rate; //p_voutf->p_vdata->i_bitrate;
	p_video_stream->codec->width = p_video_codec_ctx->width; //p_voutf->p_vdata->i_width;
	p_video_stream->codec->height = p_video_codec_ctx->height; //p_voutf->p_vdata->i_height;
	{
		AVRational time_base; 
		time_base.num = p_video_codec_ctx->time_base.num;
		time_base.den = p_video_codec_ctx->time_base.den;
		p_video_stream->codec->time_base = time_base;
	}
	p_video_stream->codec->pix_fmt = PIX_FMT_YUV420P;
	p_video_stream->codec->gop_size = p_video_codec_ctx->time_base.den; //p_voutf->p_vdata->i_framerate;

	av_opt_set(p_video_stream->codec->priv_data, "preset", "ultrafast", 0);
	av_opt_set(p_video_stream->codec->priv_data, "tune", "zerolatency", 0);

	/* open the video codec */
	if (avcodec_open2(p_video_stream->codec, p_voutf->p_codec, NULL) < 0) {
		fprintf(stderr, "Cannot open output video codec\n");
		return -1;
	}

	avformat_write_header(p_voutf->p_fmt, NULL);

	return 0;
}

int dc_ffmpeg_video_muxer_write(VideoOutputFile * p_voutf) {

	AVPacket pkt;

	AVStream * p_video_stream = p_voutf->p_fmt->streams[p_voutf->i_vstream_idx];
	AVCodecContext * p_video_codec_ctx = p_video_stream->codec;

	av_init_packet(&pkt);

	if (p_video_codec_ctx->coded_frame->pts != AV_NOPTS_VALUE) {
		pkt.pts = av_rescale_q(p_video_codec_ctx->coded_frame->pts,
				p_video_codec_ctx->time_base, p_video_stream->time_base);
	}

	if (p_video_codec_ctx->coded_frame->key_frame)
		pkt.flags |= AV_PKT_FLAG_KEY;

	pkt.stream_index = p_video_stream->index;
	pkt.data = p_voutf->p_vbuf;
	pkt.size = p_voutf->i_encoded_frame_size;

	// write the compressed frame in the media file
	if (av_interleaved_write_frame(p_voutf->p_fmt, &pkt) != 0) {
		fprintf(stderr, "Writing frame is not successful\n");
		return -1;
	}

	av_free_packet(&pkt);

	return 0;
}

int dc_ffmpeg_video_muxer_close(VideoOutputFile * p_voutf) {

	u32 i;

	av_write_trailer(p_voutf->p_fmt);

	avio_close(p_voutf->p_fmt->pb);

	// free the streams
	for (i = 0; i < p_voutf->p_fmt->nb_streams; i++) {
		avcodec_close(p_voutf->p_fmt->streams[i]->codec);
		av_freep(&p_voutf->p_fmt->streams[i]->info);
	}

	//p_voutf->p_fmt->streams[p_voutf->i_vstream_idx]->codec = NULL;
	avformat_free_context(p_voutf->p_fmt);

	return 0;
}

int dc_video_muxer_init(VideoOutputFile * p_voutf, VideoData * p_vdata,
		VideoMuxerType muxer_type, int frame_per_segment,
		int frame_per_fragment, u32 seg_marker, int gdr) {

	char name[256];
	sprintf(name, "video encoder %s", p_vdata->psz_name);
	dc_consumer_init(&p_voutf->vcon, VIDEO_CB_SIZE, name);

	p_voutf->p_sample = gf_isom_sample_new();
	p_voutf->p_isof = NULL;
	p_voutf->muxer_type = muxer_type;

	p_voutf->frame_per_segment = frame_per_segment;
	p_voutf->frame_per_fragment = frame_per_fragment;

	p_voutf->i_seg_marker = seg_marker;
	p_voutf->i_gdr = gdr;

	return 0;
}

int dc_video_muxer_free(VideoOutputFile * p_voutf) {

	if (p_voutf->p_isof != NULL) {
		gf_isom_close(p_voutf->p_isof);
	}

	gf_isom_sample_del(&p_voutf->p_sample);

	return 0;
}

GF_Err dc_video_muxer_open(VideoOutputFile * p_voutf, char * psz_directory,
		char * psz_id, int i_seg) {

	char psz_name[256];

	switch (p_voutf->muxer_type) {

	case FFMPEG_VIDEO_MUXER:
		sprintf(psz_name, "%s/%s_%d_ffmpeg.mp4", psz_directory, psz_id, i_seg);
		return dc_ffmpeg_video_muxer_open(p_voutf, psz_name);
	case RAW_VIDEO_H264:
		sprintf(psz_name, "%s/%s_%d.264", psz_directory, psz_id, i_seg);
		return dc_raw_h264_open(p_voutf, psz_name);
	case GPAC_VIDEO_MUXER:
		sprintf(psz_name, "%s/%s_%d_gpac.mp4", psz_directory, psz_id, i_seg);
		dc_gpac_video_moov_create(p_voutf, psz_name);
		return dc_gpac_video_isom_open_seg(p_voutf, NULL);
	case GPAC_INIT_VIDEO_MUXER:
		if (i_seg == 0) {
			sprintf(psz_name, "%s/%s_init_gpac.mp4", psz_directory, psz_id);
			dc_gpac_video_moov_create(p_voutf, psz_name);
			p_voutf->first_dts = 0;
		}
		sprintf(psz_name, "%s/%s_%d_gpac.m4s", psz_directory, psz_id, i_seg);
		return dc_gpac_video_isom_open_seg(p_voutf, psz_name);
	default:
		return GF_BAD_PARAM;
	};

	return -2;
}

int dc_video_muxer_write(VideoOutputFile * p_voutf, int i_frame_nb) {

	//GF_Err ret;
	switch (p_voutf->muxer_type) {

	case FFMPEG_VIDEO_MUXER:
		return dc_ffmpeg_video_muxer_write(p_voutf);
	case RAW_VIDEO_H264:
		return dc_raw_h264_write(p_voutf);
	case GPAC_VIDEO_MUXER:
	case GPAC_INIT_VIDEO_MUXER:
		if (i_frame_nb % p_voutf->frame_per_fragment == 0) {
			gf_isom_start_fragment(p_voutf->p_isof, 1);

			gf_isom_set_traf_base_media_decode_time(p_voutf->p_isof, 1,
					p_voutf->first_dts);
			p_voutf->first_dts += p_voutf->frame_per_fragment;
		}
		dc_gpac_video_isom_write(p_voutf);
		if (i_frame_nb % p_voutf->frame_per_fragment == p_voutf->frame_per_fragment - 1) {
			gf_isom_flush_fragments(p_voutf->p_isof, 1);

			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DashCast] Flushed fragment to disk at UTC "LLU" ms - last coded frame PTS %d\n", gf_net_get_utc(), p_voutf->p_codec_ctx->coded_frame->pts));
		}
		if (i_frame_nb + 1 == p_voutf->frame_per_segment)
			return 1;
		return 0;
	default:
		return -2;
	};

	return -2;
}

int dc_video_muxer_close(VideoOutputFile * p_voutf) {

	switch (p_voutf->muxer_type) {

	case FFMPEG_VIDEO_MUXER:
		return dc_ffmpeg_video_muxer_close(p_voutf);
	case RAW_VIDEO_H264:
		return dc_raw_h264_close(p_voutf);
	case GPAC_VIDEO_MUXER:
		dc_gpac_video_isom_close_seg(p_voutf);
		return dc_gpac_video_isom_close(p_voutf);
	case GPAC_INIT_VIDEO_MUXER:
		return dc_gpac_video_isom_close_seg(p_voutf);
	default:
		return -2;
	};

	return -2;
}
