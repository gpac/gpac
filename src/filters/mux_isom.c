/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2017
 *					All rights reserved
 *
 *  This file is part of GPAC / ISOBMF mux filter
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

#include <gpac/filters.h>
#include <gpac/constants.h>
#include <gpac/internal/isomedia_dev.h>
#include <gpac/internal/media_dev.h>



typedef struct
{
	GF_FilterPid *ipid;
	u32 track_num;
	GF_ISOSample sample;

	u32 timescale;
	u32 stream_type;
	u32 oti;
	u32 cfg_crc;
	u32 dep_id;
	u32 stsd_idx;

	Bool use_dref;
	Bool aborted;
	Bool has_append;
	Bool has_ctts;
	s64 min_neg_ctts;
	u32 nb_samples;
	u32 nb_frames_per_sample;
	u64 ts_shift;

	Bool is_3gpp;

	Bool next_is_first_sample;

	u32 media_profile_level;

} TrackWriter;

typedef struct
{
	//filter args
	GF_ISOFile *mov;
	Bool m4sys;
	Bool dref;
	GF_Fraction dur;
	u32 pack3gp;
	Bool importer;
	Bool noedit;
	Bool pack_nal;


	//internal
	u64 first_cts_min;

	GF_List *tracks;

	GF_BitStream *bs_r;
} GF_MP4MuxCtx;

static u32 gf_isom_stream_type_to_media_type(u32 stream_type, u32 oti)
{
	switch (stream_type) {
	case GF_STREAM_SCENE: return GF_ISOM_MEDIA_SCENE;
	case GF_STREAM_OD: return GF_ISOM_MEDIA_OD;
	case GF_STREAM_OCR: return GF_ISOM_MEDIA_OCR;
	case GF_STREAM_OCI: return GF_ISOM_MEDIA_OCI;
	case GF_STREAM_MPEG7: return GF_ISOM_MEDIA_MPEG7;
	case GF_STREAM_METADATA: return GF_ISOM_MEDIA_META;
	case GF_STREAM_VISUAL: return GF_ISOM_MEDIA_VISUAL;
	case GF_STREAM_AUDIO: return GF_ISOM_MEDIA_AUDIO;
	case GF_STREAM_TEXT: return GF_ISOM_MEDIA_TEXT;
	case GF_STREAM_INTERACT: return GF_ISOM_MEDIA_SCENE;
	case GF_STREAM_IPMP: return GF_ISOM_MEDIA_IPMP;
	case GF_STREAM_MPEGJ: return GF_ISOM_MEDIA_MPEGJ;
	case GF_STREAM_IPMP_TOOL: return GF_ISOM_MEDIA_IPMP;
	case GF_STREAM_FONT: return GF_ISOM_MEDIA_MPEGJ;//TOCHECK !!
	case GF_STREAM_ND_SUBPIC: return GF_ISOM_MEDIA_SUBPIC;

	case GF_STREAM_PRIVATE_SCENE:
	case GF_STREAM_PRIVATE_MEDIA:
	case GF_STREAM_ENCRYPTED:
	case GF_STREAM_FILE:
		return 0;
	default:
		return stream_type;
	}
	return 0;
}


