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
#include <gpac/network.h>


/**
 * A function which takes FFmpeg H264 extradata (SPS/PPS) and bring them ready to be pushed to the MP4 muxer.
 * @param extradata
 * @param extradata_size
 * @param dstcfg
 * @returns GF_OK is the extradata was parsed and is valid, other values otherwise.
 */
static GF_Err avc_import_ffextradata(const u8 *extradata, const u64 extradata_size, GF_AVCConfig *dstcfg)
{
#ifdef GPAC_DISABLE_AV_PARSERS
	return GF_OK;
#else
	u8 nal_size;
	AVCState avc;
	GF_BitStream *bs;
	if (!extradata || (extradata_size < sizeof(u32)))
		return GF_BAD_PARAM;
	bs = gf_bs_new((const char *) extradata, extradata_size, GF_BITSTREAM_READ);
	if (!bs)
		return GF_BAD_PARAM;
	if (gf_bs_read_u32(bs) != 0x00000001) {
		gf_bs_del(bs);
		return GF_BAD_PARAM;
	}

	//SPS
	{
		s32 idx;
		char *buffer = NULL;
		const u64 nal_start = 4;
		nal_size = gf_media_nalu_next_start_code_bs(bs);
		if (nal_start + nal_size > extradata_size) {
			gf_bs_del(bs);
			return GF_BAD_PARAM;
		}
		buffer = (char*)gf_malloc(nal_size);
		gf_bs_read_data(bs, buffer, nal_size);
		gf_bs_seek(bs, nal_start);
		if ((gf_bs_read_u8(bs) & 0x1F) != GF_AVC_NALU_SEQ_PARAM) {
			gf_bs_del(bs);
			gf_free(buffer);
			return GF_BAD_PARAM;
		}

		idx = gf_media_avc_read_sps(buffer, nal_size, &avc, 0, NULL);
		if (idx < 0) {
			gf_bs_del(bs);
			gf_free(buffer);
			return GF_BAD_PARAM;
		}

		dstcfg->configurationVersion = 1;
		dstcfg->profile_compatibility = avc.sps[idx].prof_compat;
		dstcfg->AVCProfileIndication = avc.sps[idx].profile_idc;
		dstcfg->AVCLevelIndication = avc.sps[idx].level_idc;
		dstcfg->chroma_format = avc.sps[idx].chroma_format;
		dstcfg->luma_bit_depth = 8 + avc.sps[idx].luma_bit_depth_m8;
		dstcfg->chroma_bit_depth = 8 + avc.sps[idx].chroma_bit_depth_m8;

		{
			GF_AVCConfigSlot *slc = (GF_AVCConfigSlot*)gf_malloc(sizeof(GF_AVCConfigSlot));
			slc->size = nal_size;
			slc->id = idx;
			slc->data = buffer;
			gf_list_add(dstcfg->sequenceParameterSets, slc);
		}
	}

	//PPS
	{
		s32 idx;
		char *buffer = NULL;
		const u64 nal_start = 4 + nal_size + 4;
		gf_bs_seek(bs, nal_start);
		nal_size = gf_media_nalu_next_start_code_bs(bs);
		if (nal_start + nal_size > extradata_size) {
			gf_bs_del(bs);
			return GF_BAD_PARAM;
		}
		buffer = (char*)gf_malloc(nal_size);
		gf_bs_read_data(bs, buffer, nal_size);
		gf_bs_seek(bs, nal_start);
		if ((gf_bs_read_u8(bs) & 0x1F) != GF_AVC_NALU_PIC_PARAM) {
			gf_bs_del(bs);
			gf_free(buffer);
			return GF_BAD_PARAM;
		}

		idx = gf_media_avc_read_pps(buffer, nal_size, &avc);
		if (idx < 0) {
			gf_bs_del(bs);
			gf_free(buffer);
			return GF_BAD_PARAM;
		}

		{
			GF_AVCConfigSlot *slc = (GF_AVCConfigSlot*)gf_malloc(sizeof(GF_AVCConfigSlot));
			slc->size = nal_size;
			slc->id = idx;
			slc->data = buffer;
			gf_list_add(dstcfg->pictureParameterSets, slc);
		}
	}

	gf_bs_del(bs);
	return GF_OK;
#endif
}

/**
 * A function which takes FFmpeg H265 extradata (SPS/PPS) and bring them ready to be pushed to the MP4 muxer.
 * @param extradata
 * @param extradata_size
 * @param dstcfg
 * @returns GF_OK is the extradata was parsed and is valid, other values otherwise.
 */