GF_Err mp4_mux_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	Bool use_m4sys = GF_FALSE;
	Bool needs_track = GF_FALSE;
	Bool needs_sample_entry = GF_FALSE;
	Bool use_gen_sample_entry = GF_FALSE;
	Bool use_3gpp_config = GF_FALSE;
	Bool use_ac3_entry = GF_FALSE;
	Bool use_dref = GF_FALSE;
	u32 m_subtype=0;
	u32 amr_mode_set = 0;
	u32 width, height, sr, nb_chan, nb_bps;
	GF_Fraction fps, sar;
	const char *comp_name = NULL;
	const char *imp_name = NULL;
	const char *src_url = NULL;
	const char *meta_mime = NULL;
	const char *meta_encoding = NULL;
	const char *meta_config = NULL;
	const char *meta_xmlns = NULL;
	const char *meta_schemaloc = NULL;
	const char *meta_auxmimes = NULL;

	u32 i, count, reuse_stsd = 0;
	GF_Err e;
	const GF_PropertyValue *dsi=NULL;
	const GF_PropertyValue *p;
	GF_MP4MuxCtx *ctx = gf_filter_get_udta(filter);
	TrackWriter *tkw;

	if (is_remove) {
		tkw = gf_filter_pid_get_udta(pid);
		if (tkw) {
			gf_list_del_item(ctx->tracks, tkw);
			gf_free(tkw);
		}
		return GF_OK;
	}

	//new pid ?
	tkw = gf_filter_pid_get_udta(pid);
	if (!tkw) {
		GF_FilterEvent evt;
		GF_SAFEALLOC(tkw, TrackWriter);
		gf_list_add(ctx->tracks, tkw);
		tkw->ipid = pid;
		gf_filter_pid_set_udta(pid, tkw);

		GF_FEVT_INIT(evt, GF_FEVT_PLAY, pid);
		gf_filter_pid_send_event(pid, &evt);

		gf_filter_pid_set_framing_mode(pid, GF_TRUE);
	}

	//check change of pid config
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
	if (p) {
		if (p->value.uint != tkw->stream_type) needs_track = GF_TRUE;
		tkw->stream_type = p->value.uint;
	}
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DEPENDENCY_ID);
	if (p) {
		if (p->value.uint!=tkw->dep_id) needs_track = GF_TRUE;
		tkw->stream_type = p->value.uint;
	}

	//check change of pid config
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_OTI);
	if (p) {
		if (p->value.uint!=tkw->stream_type) needs_sample_entry = GF_TRUE;
		tkw->oti = p->value.uint;
	}

	dsi = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
	if (dsi) {
		u32 cfg_crc = gf_crc_32(dsi->value.data.ptr, dsi->value.data.size);
		if (cfg_crc!=tkw->cfg_crc) needs_sample_entry = GF_TRUE;
		tkw->cfg_crc = cfg_crc;
	} else if (tkw->cfg_crc) {
		tkw->cfg_crc = 0;
		needs_sample_entry = GF_TRUE;
	}

	p = gf_filter_pid_get_info(pid, GF_PROP_PID_URL);
	if (p) src_url = p->value.string;


	if (needs_track) {
		u32 tkid=0;
		u32 mtype=0;
		//assign some defaults
		tkw->timescale = 1000;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
		if (!p) p = gf_filter_pid_get_property(pid, GF_PROP_PID_SAMPLE_RATE);
		if (p) tkw->timescale = p->value.uint;
		else {
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_FPS);
			if (p && p->value.frac.den) tkw->timescale = p->value.frac.den;
		}


		p = gf_filter_pid_get_property(pid, GF_PROP_PID_ESID);
		if (!p) p = gf_filter_pid_get_property(pid, GF_PROP_PID_ID);
		if (p) tkid = p->value.uint;
		mtype = gf_isom_stream_type_to_media_type(tkw->stream_type, tkw->oti);

		tkw->track_num = gf_isom_new_track(ctx->mov, tkid, mtype, tkw->timescale);
		if (!tkw->track_num) {
			tkw->track_num = gf_isom_new_track(ctx->mov, 0, mtype, tkw->timescale);
		}
		if (!tkw->track_num) {
			e = gf_isom_last_error(ctx->mov);
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Failed to create new track: %s\n", gf_error_to_string(e) ));
			return e;
		}
		tkw->next_is_first_sample = GF_TRUE;
		gf_isom_set_track_enabled(ctx->mov, tkw->track_num, 1);
		//by default use cttsv1 (negative ctts)
		gf_isom_set_composition_offset_mode(ctx->mov, tkw->track_num, GF_TRUE);

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_PROFILE_LEVEL);
		if (p) {
			tkw->media_profile_level = p->value.uint;
			if (tkw->stream_type == GF_STREAM_AUDIO) {
				gf_isom_set_pl_indication(ctx->mov, GF_ISOM_PL_AUDIO, p->value.uint);
			} else if (tkw->stream_type == GF_STREAM_VISUAL) {
				gf_isom_set_pl_indication(ctx->mov, GF_ISOM_PL_VISUAL, p->value.uint);
			}
		}


	}

	use_m4sys = ctx->m4sys;
	use_gen_sample_entry = GF_TRUE;
	use_dref = ctx->dref;

	//get our subtype
	switch (tkw->oti) {
	case GPAC_OTI_AUDIO_MPEG1:
	case GPAC_OTI_AUDIO_MPEG2_PART3:
		m_subtype = GF_ISOM_SUBTYPE_MP3;
		comp_name = "MP3";
		break;
	case GPAC_OTI_AUDIO_AAC_MPEG4:
	case GPAC_OTI_AUDIO_AAC_MPEG2_MP:
	case GPAC_OTI_AUDIO_AAC_MPEG2_LCP:
	case GPAC_OTI_AUDIO_AAC_MPEG2_SSRP:
		m_subtype = GF_ISOM_SUBTYPE_MPEG4;
		use_m4sys = GF_TRUE;
		comp_name = "AAC";
		use_gen_sample_entry = GF_FALSE;

		if (ctx->importer) {
			const char *pid_args = gf_filter_pid_get_args(pid);
			if (pid_args) {
				Bool sbr_i = strstr(pid_args, "sbr=imp") ? GF_TRUE : GF_FALSE;
				Bool sbr_x = strstr(pid_args, "sbr=exp") ? GF_TRUE : GF_FALSE;
				Bool ps_i = strstr(pid_args, "ps=imp") ? GF_TRUE : GF_FALSE;
				Bool ps_x = strstr(pid_args, "ps=exp") ? GF_TRUE : GF_FALSE;

				if (sbr_x) {
					if (ps_i) imp_name = "AAC explicit SBR implict PS";
					else if (ps_x) imp_name = "AAC explicit SBR+PS";
					else imp_name = "AAC explicit SBR";
				} else if (sbr_i) {
					if (ps_i) imp_name = "AAC implicit SBR+PS";
					else if (ps_x) imp_name = "AAC implicit SBR explicit PS";
					else imp_name = "AAC implicit SBR";
				} else {
					if (ps_i) imp_name = "AAC implicit PS";
					else if (ps_x) imp_name = "AAC explicit PS";
					else imp_name = "AAC ";
				}
			}
		}
		break;
	case GPAC_OTI_IMAGE_JPEG:
		m_subtype = GF_ISOM_BOX_TYPE_JPEG;
		comp_name = "JPEG";
		break;
	case GPAC_OTI_IMAGE_PNG:
		m_subtype = GF_ISOM_BOX_TYPE_PNG;
		comp_name = "PNG";
		break;
	case GPAC_OTI_IMAGE_JPEG_2000:
		m_subtype = GF_ISOM_BOX_TYPE_JP2K;
		comp_name = "JP2K";
		break;

	case GPAC_OTI_AUDIO_AMR:
		m_subtype = GF_ISOM_SUBTYPE_3GP_AMR;
		comp_name = "AMR";
		use_3gpp_config = GF_TRUE;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_AMR_MODE_SET);
		if (p) amr_mode_set = p->value.uint;
		break;
	case GPAC_OTI_AUDIO_AMR_WB:
		m_subtype = GF_ISOM_SUBTYPE_3GP_AMR_WB;
		comp_name = "AMR-WB";
		use_3gpp_config = GF_TRUE;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_AMR_MODE_SET);
		if (p) amr_mode_set = p->value.uint;
		break;
	case GPAC_OTI_AUDIO_EVRC:
		m_subtype = GF_ISOM_SUBTYPE_3GP_EVRC;
		comp_name = "EVRC";
		use_3gpp_config = GF_TRUE;
		break;
	case GPAC_OTI_AUDIO_SMV:
		m_subtype = GF_ISOM_SUBTYPE_3GP_SMV;
		comp_name = "SMV";
		use_3gpp_config = GF_TRUE;
		break;
	case GPAC_OTI_AUDIO_QCELP:
		m_subtype = GF_ISOM_SUBTYPE_3GP_QCELP;
		comp_name = "QCELP";
		use_3gpp_config = GF_TRUE;
		break;
	case GPAC_OTI_VIDEO_H263:
		m_subtype = GF_ISOM_SUBTYPE_3GP_H263;
		comp_name = "H263";
		use_3gpp_config = GF_TRUE;
		break;
	case GPAC_OTI_AUDIO_AC3:
		m_subtype = GF_ISOM_SUBTYPE_AC3;
		comp_name = "AC-3";
		use_ac3_entry = GF_TRUE;
		break;
	case GPAC_OTI_AUDIO_EAC3:
		m_subtype = GF_ISOM_SUBTYPE_AC3;
		comp_name = "EAC-3";
		use_ac3_entry = GF_TRUE;
		break;
	case GPAC_OTI_VIDEO_MPEG4_PART2:
		m_subtype = GF_ISOM_SUBTYPE_MPEG4;
		use_m4sys = GF_TRUE;
		comp_name = "MPEG-4 Visual Part 2";
		use_gen_sample_entry = GF_FALSE;
		break;
	case GPAC_OTI_VIDEO_AVC:
		m_subtype = GF_ISOM_SUBTYPE_AVC_H264;
		use_m4sys = GF_TRUE;
		comp_name = "MPEG-4 AVC";
		use_gen_sample_entry = GF_FALSE;
		break;
	case GPAC_OTI_VIDEO_MPEG1:
	case GPAC_OTI_VIDEO_MPEG2_422:
	case GPAC_OTI_VIDEO_MPEG2_SNR:
	case GPAC_OTI_VIDEO_MPEG2_HIGH:
	case GPAC_OTI_VIDEO_MPEG2_MAIN:
	case GPAC_OTI_VIDEO_MPEG2_SIMPLE:
	case GPAC_OTI_VIDEO_MPEG2_SPATIAL:
		m_subtype = GF_ISOM_SUBTYPE_MPEG4;
		use_m4sys = GF_TRUE;
		comp_name = "MPEG-2 Video";
		use_gen_sample_entry = GF_FALSE;
		break;
	case 0:
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] muxing OTI %d not yet implemented - patch welcome\n", tkw->oti));
		return GF_NOT_SUPPORTED;

	default:
		m_subtype = tkw->oti;
		use_gen_sample_entry = GF_TRUE;
		use_m4sys = GF_FALSE;
		comp_name = gf_4cc_to_str(m_subtype);

		p = gf_filter_pid_get_property_str(pid, "meta:mime");
		if (p) meta_mime = p->value.string;
		p = gf_filter_pid_get_property_str(pid, "meta:encoding");
		if (p) meta_encoding = p->value.string;
		p = gf_filter_pid_get_property_str(pid, "meta:config");
		if (p) meta_config = p->value.string;
		p = gf_filter_pid_get_property_str(pid, "meta:xmlns");
		if (p) meta_xmlns = p->value.string;
		p = gf_filter_pid_get_property_str(pid, "meta:schemaloc");
		if (p) meta_schemaloc = p->value.string;
		p = gf_filter_pid_get_property_str(pid, "meta:aux_mimes");
		if (p) meta_auxmimes = p->value.string;
		break;
	}

	if (!needs_sample_entry) return GF_OK;

	width = height = sr = nb_chan = 0;
	nb_bps = 16;
	fps.num = 25;
	sar.num = 1;
	fps.den = sar.den = 1;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_WIDTH);
	if (p) width = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_HEIGHT);
	if (p) height = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_FPS);
	if (p) fps = p->value.frac;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_SAR);
	if (p) sar = p->value.frac;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_SAMPLE_RATE);
	if (p) sr = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_NUM_CHANNELS);
	if (p) nb_chan = p->value.uint;

	reuse_stsd = 0;
	count = gf_isom_get_sample_description_count(ctx->mov, tkw->track_num);
	for (i=0; i<count; i++) {
		u32 existing_subtype = gf_isom_get_media_subtype(ctx->mov, tkw->track_num, i+1);
		if (existing_subtype != m_subtype) continue;
		if (use_m4sys) {
			Bool same_entry = GF_TRUE;
			GF_ESD *esd = gf_isom_get_esd(ctx->mov, tkw->track_num, i+1);
			if (!esd) continue;
			if (esd->decoderConfig->streamType!=tkw->stream_type) same_entry = GF_FALSE;
			else if (esd->decoderConfig->objectTypeIndication != tkw->oti) same_entry = GF_FALSE;
			else if (esd->decoderConfig->decoderSpecificInfo && esd->decoderConfig->decoderSpecificInfo->data) {
				u32 crc1 = gf_crc_32(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength);
				if (crc1 != tkw->cfg_crc) same_entry = GF_FALSE;
			}

			gf_odf_desc_del((GF_Descriptor *)esd);
			if (!same_entry) continue;
		}
		//same type, need to check for other info ?
		//for now we allow creating a single track with different configs (sample rates, etc ...), might need to change that
		reuse_stsd = 1;
		break;
	}

	if (reuse_stsd) {
		tkw->stsd_idx = reuse_stsd;

		if (use_3gpp_config && amr_mode_set) {
			GF_3GPConfig *gpp_cfg = gf_isom_3gp_config_get(ctx->mov, tkw->track_num, tkw->stsd_idx);
			if (gpp_cfg->AMR_mode_set != amr_mode_set) {
				gpp_cfg->AMR_mode_set = amr_mode_set;
				gf_isom_3gp_config_update(ctx->mov, tkw->track_num, gpp_cfg, tkw->stsd_idx);
			}
			gf_free(gpp_cfg);
		}
		return GF_OK;
	}

	if (!use_dref) src_url = NULL;

	//nope, create sample entry
	if (use_m4sys) {
		GF_ESD *esd = gf_odf_desc_esd_new(2);
		esd->decoderConfig->streamType = tkw->stream_type;
		esd->decoderConfig->objectTypeIndication = tkw->oti;
		esd->slConfig->timestampResolution = tkw->timescale;
		if (dsi) {
			esd->decoderConfig->decoderSpecificInfo->data = dsi->value.data.ptr;
			esd->decoderConfig->decoderSpecificInfo->dataLength = dsi->value.data.size;
		}

		e = gf_isom_new_mpeg4_description(ctx->mov, tkw->track_num, esd, (char *)src_url, NULL, &tkw->stsd_idx);
		if (dsi) {
			esd->decoderConfig->decoderSpecificInfo->data = NULL;
			esd->decoderConfig->decoderSpecificInfo->dataLength = 0;
		}
		gf_odf_desc_del((GF_Descriptor *) esd);

		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Error creating new MPEG-4 Systems sample description for stream type %d OTI %d: %s\n", tkw->stream_type, tkw->oti, gf_error_to_string(e) ));
			return e;
		}
		tkw->use_dref = src_url ? GF_TRUE : GF_FALSE;
	} else if (use_3gpp_config) {
		GF_3GPConfig gpp_cfg;
		memset(&gpp_cfg, 0, sizeof(GF_3GPConfig));
		gpp_cfg.type = m_subtype;
		gpp_cfg.vendor = GF_VENDOR_GPAC;

		if (use_dref) {
			gpp_cfg.frames_per_sample  = 1;
		} else {
			gpp_cfg.frames_per_sample = ctx->pack3gp;
			if (!gpp_cfg.frames_per_sample) gpp_cfg.frames_per_sample  = 1;
			else if (gpp_cfg.frames_per_sample >15) gpp_cfg.frames_per_sample = 15;
		}
		if (tkw->stream_type==GF_STREAM_VISUAL) {
			/*FIXME - we need more in-depth parsing of the bitstream to detect P3@L10 (streaming wireless)*/
			gpp_cfg.H263_profile = 0;
			gpp_cfg.H263_level = 10;
			gpp_cfg.frames_per_sample = 0;
		}
		tkw->nb_frames_per_sample = gpp_cfg.frames_per_sample;

		e = gf_isom_3gp_config_new(ctx->mov, tkw->track_num, &gpp_cfg, (char *) src_url, NULL, &tkw->stsd_idx);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Error creating new 3GPP audio sample description for stream type %d OTI %d: %s\n", tkw->stream_type, tkw->oti, gf_error_to_string(e) ));
			return e;
		}
		tkw->use_dref = src_url ? GF_TRUE : GF_FALSE;

		if (gpp_cfg.type==GF_ISOM_SUBTYPE_3GP_QCELP) {
			gf_isom_set_brand_info(ctx->mov, GF_ISOM_BRAND_3G2A, 65536);
		} else if (gpp_cfg.type==GF_ISOM_SUBTYPE_3GP_H263) {
			gf_isom_modify_alternate_brand(ctx->mov, GF_ISOM_BRAND_3GG6, 1);
			gf_isom_modify_alternate_brand(ctx->mov, GF_ISOM_BRAND_3GG5, 1);
		}
		tkw->is_3gpp = GF_TRUE;
	} else if (use_ac3_entry) {
		GF_AC3Config ac3cfg;
		memset(&ac3cfg, 0, sizeof(GF_AC3Config));

		p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_AC3_CFG);
		if (p) {
			GF_BitStream *bs = gf_bs_new(p->value.data.ptr, p->value.data.size, GF_BITSTREAM_READ);
			ac3cfg.nb_streams = 1;
			ac3cfg.streams[0].fscod = gf_bs_read_int(bs, 2);
			ac3cfg.streams[0].bsid = gf_bs_read_int(bs, 5);
			ac3cfg.streams[0].bsmod = gf_bs_read_int(bs, 3);
			ac3cfg.streams[0].acmod = gf_bs_read_int(bs, 3);
			ac3cfg.streams[0].lfon = gf_bs_read_int(bs, 1);
			ac3cfg.brcode = gf_bs_read_int(bs, 5);
			gf_bs_del(bs);
		}
		e = gf_isom_ac3_config_new(ctx->mov, tkw->track_num, &ac3cfg, (char *)src_url, NULL, &tkw->stsd_idx);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Error creating new AC3 audio sample description for stream type %d OTI %d: %s\n", tkw->stream_type, tkw->oti, gf_error_to_string(e) ));
			return e;
		}
		tkw->use_dref = src_url ? GF_TRUE : GF_FALSE;
	} else if (m_subtype == GF_ISOM_SUBTYPE_METX) {
		comp_name = "XML Metadata";
		e = gf_isom_new_xml_metadata_description(ctx->mov, tkw->track_num, meta_xmlns, meta_schemaloc, meta_encoding, &tkw->stsd_idx);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Error creating new METX sample description: %s\n", gf_error_to_string(e) ));
			return e;
		}
	} else if (m_subtype == GF_ISOM_SUBTYPE_METT) {
		comp_name = "Text Metadata";
		e = gf_isom_new_stxt_description(ctx->mov, tkw->track_num, GF_ISOM_SUBTYPE_METT, meta_mime, meta_encoding, meta_config, &tkw->stsd_idx);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Error creating new METT sample description: %s\n", gf_error_to_string(e) ));
			return e;
		}
	} else if (m_subtype == GF_ISOM_SUBTYPE_STPP) {
		comp_name = "XML Subtitle";
		e = gf_isom_new_xml_subtitle_description(ctx->mov, tkw->track_num, meta_xmlns, meta_schemaloc, meta_auxmimes, &tkw->stsd_idx);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Error creating new METT sample description: %s\n", gf_error_to_string(e) ));
			return e;
		}
	} else if ((m_subtype == GF_ISOM_SUBTYPE_SBTT) || (m_subtype == GF_ISOM_SUBTYPE_STXT) ) {
		comp_name = (m_subtype == GF_ISOM_SUBTYPE_STXT) ? "Simple Timed Text" : "Textual Subtitle";
		e = gf_isom_new_stxt_description(ctx->mov, tkw->track_num, m_subtype, meta_mime, meta_encoding, meta_config, &tkw->stsd_idx);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Error creating new %s sample description: %s\n", gf_4cc_to_str(m_subtype), gf_error_to_string(e) ));
			return e;
		}
	} else if (use_gen_sample_entry) {
		u32 len = 0;
		GF_GenericSampleDescription udesc;
		memset(&udesc, 0, sizeof(GF_GenericSampleDescription));
		udesc.codec_tag = m_subtype;
		if (!comp_name) comp_name = "Unknown";
		len = strlen(comp_name);
		if (len>32) len = 32;
		udesc.compressor_name[0] = len;
		memcpy(udesc.compressor_name+1, comp_name, len);
		udesc.samplerate = sr;
		udesc.nb_channels = nb_chan;
		udesc.width = width;
		udesc.height = height;
		if (width) {
			udesc.v_res = 72;
			udesc.h_res = 72;
			udesc.depth = 24;
		}

		e = gf_isom_new_generic_sample_description(ctx->mov, tkw->track_num, (char *)src_url, NULL, &udesc, &tkw->stsd_idx);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Error creating new sample description for stream type %d OTI %d: %s\n", tkw->stream_type, tkw->oti, gf_error_to_string(e) ));
			return e;
		}
		tkw->use_dref = src_url ? GF_TRUE : GF_FALSE;
	}

	if (sr) gf_isom_set_audio_info(ctx->mov, tkw->track_num, tkw->stsd_idx, sr, nb_chan, nb_bps);
	else if (width) {
		gf_isom_set_visual_info(ctx->mov, tkw->track_num, tkw->stsd_idx, width, height);
		if (sar.num != sar.den)
			gf_isom_set_pixel_aspect_ratio(ctx->mov, tkw->track_num, tkw->stsd_idx, sar.num, sar.den);
	}


	if (ctx->importer) {
		if (!imp_name) imp_name = comp_name;
		if (sr) {
			GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("Importing %s - SampleRate %d Num Channels %d\n", imp_name, sr, nb_chan));
		} else if (width) {
			GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("Importing %s - Width %d Height %d FPS %d/%d SAR %d/%u\n", imp_name, width, height, fps.num, fps.den, sar.num, sar.den));
		} else {
			GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("Importing %s\n", imp_name));
		}
	}

	return GF_OK;
}