static GF_Err hevc_import_ffextradata(const u8 *extradata, const u64 extradata_size, GF_HEVCConfig *dst_cfg)
{
#ifdef GPAC_DISABLE_AV_PARSERS
	return GF_OK;
#else
	HEVCState hevc;
	GF_HEVCParamArray *vpss = NULL, *spss = NULL, *ppss = NULL, *seis = NULL;
	GF_BitStream *bs;
	char *buffer = NULL;
	u32 buffer_size = 0;
	if (!extradata || (extradata_size < sizeof(u32)))
		return GF_BAD_PARAM;
	bs = gf_bs_new((const char *) extradata, extradata_size, GF_BITSTREAM_READ);
	if (!bs)
		return GF_BAD_PARAM;

	memset(&hevc, 0, sizeof(HEVCState));
	hevc.sps_active_idx = -1;

	while (gf_bs_available(bs)) {
		s32 idx;
		GF_AVCConfigSlot *slc;
		u8 nal_unit_type, temporal_id, layer_id;
		u64 nal_start, start_code;
		u32 nal_size;

		start_code = gf_bs_read_u32(bs);
		if (start_code>>8 == 0x000001) {
			nal_start = gf_bs_get_position(bs) - 1;
			gf_bs_seek(bs, nal_start);
			start_code = 1;
		}
		if (start_code != 0x00000001) {
			gf_bs_del(bs);
			if (buffer) gf_free(buffer);
			if (vpss && spss && ppss) return GF_OK;
			return GF_BAD_PARAM;
		}
		nal_start = gf_bs_get_position(bs);
		nal_size = gf_media_nalu_next_start_code_bs(bs);
		if (nal_start + nal_size > extradata_size) {
			gf_bs_del(bs);
			return GF_BAD_PARAM;
		}

		if (nal_size > buffer_size) {
			buffer = (char*)gf_realloc(buffer, nal_size);
			buffer_size = nal_size;
		}
		gf_bs_read_data(bs, buffer, nal_size);
		gf_bs_seek(bs, nal_start);

		gf_media_hevc_parse_nalu(bs, &hevc, &nal_unit_type, &temporal_id, &layer_id);
		if (layer_id) {
			gf_bs_del(bs);
			gf_free(buffer);
			return GF_BAD_PARAM;
		}

		switch (nal_unit_type) {
		case GF_HEVC_NALU_VID_PARAM:
			idx = gf_media_hevc_read_vps(buffer, nal_size , &hevc);
			if (idx < 0) {
				gf_bs_del(bs);
				gf_free(buffer);
				return GF_BAD_PARAM;
			}

			assert(hevc.vps[idx].state == 1); //we don't expect multiple VPS
			if (hevc.vps[idx].state == 1) {
				hevc.vps[idx].state = 2;
				hevc.vps[idx].crc = gf_crc_32(buffer, nal_size);

				dst_cfg->avgFrameRate = hevc.vps[idx].rates[0].avg_pic_rate;
				dst_cfg->constantFrameRate = hevc.vps[idx].rates[0].constand_pic_rate_idc;
				dst_cfg->numTemporalLayers = hevc.vps[idx].max_sub_layers;
				dst_cfg->temporalIdNested = hevc.vps[idx].temporal_id_nesting;

				if (!vpss) {
					GF_SAFEALLOC(vpss, GF_HEVCParamArray);
					if (vpss) {
						vpss->nalus = gf_list_new();
						gf_list_add(dst_cfg->param_array, vpss);
						vpss->array_completeness = 1;
						vpss->type = GF_HEVC_NALU_VID_PARAM;
					}
				}

				slc = (GF_AVCConfigSlot*)gf_malloc(sizeof(GF_AVCConfigSlot));
				if (slc) {
					slc->size = nal_size;
					slc->id = idx;
					slc->data = (char*)gf_malloc(sizeof(char)*slc->size);
					if (slc->data)
						memcpy(slc->data, buffer, sizeof(char)*slc->size);

					if (vpss)
						gf_list_add(vpss->nalus, slc);
				}
			}
			break;
		case GF_HEVC_NALU_SEQ_PARAM:
			idx = gf_media_hevc_read_sps(buffer, nal_size, &hevc);
			if (idx < 0) {
				gf_bs_del(bs);
				gf_free(buffer);
				return GF_BAD_PARAM;
			}

			assert(!(hevc.sps[idx].state & AVC_SPS_DECLARED)); //we don't expect multiple SPS
			if ((hevc.sps[idx].state & AVC_SPS_PARSED) && !(hevc.sps[idx].state & AVC_SPS_DECLARED)) {
				hevc.sps[idx].state |= AVC_SPS_DECLARED;
				hevc.sps[idx].crc = gf_crc_32(buffer, nal_size);
			}

			dst_cfg->configurationVersion = 1;
			dst_cfg->profile_space = hevc.sps[idx].ptl.profile_space;
			dst_cfg->tier_flag = hevc.sps[idx].ptl.tier_flag;
			dst_cfg->profile_idc = hevc.sps[idx].ptl.profile_idc;
			dst_cfg->general_profile_compatibility_flags = hevc.sps[idx].ptl.profile_compatibility_flag;
			dst_cfg->progressive_source_flag = hevc.sps[idx].ptl.general_progressive_source_flag;
			dst_cfg->interlaced_source_flag = hevc.sps[idx].ptl.general_interlaced_source_flag;
			dst_cfg->non_packed_constraint_flag = hevc.sps[idx].ptl.general_non_packed_constraint_flag;
			dst_cfg->frame_only_constraint_flag = hevc.sps[idx].ptl.general_frame_only_constraint_flag;

			dst_cfg->constraint_indicator_flags = hevc.sps[idx].ptl.general_reserved_44bits;
			dst_cfg->level_idc = hevc.sps[idx].ptl.level_idc;

			dst_cfg->chromaFormat = hevc.sps[idx].chroma_format_idc;
			dst_cfg->luma_bit_depth = hevc.sps[idx].bit_depth_luma;
			dst_cfg->chroma_bit_depth = hevc.sps[idx].bit_depth_chroma;

			if (!spss) {
				GF_SAFEALLOC(spss, GF_HEVCParamArray);
				if (spss) {
					spss->nalus = gf_list_new();
					gf_list_add(dst_cfg->param_array, spss);
					spss->array_completeness = 1;
					spss->type = GF_HEVC_NALU_SEQ_PARAM;
				}
			}

			slc = (GF_AVCConfigSlot*)gf_malloc(sizeof(GF_AVCConfigSlot));
			if (slc) {
				slc->size = nal_size;
				slc->id = idx;
				slc->data = (char*)gf_malloc(sizeof(char)*slc->size);
				if (slc->data)
					memcpy(slc->data, buffer, sizeof(char)*slc->size);
				if (spss)
					gf_list_add(spss->nalus, slc);
			}
			break;
		case GF_HEVC_NALU_PIC_PARAM:
			idx = gf_media_hevc_read_pps(buffer, nal_size, &hevc);
			if (idx < 0) {
				gf_bs_del(bs);
				gf_free(buffer);
				return GF_BAD_PARAM;
			}

			assert(hevc.pps[idx].state == 1); //we don't expect multiple PPS
			if (hevc.pps[idx].state == 1) {
				hevc.pps[idx].state = 2;
				hevc.pps[idx].crc = gf_crc_32(buffer, nal_size);

				if (!ppss) {
					GF_SAFEALLOC(ppss, GF_HEVCParamArray);
					if (ppss) {
						ppss->nalus = gf_list_new();
						gf_list_add(dst_cfg->param_array, ppss);
						ppss->array_completeness = 1;
						ppss->type = GF_HEVC_NALU_PIC_PARAM;
					}
				}

				slc = (GF_AVCConfigSlot*)gf_malloc(sizeof(GF_AVCConfigSlot));
				if (slc) {
					slc->size = nal_size;
					slc->id = idx;
					slc->data = (char*)gf_malloc(sizeof(char)*slc->size);
					if (slc->data)
						memcpy(slc->data, buffer, sizeof(char)*slc->size);

					if (ppss)
						gf_list_add(ppss->nalus, slc);
				}
			}
			break;
		case GF_HEVC_NALU_SEI_PREFIX:
			if (!seis) {
				GF_SAFEALLOC(seis, GF_HEVCParamArray);
				if (seis) {
					seis->nalus = gf_list_new();
					seis->array_completeness = 0;
					seis->type = GF_HEVC_NALU_SEI_PREFIX;
				}
			}
			slc = (GF_AVCConfigSlot*)gf_malloc(sizeof(GF_AVCConfigSlot));
			if (slc) {
				slc->size = nal_size;
				slc->data = (char*)gf_malloc(sizeof(char)*slc->size);
				if (slc->data)
					memcpy(slc->data, buffer, sizeof(char)*slc->size);
				if (seis)
					gf_list_add(seis->nalus, slc);
			}
			break;
		default:
			break;
		}

		if (gf_bs_seek(bs, nal_start+nal_size)) {
			assert(nal_start+nal_size <= gf_bs_get_size(bs));
			break;
		}
	}

	gf_bs_del(bs);
	if (buffer) gf_free(buffer);

	return GF_OK;
#endif
}