#if 0
static Bool mp4_mux_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	u32 i;
	GF_FilterEvent fevt;
	GF_MP4MuxCtx *ctx = gf_filter_get_udta(filter);

	//by default don't cancel event - to rework once we have downloading in place
	return GF_FALSE;
}
#endif

GF_Err mp4_mux_process(GF_Filter *filter)
{
	GF_Err e = GF_OK;
	GF_MP4MuxCtx *ctx = gf_filter_get_udta(filter);
	u32 nb_eos, i, count = gf_list_count(ctx->tracks);

	nb_eos = 0;

	for (i=0; i<count; i++) {
		u64 cts, prev_dts;
		u32 prev_size=0;
		u32 duration = 0;
		u32 timescale = 0;
		const GF_PropertyValue *p;
		GF_FilterSAPType sap_type;
		TrackWriter *tkw = gf_list_get(ctx->tracks, i);
		GF_FilterPacket *pck = gf_filter_pid_get_packet(tkw->ipid);

		if (!pck) {
			if (gf_filter_pid_is_eos(tkw->ipid))
				nb_eos++;
			continue;
		}
		if (tkw->aborted) {
			gf_filter_pid_drop_packet(tkw->ipid);
			nb_eos++;
			continue;
		}

		timescale = gf_filter_pck_get_timescale(pck);

		prev_dts = tkw->sample.DTS;
		prev_size = tkw->sample.dataLength;
		tkw->sample.CTS_Offset = 0;
		tkw->sample.data = (char *)gf_filter_pck_get_data(pck, &tkw->sample.dataLength);
		tkw->sample.DTS = gf_filter_pck_get_dts(pck);
		cts = gf_filter_pck_get_cts(pck);
		if (tkw->sample.DTS == GF_FILTER_NO_TS) {
			if (cts == GF_FILTER_NO_TS) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Sample with no DTS/CTS, cannot add (last DTS "LLU", last size %d)\n", prev_dts, prev_size ));
				gf_filter_pid_drop_packet(tkw->ipid);
				continue;
			} else {
				tkw->sample.DTS = cts;
			}
		} else {
			tkw->sample.CTS_Offset = (s32) ((s64) cts - (s64) tkw->sample.DTS);
		}
		tkw->sample.IsRAP = 0;
		sap_type = gf_filter_pck_get_sap(pck);
		if (sap_type!=GF_FILTER_SAP_REDUNDANT)
			tkw->sample.IsRAP = gf_filter_pck_get_sap(pck);

		if (tkw->sample.CTS_Offset) tkw->has_ctts = GF_TRUE;
		if (tkw->next_is_first_sample && tkw->sample.DTS) {
			if (!ctx->first_cts_min) {
				ctx->first_cts_min = tkw->sample.DTS * 1000000;
				ctx->first_cts_min /= tkw->timescale;
				tkw->ts_shift = tkw->sample.DTS;
			}
		}
		if (tkw->ts_shift) {
			assert(tkw->sample.DTS >= tkw->ts_shift);
			tkw->sample.DTS -= tkw->ts_shift;
		}

		duration = gf_filter_pck_get_duration(pck);

		if (timescale != tkw->timescale) {
			tkw->sample.DTS *= tkw->timescale;
			tkw->sample.DTS /= timescale;
			tkw->sample.CTS_Offset *= tkw->timescale;
			tkw->sample.CTS_Offset /= timescale;
			duration *= tkw->timescale;
			duration /= timescale;
		}
		if (tkw->sample.CTS_Offset < tkw->min_neg_ctts) tkw->min_neg_ctts = tkw->sample.CTS_Offset;

		if (sap_type == GF_FILTER_SAP_REDUNDANT) {
			tkw->sample.IsRAP = SAP_TYPE_1;
			gf_isom_add_sample_shadow(ctx->mov, tkw->track_num, &tkw->sample);
		}
		else if (tkw->use_dref) {
			u64 data_offset = gf_filter_pck_get_byte_offset(pck);
			if (data_offset != GF_FILTER_NO_BO) {
				e = gf_isom_add_sample_reference(ctx->mov, tkw->track_num, tkw->stsd_idx, &tkw->sample, data_offset);
				if (e) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Failed to add sample DTS "LLU" as reference: %s\n", tkw->sample.DTS, gf_error_to_string(e) ));
				}
			} else {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Cannot add sample reference at DTS "LLU" , input sample data is not continous in source\n", tkw->sample.DTS ));
			}
		} else if (tkw->nb_frames_per_sample && (tkw->nb_samples % tkw->nb_frames_per_sample)) {
			e = gf_isom_append_sample_data(ctx->mov, tkw->track_num, tkw->sample.data, tkw->sample.dataLength);
			tkw->has_append = GF_TRUE;
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Failed to append sample DTS "LLU" data: %s\n", tkw->sample.DTS, gf_error_to_string(e) ));
			}
		} else {
			e = gf_isom_add_sample(ctx->mov, tkw->track_num, tkw->stsd_idx, &tkw->sample);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Failed to add sample DTS "LLU" - prev DTS "LLU": %s\n", tkw->sample.DTS, prev_dts, gf_error_to_string(e) ));
			} else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MP4Mux] added sample DTS "LLU" - prev DTS "LLU" - prev size %d\n", tkw->sample.DTS, prev_dts, prev_size));
			}
		}

		tkw->nb_samples++;

		p = gf_filter_pck_get_property(pck, GF_PROP_PCK_SUBS);
		if (p) {
			if (!ctx->bs_r) ctx->bs_r = gf_bs_new(p->value.data.ptr, p->value.data.size, GF_BITSTREAM_READ);
			else gf_bs_reassign_buffer(ctx->bs_r, p->value.data.ptr, p->value.data.size);

			while (gf_bs_available(ctx->bs_r)) {
				u32 flags = gf_bs_read_u32(ctx->bs_r);
				u32 subs_size = gf_bs_read_u32(ctx->bs_r);
				u32 reserved = gf_bs_read_u32(ctx->bs_r);
				u8 priority = gf_bs_read_u8(ctx->bs_r);
				u8 discardable = gf_bs_read_u8(ctx->bs_r);

				gf_isom_add_subsample(ctx->mov, tkw->track_num, tkw->nb_samples, flags, subs_size, priority, reserved, discardable);
			}
		}

		tkw->next_is_first_sample = GF_FALSE;

		if (duration) gf_isom_set_last_sample_duration(ctx->mov, tkw->track_num, duration);

		if (ctx->dur.num) {
			u32 mdur = gf_isom_get_media_duration(ctx->mov, tkw->track_num);

			if (ctx->importer) {
				gf_set_progress("Import", mdur * ctx->dur.den, tkw->timescale * ctx->dur.num);
			}
			
			if (mdur * ctx->dur.den > tkw->timescale * ctx->dur.num) {
				GF_FilterEvent evt;
				GF_FEVT_INIT(evt, GF_FEVT_STOP, tkw->ipid);
				gf_filter_pid_send_event(tkw->ipid, &evt);

				tkw->aborted = GF_TRUE;
			}
		} else if (ctx->importer) {
			const GF_PropertyValue *p;
			u64 data_offset = gf_filter_pck_get_byte_offset(pck);
			if (data_offset == GF_FILTER_NO_BO) {
				p = gf_filter_pid_get_info(tkw->ipid, GF_PROP_PID_DOWN_BYTES);
				if (p) data_offset = p->value.longuint;
			}

			p = gf_filter_pid_get_info(tkw->ipid, GF_PROP_PID_DOWN_SIZE);
			if ((data_offset != GF_FILTER_NO_BO) && p) {
				gf_set_progress("Import", data_offset, p->value.uint);
			} else {
				p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_DURATION);
				if (p) {
					gf_set_progress("Import", tkw->sample.DTS, p->value.frac.num);
				} else {
					gf_set_progress("Import", 0, 1);
				}

			}
		}

		gf_filter_pid_drop_packet(tkw->ipid);
	}

	if (count == nb_eos) return GF_EOS;

	return GF_OK;
}