#ifndef GPAC_DISABLE_ISOM

static GF_Err dc_gpac_video_write_config(VideoOutputFile *video_output_file, u32 *di, u32 track) {
	GF_Err ret;
	if (video_output_file->codec_ctx->codec_id == CODEC_ID_H264) {
		GF_AVCConfig *avccfg;
		avccfg = gf_odf_avc_cfg_new();
		if (!avccfg) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot create AVCConfig\n"));
			return GF_OUT_OF_MEM;
		}

		ret = avc_import_ffextradata(video_output_file->codec_ctx->extradata, video_output_file->codec_ctx->extradata_size, avccfg);
		if (ret != GF_OK) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot parse AVC/H264 SPS/PPS\n"));
			gf_odf_avc_cfg_del(avccfg);
			return ret;
		}

		ret = gf_isom_avc_config_new(video_output_file->isof, track, avccfg, NULL, NULL, di);
		if (ret != GF_OK) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("%s: gf_isom_avc_config_new\n", gf_error_to_string(ret)));
			return ret;
		}

		gf_odf_avc_cfg_del(avccfg);

		//inband SPS/PPS
		if (video_output_file->muxer_type == GPAC_INIT_VIDEO_MUXER_AVC3) {
			ret = gf_isom_avc_set_inband_config(video_output_file->isof, track, 1);
			if (ret != GF_OK) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("%s: gf_isom_avc_set_inband_config\n", gf_error_to_string(ret)));
				return ret;
			}
		}
	} else if (!strcmp(video_output_file->codec_ctx->codec->name, "libx265")) { //FIXME CODEC_ID_HEVC would break on old releases
		GF_HEVCConfig *hevccfg = gf_odf_hevc_cfg_new();
		if (!hevccfg) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot create HEVCConfig\n"));
			return GF_OUT_OF_MEM;
		}

		ret = hevc_import_ffextradata(video_output_file->codec_ctx->extradata, video_output_file->codec_ctx->extradata_size, hevccfg);
		if (ret != GF_OK) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot parse HEVC/H265 SPS/PPS\n"));
			gf_odf_hevc_cfg_del(hevccfg);
			return ret;
		}

		ret = gf_isom_hevc_config_new(video_output_file->isof, track, hevccfg, NULL, NULL, di);
		if (ret != GF_OK) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("%s: gf_isom_hevc_config_new\n", gf_error_to_string(ret)));
			return ret;
		}

		gf_odf_hevc_cfg_del(hevccfg);

		//inband SPS/PPS
		if (video_output_file->muxer_type == GPAC_INIT_VIDEO_MUXER_AVC3) {
			ret = gf_isom_hevc_set_inband_config(video_output_file->isof, track, 1);
			if (ret != GF_OK) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("%s: gf_isom_hevc_set_inband_config\n", gf_error_to_string(ret)));
				return ret;
			}
		}
	}

	return GF_OK;
}

int dc_gpac_video_moov_create(VideoOutputFile *video_output_file, char *filename)
{
	GF_Err ret;
	AVCodecContext *video_codec_ctx = video_output_file->codec_ctx;
	u32 di=1, track;

	//TODO: For the moment it is fixed
	//u32 sample_dur = video_output_file->codec_ctx->time_base.den;

	//int64_t profile = 0;
	//av_opt_get_int(video_output_file->codec_ctx->priv_data, "level", AV_OPT_SEARCH_CHILDREN, &profile);

	video_output_file->isof = gf_isom_open(filename, GF_ISOM_OPEN_WRITE, NULL);
	if (!video_output_file->isof) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot open iso file %s\n", filename));
		return -1;
	}
	//gf_isom_store_movie_config(video_output_file->isof, 0);
	track = gf_isom_new_track(video_output_file->isof, 0, GF_ISOM_MEDIA_VISUAL, video_codec_ctx->time_base.den);
	video_output_file->trackID = gf_isom_get_track_id(video_output_file->isof, track);

	video_output_file->timescale = video_codec_ctx->time_base.den;
	if (!video_output_file->frame_dur)
		video_output_file->frame_dur = video_codec_ctx->time_base.num;

	if (!track) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot create new track\n"));
		return -1;
	}

	ret = gf_isom_set_track_enabled(video_output_file->isof, track, 1);
	if (ret != GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("%s: gf_isom_set_track_enabled\n", gf_error_to_string(ret)));
		return -1;
	}

	ret = dc_gpac_video_write_config(video_output_file, &di, track);
	if (ret != GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("%s: dc_gpac_video_write_config\n", gf_error_to_string(ret)));
		return -1;
	}

	gf_isom_set_visual_info(video_output_file->isof, track, di, video_codec_ctx->width, video_codec_ctx->height);
	gf_isom_set_sync_table(video_output_file->isof, track);

	ret = gf_isom_setup_track_fragment(video_output_file->isof, track, 1, video_output_file->use_source_timing ? (u32) video_output_file->frame_dur : 1, 0, 0, 0, 0);
	if (ret != GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("%s: gf_isom_setup_track_fragment\n", gf_error_to_string(ret)));
		return -1;
	}

	ret = gf_isom_finalize_for_fragment(video_output_file->isof, track);
	if (ret != GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("%s: gf_isom_finalize_for_fragment\n", gf_error_to_string(ret)));
		return -1;
	}

	ret = gf_media_get_rfc_6381_codec_name(video_output_file->isof, track, video_output_file->video_data_conf->codec6381, GF_FALSE, GF_FALSE);
	if (ret != GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("%s: gf_isom_finalize_for_fragment\n", gf_error_to_string(ret)));
		return -1;
	}

	return 0;
}