static GF_Err mp4_mux_initialize(GF_Filter *filter)
{
	GF_MP4MuxCtx *ctx = gf_filter_get_udta(filter);

	if (!ctx->mov) return GF_BAD_PARAM;
	if (gf_isom_get_mode(ctx->mov) < GF_ISOM_OPEN_WRITE) return GF_BAD_PARAM;

	ctx->tracks = gf_list_new();
	return GF_OK;
}


static void mp4_mux_update_edit_list_for_bframes(GF_ISOFile *file, u32 track)
{
	u32 i, count, di;
	u64 max_cts, min_cts, doff;

	count = gf_isom_get_sample_count(file, track);
	max_cts = 0;
	min_cts = (u64) -1;
	for (i=0; i<count; i++) {
		GF_ISOSample *s = gf_isom_get_sample_info(file, track, i+1, &di, &doff);
		if (s->DTS + s->CTS_Offset > max_cts)
			max_cts = s->DTS + s->CTS_Offset;

		if (min_cts > s->DTS + s->CTS_Offset)
			min_cts = s->DTS + s->CTS_Offset;

		gf_isom_sample_del(&s);
	}

	if (min_cts) {
		max_cts -= min_cts;
		max_cts += gf_isom_get_sample_duration(file, track, count);

		max_cts *= gf_isom_get_timescale(file);
		max_cts /= gf_isom_get_media_timescale(file, track);
		gf_isom_set_edit_segment(file, track, 0, max_cts, min_cts, GF_ISOM_EDIT_NORMAL);
	}
}