int dc_gpac_video_isom_open_seg(VideoOutputFile *video_output_file, char *filename)
{
	GF_Err ret;
	ret = gf_isom_start_segment(video_output_file->isof, filename, 1);
	if (ret != GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("%s: gf_isom_start_segment\n", gf_error_to_string(ret)));
		return -1;
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DashCast] Opening new segment %s at UTC "LLU" ms\n", filename, gf_net_get_utc() ));
	return 0;
}

int dc_gpac_video_isom_write(VideoOutputFile *video_output_file)
{
	GF_Err ret;
	AVCodecContext *video_codec_ctx = video_output_file->codec_ctx;

	u32 sc_size = 0;
	u32 nalu_size = 0;

	u32 buf_len = video_output_file->encoded_frame_size;
	u8 *buf_ptr = video_output_file->vbuf;

	GF_BitStream *out_bs = gf_bs_new(NULL, 2 * buf_len, GF_BITSTREAM_WRITE);
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

	gf_bs_get_content(out_bs, &video_output_file->sample->data, &video_output_file->sample->dataLength);
	//video_output_file->sample->data = //(char *) (video_output_file->vbuf + nalu_size + sc_size);
	//video_output_file->sample->dataLength = //video_output_file->encoded_frame_size - (sc_size + nalu_size);

	video_output_file->sample->DTS = video_codec_ctx->coded_frame->pkt_dts;
	video_output_file->sample->CTS_Offset = (s32) (video_codec_ctx->coded_frame->pts - video_output_file->sample->DTS);
	video_output_file->sample->IsRAP = video_codec_ctx->coded_frame->key_frame;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("Isom Write: RAP %d , DTS "LLD" CTS offset %d \n", video_output_file->sample->IsRAP, video_output_file->sample->DTS, video_output_file->sample->CTS_Offset));

	ret = gf_isom_fragment_add_sample(video_output_file->isof, video_output_file->trackID, video_output_file->sample, 1, video_output_file->use_source_timing ? (u32) video_output_file->frame_dur : 1, 0, 0, 0);
	if (ret != GF_OK) {
		gf_bs_del(out_bs);
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("%s: gf_isom_fragment_add_sample\n", gf_error_to_string(ret)));
		return -1;
	}

	//free data but keep sample structure alive
	gf_free(video_output_file->sample->data);
	video_output_file->sample->data = NULL;
	video_output_file->sample->dataLength = 0;

	gf_bs_del(out_bs);
	return 0;
}

int dc_gpac_video_isom_close_seg(VideoOutputFile *video_output_file)
{
	GF_Err ret;
	ret = gf_isom_close_segment(video_output_file->isof, 0, 0, 0, 0, 0, 0, 1, video_output_file->seg_marker, NULL, NULL);
	if (ret != GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("%s: gf_isom_close_segment\n", gf_error_to_string(ret)));
		return -1;
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DashCast] Rep %s Closing segment at UTC "LLU" ms\n", video_output_file->rep_id, gf_net_get_utc() ));

	return 0;
}

int dc_gpac_video_isom_close(VideoOutputFile *video_output_file)
{
	GF_Err ret;
	ret = gf_isom_close(video_output_file->isof);
	if (ret != GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("%s: gf_isom_close\n", gf_error_to_string(ret)));
		return -1;
	}

	return 0;
}

#endif


int dc_raw_h264_open(VideoOutputFile *video_output_file, char *filename)
{
	video_output_file->file = gf_fopen(filename, "w");
	return 0;
}

int dc_raw_h264_write(VideoOutputFile *video_output_file)
{
	fwrite(video_output_file->vbuf, video_output_file->encoded_frame_size, 1, video_output_file->file);
	return 0;
}

int dc_raw_h264_close(VideoOutputFile *video_output_file)
{
	gf_fclose(video_output_file->file);
	return 0;
}

int dc_ffmpeg_video_muxer_open(VideoOutputFile *video_output_file, char *filename)
{
	AVStream *video_stream;
	AVOutputFormat *output_fmt;

	AVCodecContext *video_codec_ctx = video_output_file->codec_ctx;
	video_output_file->av_fmt_ctx = NULL;

//	video_output_file->vbr = video_data_conf->bitrate;
//	video_output_file->vfr = video_data_conf->framerate;
//	video_output_file->width = video_data_conf->width;
//	video_output_file->height = video_data_conf->height;
//	strcpy(video_output_file->filename, video_data_conf->filename);
//	strcpy(video_output_file->codec, video_data_conf->codec);

	/* Find output format */
	output_fmt = av_guess_format(NULL, filename, NULL);
	if (!output_fmt) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot find suitable output format\n"));
		return -1;
	}

	video_output_file->av_fmt_ctx = avformat_alloc_context();
	if (!video_output_file->av_fmt_ctx) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot allocate memory for pOutVideoFormatCtx\n"));
		return -1;
	}

	video_output_file->av_fmt_ctx->oformat = output_fmt;
	strcpy(video_output_file->av_fmt_ctx->filename, filename);

	/* Open the output file */
	if (!(output_fmt->flags & AVFMT_NOFILE)) {
		if (avio_open(&video_output_file->av_fmt_ctx->pb, filename, URL_WRONLY) < 0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot not open '%s'\n", filename));
			return -1;
		}
	}

	video_stream = avformat_new_stream(video_output_file->av_fmt_ctx,
	                                   video_output_file->codec);
	if (!video_stream) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot create output video stream\n"));
		return -1;
	}

	//video_stream->codec = video_output_file->codec_ctx;

	video_stream->codec->codec_id = video_output_file->codec->id;
	video_stream->codec->codec_type = AVMEDIA_TYPE_VIDEO;
	video_stream->codec->bit_rate = video_codec_ctx->bit_rate; //video_output_file->video_data_conf->bitrate;
	video_stream->codec->width = video_codec_ctx->width; //video_output_file->video_data_conf->width;
	video_stream->codec->height = video_codec_ctx->height; //video_output_file->video_data_conf->height;

	video_stream->codec->time_base = video_codec_ctx->time_base;

	video_stream->codec->pix_fmt = PIX_FMT_YUV420P;
	video_stream->codec->gop_size = video_codec_ctx->time_base.den; //video_output_file->video_data_conf->framerate;

	av_opt_set(video_stream->codec->priv_data, "preset", "ultrafast", 0);
	av_opt_set(video_stream->codec->priv_data, "tune", "zerolatency", 0);

	/* open the video codec */
	if (avcodec_open2(video_stream->codec, video_output_file->codec, NULL) < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot open output video codec\n"));
		return -1;
	}

	avformat_write_header(video_output_file->av_fmt_ctx, NULL);

	video_output_file->timescale = video_codec_ctx->time_base.den;
	return 0;
}

int dc_ffmpeg_video_muxer_write(VideoOutputFile *video_output_file)
{
	AVPacket pkt;
	AVStream *video_stream = video_output_file->av_fmt_ctx->streams[video_output_file->vstream_idx];
	AVCodecContext *video_codec_ctx = video_stream->codec;

	av_init_packet(&pkt);
	pkt.data = NULL;
	pkt.size = 0;

	if (video_codec_ctx->coded_frame->pts != AV_NOPTS_VALUE) {
		pkt.pts = av_rescale_q(video_codec_ctx->coded_frame->pts, video_codec_ctx->time_base, video_stream->time_base);
	}

	if (video_codec_ctx->coded_frame->key_frame)
		pkt.flags |= AV_PKT_FLAG_KEY;

	pkt.stream_index = video_stream->index;
	pkt.data = video_output_file->vbuf;
	pkt.size = video_output_file->encoded_frame_size;

	// write the compressed frame in the media file
	if (av_interleaved_write_frame(video_output_file->av_fmt_ctx, &pkt) != 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Writing frame is not successful\n"));
		return -1;
	}

	av_free_packet(&pkt);

	return 0;
}

int dc_ffmpeg_video_muxer_close(VideoOutputFile *video_output_file)
{
	u32 i;

	av_write_trailer(video_output_file->av_fmt_ctx);

	avio_close(video_output_file->av_fmt_ctx->pb);

	// free the streams
	for (i = 0; i < video_output_file->av_fmt_ctx->nb_streams; i++) {
		avcodec_close(video_output_file->av_fmt_ctx->streams[i]->codec);
		av_freep(&video_output_file->av_fmt_ctx->streams[i]->info);
	}

	//video_output_file->av_fmt_ctx->streams[video_output_file->vstream_idx]->codec = NULL;
	avformat_free_context(video_output_file->av_fmt_ctx);

	return 0;
}

int dc_video_muxer_init(VideoOutputFile *video_output_file, VideoDataConf *video_data_conf, VideoMuxerType muxer_type, int frame_per_segment, int frame_per_fragment, u32 seg_marker, int gdr, int seg_dur, int frag_dur, int frame_dur, int gop_size, int video_cb_size)
{
	char name[GF_MAX_PATH];
	memset(video_output_file, 0, sizeof(VideoOutputFile));
	snprintf(name, sizeof(name), "video encoder %s", video_data_conf->filename);
	dc_consumer_init(&video_output_file->consumer, video_cb_size, name);

#ifndef GPAC_DISABLE_ISOM
	video_output_file->sample = gf_isom_sample_new();
	video_output_file->isof = NULL;
#endif

	video_output_file->muxer_type = muxer_type;

	video_output_file->frame_per_segment = frame_per_segment;
	video_output_file->frame_per_fragment = frame_per_fragment;

	video_output_file->seg_dur = seg_dur;
	video_output_file->frag_dur = frag_dur;

	video_output_file->seg_marker = seg_marker;
	video_output_file->gdr = gdr;
	video_output_file->gop_size = gop_size;
	video_output_file->frame_dur = frame_dur;

	return 0;
}