//todo: move this func from media_import.c to here once done
void gf_media_update_bitrate(GF_ISOFile *file, u32 track);

static void mp4_mux_finalize(GF_Filter *filter)
{
	Bool is_nalu = GF_FALSE;
	GF_MP4MuxCtx *ctx = gf_filter_get_udta(filter);

	while (gf_list_count(ctx->tracks)) {
		Bool has_bframes = GF_FALSE;
		TrackWriter *tkw = gf_list_pop_back(ctx->tracks);

		//keep the old importer behaviour: use ctts v0
		if (tkw->min_neg_ctts<0) {
			gf_isom_set_cts_packing(ctx->mov, tkw->track_num, GF_TRUE);
			gf_isom_shift_cts_offset(ctx->mov, tkw->track_num, tkw->min_neg_ctts);
			gf_isom_set_cts_packing(ctx->mov, tkw->track_num, GF_FALSE);
			gf_isom_set_composition_offset_mode(ctx->mov, tkw->track_num, GF_FALSE);

			if (! ctx->noedit)
				mp4_mux_update_edit_list_for_bframes(ctx->mov, tkw->track_num);

			has_bframes = GF_TRUE;
		} else if (tkw->has_ctts && (tkw->stream_type==GF_STREAM_VISUAL) ) {
			if (! ctx->noedit)
				mp4_mux_update_edit_list_for_bframes(ctx->mov, tkw->track_num);

			has_bframes = GF_TRUE;

		}

		/*this is plain ugly but since some encoders (divx) don't use the video PL correctly
		 we force the system video_pl to ASP@L5 since we have I, P, B in base layer*/
		if (tkw->oti == GPAC_OTI_VIDEO_MPEG4_PART2) {
			Bool force_rewrite = GF_FALSE;
			u32 PL = tkw->media_profile_level;
			if (!PL) PL = 0x01;

			if (has_bframes && (tkw->media_profile_level <= 3)) {
				PL = 0xF5;
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MP4Mux] Indicated profile doesn't include B-VOPs - forcing %s", gf_m4v_get_profile_name((u8) PL) ));
				force_rewrite = GF_TRUE;
			}
			if (PL != tkw->media_profile_level) {
				if (force_rewrite) {
					GF_ESD *esd = gf_isom_get_esd(ctx->mov, tkw->track_num, tkw->stsd_idx);
					assert(esd);
					gf_m4v_rewrite_pl(&esd->decoderConfig->decoderSpecificInfo->data, &esd->decoderConfig->decoderSpecificInfo->dataLength, (u8) PL);
					gf_isom_change_mpeg4_description(ctx->mov, tkw->track_num, tkw->stsd_idx, esd);
					gf_odf_desc_del((GF_Descriptor*)esd);
				}
				gf_isom_set_pl_indication(ctx->mov, GF_ISOM_PL_VISUAL, PL);
			}
		} else if (tkw->oti == GPAC_OTI_VIDEO_AVC) {
			gf_isom_set_pl_indication(ctx->mov, GF_ISOM_PL_VISUAL, 0x7F);
			gf_isom_modify_alternate_brand(ctx->mov, GF_ISOM_BRAND_AVC1, 1);
			is_nalu = GF_TRUE;
		}
		if (tkw->has_append)
			gf_isom_refresh_size_info(ctx->mov, tkw->track_num);

		if ((tkw->nb_samples == 1) && ctx->dur.num && ctx->dur.den) {
			u32 dur = tkw->timescale * ctx->dur.num;
			dur /= ctx->dur.den;
			gf_isom_set_last_sample_duration(ctx->mov, tkw->track_num, dur);
		}

		if (!tkw->is_3gpp)
			gf_media_update_bitrate(ctx->mov, tkw->track_num);


		if (is_nalu && ctx->pack_nal && (gf_isom_get_mode(ctx->mov)!=GF_ISOM_OPEN_WRITE)) {

			GF_Err gf_media_nal_rewrite_samples(GF_ISOFile *file, u32 track, u32 new_size);

			u32 msize = 0;
			Bool do_rewrite = GF_FALSE;
			u32 i, count = gf_isom_get_sample_description_count(ctx->mov, tkw->track_num);
			const GF_PropertyValue *p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_MAX_NALU_SIZE);
			msize = gf_get_bit_size(p->value.uint);
			if (msize<8) msize = 8;
			else if (msize<16) msize = 16;
			else msize = 32;

			if (msize<=0xFFFF) {
				for (i=0; i<count; i++) {
					u32 k = 8 * gf_isom_get_nalu_length_field(ctx->mov, tkw->track_num, i+1);
					if (k > msize) {
						do_rewrite = GF_TRUE;
					}
				}
				if (do_rewrite) {
					GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[MP4Mux] Adjusting NALU SizeLength to %d bits\n", msize ));
					gf_media_nal_rewrite_samples(ctx->mov, tkw->track_num, msize);
					msize /= 8;
					for (i=0; i<count; i++) {
						gf_isom_set_nalu_length_field(ctx->mov, tkw->track_num, i+1, msize);
					}
				}
			}
		}

		gf_free(tkw);
	}
	gf_list_del(ctx->tracks);

	if (ctx->bs_r) gf_bs_del(ctx->bs_r);
}