int dc_video_muxer_free(VideoOutputFile *video_output_file)
{
#ifndef GPAC_DISABLE_ISOM
	if (video_output_file->isof != NULL) {
		gf_isom_close(video_output_file->isof);
	}

	gf_isom_sample_del(&video_output_file->sample);
#endif
	return 0;
}

GF_Err dc_video_muxer_open(VideoOutputFile *video_output_file, char *directory, char *id_name, int seg)
{
	char name[GF_MAX_PATH];

	switch (video_output_file->muxer_type) {
	case FFMPEG_VIDEO_MUXER:
		snprintf(name, sizeof(name), "%s/%s_%d_ffmpeg.mp4", directory, id_name, seg);
		return dc_ffmpeg_video_muxer_open(video_output_file, name);
	case RAW_VIDEO_H264:
		snprintf(name, sizeof(name), "%s/%s_%d.264", directory, id_name, seg);
		return dc_raw_h264_open(video_output_file, name);
#ifndef GPAC_DISABLE_ISOM
	case GPAC_VIDEO_MUXER:
		snprintf(name, sizeof(name), "%s/%s_%d_gpac.mp4", directory, id_name, seg);
		dc_gpac_video_moov_create(video_output_file, name);
		return dc_gpac_video_isom_open_seg(video_output_file, NULL);
	case GPAC_INIT_VIDEO_MUXER_AVC1:
		if (seg == 1) {
			snprintf(name, sizeof(name), "%s/%s_init_gpac.mp4", directory, id_name);
			dc_gpac_video_moov_create(video_output_file, name);
			video_output_file->first_dts_in_fragment = 0;
		}
		snprintf(name, sizeof(name), "%s/%s_%d_gpac.m4s", directory, id_name, seg);
		return dc_gpac_video_isom_open_seg(video_output_file, name);
	case GPAC_INIT_VIDEO_MUXER_AVC3:
		if (seg == 0) {
			snprintf(name, sizeof(name), "%s/%s_init_gpac.mp4", directory, id_name);
			dc_gpac_video_moov_create(video_output_file, name);
			video_output_file->first_dts_in_fragment = 0;
		}
		snprintf(name, sizeof(name), "%s/%s_%d_gpac.m4s", directory, id_name, seg);
		return dc_gpac_video_isom_open_seg(video_output_file, name);
#endif
	default:
		return GF_BAD_PARAM;
	};

	return -2;
}

int dc_video_muxer_write(VideoOutputFile *video_output_file, int frame_nb, Bool insert_ntp)
{
	Bool segment_close = GF_FALSE;
	Bool fragment_close = GF_FALSE;
	switch (video_output_file->muxer_type) {
	case FFMPEG_VIDEO_MUXER:
		return dc_ffmpeg_video_muxer_write(video_output_file);
	case RAW_VIDEO_H264:
		return dc_raw_h264_write(video_output_file);
#ifndef GPAC_DISABLE_ISOM
	case GPAC_VIDEO_MUXER:
	case GPAC_INIT_VIDEO_MUXER_AVC1:
	case GPAC_INIT_VIDEO_MUXER_AVC3:
		if (video_output_file->use_source_timing) {
			GF_Err ret;
			if (!video_output_file->fragment_started) {
				video_output_file->fragment_started = 1;
				ret = gf_isom_start_fragment(video_output_file->isof, 1);
				if (ret < 0)
					return -1;

				
				//insert UTC for each fragment
				if (insert_ntp) {
					gf_isom_set_fragment_reference_time(video_output_file->isof, video_output_file->trackID, video_output_file->frame_ntp, video_output_file->codec_ctx->coded_frame->pts);
				}

				video_output_file->first_dts_in_fragment = video_output_file->codec_ctx->coded_frame->pkt_dts;
				if (!video_output_file->segment_started) {
					video_output_file->pts_at_segment_start = video_output_file->codec_ctx->coded_frame->pts;
					video_output_file->segment_started = 1;
					if (!video_output_file->nb_segments) {
						video_output_file->pts_at_first_segment = video_output_file->pts_at_segment_start;
					}

#ifndef GPAC_DISABLE_LOG
					if (insert_ntp && gf_log_tool_level_on(GF_LOG_DASH, GF_LOG_INFO)) {
						if (!video_output_file->ntp_at_first_dts) {
							video_output_file->ntp_at_first_dts = video_output_file->frame_ntp;
						} else {
							s32 ntp_diff = gf_net_get_ntp_diff_ms(video_output_file->ntp_at_first_dts);
							s32 ts_diff = (s32) ( 1000 * (video_output_file->codec_ctx->coded_frame->pts - video_output_file->pts_at_first_segment) / video_output_file->timescale );

							s32 diff_ms = ts_diff - ntp_diff;
							GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DashCast] Video Segment start NTP diff: %d ms TS diff: %d ms drift: %d ms\n", ntp_diff, ts_diff, diff_ms));
						}
					}
#endif
				}
				gf_isom_set_traf_base_media_decode_time(video_output_file->isof, video_output_file->trackID, video_output_file->first_dts_in_fragment);
			}

			if (dc_gpac_video_isom_write(video_output_file) < 0) {
				return -1;
			}
			video_output_file->last_pts = video_output_file->codec_ctx->coded_frame->pts;
			video_output_file->last_dts = video_output_file->codec_ctx->coded_frame->pkt_dts;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DashCast] PTS: "LLU", DTS: "LLU", first DTS in frag: "LLU", timescale: %d, frag dur: %d\n", video_output_file->last_pts, video_output_file->last_dts, video_output_file->first_dts_in_fragment, video_output_file->timescale, video_output_file->frag_dur));

			//we may have rounding errors on the input PTS :( add half frame dur safety
			//flush segments based on the cumultated duration , to avoid drift
			/* Check why segment tests work on PTS while fragment tests work on DTS ? */
			/* Check why fragment closing is not tested based on accumulation of fragment duration to avoid drifts */
			segment_close =  ((video_output_file->last_pts - video_output_file->pts_at_first_segment + video_output_file->frame_dur) * 1000 >=
			                  (video_output_file->nb_segments+1)*video_output_file->seg_dur * (u64)video_output_file->timescale);
#if 0
			segment_close =  ((video_output_file->last_pts - video_output_file->pts_at_segment_start + 3*video_output_file->frame_dur/2) * 1000 >=
			                  (video_output_file->seg_dur * (u64)video_output_file->timescale);
#endif
			//flush fragment if adding next frame will exceed target duration by half the frame duration
			                  fragment_close = ((video_output_file->last_dts - video_output_file->first_dts_in_fragment + 3 * video_output_file->frame_dur / 2) * 1000 >=
			                                    (video_output_file->frag_dur * (u64)video_output_file->timescale));

			if (segment_close || fragment_close) {
				gf_isom_flush_fragments(video_output_file->isof, 1);
				video_output_file->fragment_started = 0;
				GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DashCast] Flushed fragment at UTC "LLU" ms - First DTS "LLU" last PTS "LLU" - First Segment PTS "LLU" timescale %d\n", gf_net_get_utc(), video_output_file->first_dts_in_fragment, video_output_file->codec_ctx->coded_frame->pts, video_output_file->pts_at_segment_start, video_output_file->timescale));
			}

			if (segment_close) {
			return 1;
		}
		return 0;
	}

	if (frame_nb % video_output_file->frame_per_fragment == 0) {
			gf_isom_start_fragment(video_output_file->isof, 1);

			if (!video_output_file->segment_started) {
				video_output_file->pts_at_segment_start = video_output_file->codec_ctx->coded_frame->pts;
				video_output_file->segment_started = 1;

				if (insert_ntp) {
					gf_isom_set_fragment_reference_time(video_output_file->isof, video_output_file->trackID, video_output_file->frame_ntp, video_output_file->pts_at_segment_start);
				}
			}


			gf_isom_set_traf_base_media_decode_time(video_output_file->isof, video_output_file->trackID, video_output_file->first_dts_in_fragment);
			video_output_file->first_dts_in_fragment += video_output_file->frame_per_fragment;
		}

		dc_gpac_video_isom_write(video_output_file);

		if (frame_nb % video_output_file->frame_per_fragment == video_output_file->frame_per_fragment - 1) {
			gf_isom_flush_fragments(video_output_file->isof, 1);
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DashCast] Flushed fragment to disk at UTC "LLU" ms - last coded frame PTS "LLU"\n", gf_net_get_utc(), video_output_file->codec_ctx->coded_frame->pts));
		}

		if (frame_nb + 1 == video_output_file->frame_per_segment)
			return 1;

		return 0;
#endif

	default:
		return -2;
	}

	return -2;
}

int dc_video_muxer_close(VideoOutputFile *video_output_file)
{
	video_output_file->fragment_started = video_output_file->segment_started = 0;
	video_output_file->nb_segments++;

	switch (video_output_file->muxer_type) {
	case FFMPEG_VIDEO_MUXER:
		return dc_ffmpeg_video_muxer_close(video_output_file);
	case RAW_VIDEO_H264:
		return dc_raw_h264_close(video_output_file);
#ifndef GPAC_DISABLE_ISOM
	case GPAC_VIDEO_MUXER:
		dc_gpac_video_isom_close_seg(video_output_file);
		return dc_gpac_video_isom_close(video_output_file);
	case GPAC_INIT_VIDEO_MUXER_AVC1:
	case GPAC_INIT_VIDEO_MUXER_AVC3:
		return dc_gpac_video_isom_close_seg(video_output_file);
#endif
	default:
		return -2;
	}

	return -2;
}