static const GF_FilterCapability MP4MuxInputs[] =
{
	//for now don't accep files as input, although we could store them as items
	CAP_EXC_UINT(GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	//we want framed media only
	CAP_EXC_BOOL(GF_PROP_PID_UNFRAMED, GF_TRUE),
	//and any OTI
	CAP_EXC_UINT(GF_PROP_PID_OTI, GPAC_OTI_FORBIDDEN),
	{}
};


static const GF_FilterCapability MP4MuxOutputs[] =
{
	CAP_INC_UINT(GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	{}
};

#define OFFS(_n)	#_n, offsetof(GF_MP4MuxCtx, _n)
static const GF_FilterArgs MP4MuxArgs[] =
{
	{ OFFS(mov), "pointer to a write/edit ISOBMF file", GF_PROP_POINTER, NULL, NULL, GF_FALSE},
	{ OFFS(m4sys), "force MPEG-4 Systems signaling of tracks", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(dref), "only references data from source file - not compatible with all media sources", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(dur), "only imports the specified duration", GF_PROP_FRACTION, "0", NULL, GF_FALSE},
	{ OFFS(pack3gp), "packs a given number of 3GPP audio frames in one sample", GF_PROP_UINT, "1", NULL, GF_FALSE},
	{ OFFS(importer), "compatibility with old importer, displays import progress", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(noedit), "disable edit lists on video tracks", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(pack_nal), "repacks NALU size length to minimum possible size for AVC/HEVC/...", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{}
};


GF_FilterRegister MP4MuxRegister = {
	.name = "mp4mx",
	.description = "ISOBMFF File Multiplexer",
	.private_size = sizeof(GF_MP4MuxCtx),
	.args = MP4MuxArgs,
	.initialize = mp4_mux_initialize,
	.finalize = mp4_mux_finalize,
	INCAPS(MP4MuxInputs),
	OUTCAPS(MP4MuxOutputs),
	.configure_pid = mp4_mux_configure_pid,
	.process = mp4_mux_process,
//	.process_event = mp4_mux_process_event
};


const GF_FilterRegister *mp4_mux_register(GF_FilterSession *session)
{
	return &MP4MuxRegister;
}
