/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2018
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


#define TEXT_DEFAULT_WIDTH	400
#define TEXT_DEFAULT_HEIGHT	60
#define TEXT_DEFAULT_FONT_SIZE	18


typedef struct
{
	GF_FilterPid *ipid;
	u32 track_num, track_id;
	GF_ISOSample sample;

	u32 timescale;
	u32 stream_type;
	u32 codecid;

	u32 cfg_crc, enh_cfg_crc;
	u32 dep_id;
	u32 stsd_idx;

	Bool use_dref;
	Bool aborted;
	Bool has_append;
	Bool has_ctts;
	s64 min_neg_ctts;
	u32 nb_samples, samples_in_stsd;
	u32 nb_frames_per_sample;
	u64 ts_shift;

	Bool skip_bitrate_update;
	Bool has_open_gop;
	Bool has_gdr;

	Bool next_is_first_sample;

	u32 media_profile_level;

	Bool import_msg_header_done;

	GF_AVCConfig *avcc, *svcc;
	GF_HEVCConfig *hvcc, *lvcc;

	char *inband_hdr;
	u32 inband_hdr_size;
	Bool is_nalu;
	Bool fragment_done;
	s32 ts_delay;
	Bool insert_tfdt;
	u64 first_dts_in_seg, next_seg_cts, cts_next;
	u64 offset_dts;
} TrackWriter;

enum
{
	MP4MX_MODE_INTER=0,
	MP4MX_MODE_FLAT,
	MP4MX_MODE_CAP,
	MP4MX_MODE_TIGHT,
	MP4MX_MODE_FRAG,
	MP4MX_MODE_SFRAG,
};


enum
{
	MP4MX_DASH_OFF=0,
	MP4MX_DASH_ON,
	MP4MX_DASH_VOD,
};

typedef struct
{
	//filter args
	GF_ISOFile *file;
	char *tmp_dir;
	Bool m4sys, dref;
	GF_Fraction dur;
	u32 pack3gp;
	Bool importer, noedit, pack_nal, for_test, moof_first, abs_offset, fsap;
	u32 xps_inband;
	u32 block_size;
	u32 mode, tktpl, mudta;
	s32 subs_sidx;
	Double cdur;
	u32 timescale;
	char *m4cc;
	Bool chain_sidx;
	u32 moof_sn;
	GF_Fraction64 tfdt;
	Bool no_def, straf, pssh_moof, sgpd_traf, cache, noinit;
	
	//internal
	u64 first_cts_min;
	Bool owns_mov;
	GF_FilterPid *opid;
	Bool first_pck_sent;

	GF_List *tracks;

	GF_BitStream *bs_r;
	//fragmentation state
	Bool init_movie_done, fragment_started, segment_started, insert_tfdt;
	Double next_frag_start, adjusted_next_frag_start;

	u64 current_offset;
	u64 current_size;

	u32 nb_segs;

	GF_FilterPacket *dst_pck;
	char *seg_name;
	u32 dash_seg_num;
	Bool flush_seg;
	u32 eos_marker;
	TrackWriter *ref_tkw;
	Bool single_file;
	Bool store_output;
	FILE *tmp_store;
	u64 flush_size, flush_done;

	u32 dash_mode;
	Double dash_dur;
	Double media_dur;
	u32 sidx_max_size, sidx_chunk_offset;
	Bool final_sidx_flush;
} GF_MP4MuxCtx;

static void mp4_mux_set_hevc_groups(GF_MP4MuxCtx *ctx, TrackWriter *tkw);

static GF_Err mp4mx_setup_dash_vod(GF_MP4MuxCtx *ctx, TrackWriter *tkw)
{
	if (tkw) {
		const GF_PropertyValue *p;
		p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_DASH_DUR);
		if (p) {
			ctx->dash_dur = p->value.number;
		}
		p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_DURATION);
		if (p && p->value.frac.den) {
			Double mdur = p->value.frac.num;
			mdur /= p->value.frac.den;
			if (ctx->media_dur < mdur) ctx->media_dur = mdur;
		}
	}
	ctx->dash_mode = MP4MX_DASH_VOD;
	if (ctx->cache && !ctx->tmp_store) {
		ctx->tmp_store = gf_temp_file_new(NULL);
		if (!ctx->tmp_store) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Cannot allocate temp file for VOD sidx generation\n"));
			return GF_IO_ERR;
		}
		if (!ctx->block_size) ctx->block_size = 10000;
	}

	return GF_OK;
}


static u32 gf_isom_stream_type_to_media_type(u32 stream_type, u32 codecid)
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
	case GF_STREAM_TEXT:
		if (codecid==GF_ISOM_SUBTYPE_STPP)
			return GF_ISOM_MEDIA_MPEG_SUBT;
		if (codecid == GF_CODECID_SUBPIC)
			return GF_ISOM_MEDIA_SUBPIC;
		return GF_ISOM_MEDIA_TEXT;
	case GF_STREAM_INTERACT: return GF_ISOM_MEDIA_SCENE;
	case GF_STREAM_IPMP: return GF_ISOM_MEDIA_IPMP;
	case GF_STREAM_MPEGJ: return GF_ISOM_MEDIA_MPEGJ;
	case GF_STREAM_IPMP_TOOL: return GF_ISOM_MEDIA_IPMP;
	case GF_STREAM_FONT: return GF_ISOM_MEDIA_MPEGJ;//TOCHECK !!

	case GF_STREAM_PRIVATE_SCENE:
	case GF_STREAM_ENCRYPTED:
	case GF_STREAM_FILE:
		return 0;
	default:
		return stream_type;
	}
	return 0;
}

static void mp4_mux_write_ps_list(GF_BitStream *bs, GF_List *list, u32 nalu_size_length)
{
	u32 i, count = list ? gf_list_count(list) : 0;
	for (i=0; i<count; i++) {
		GF_AVCConfigSlot *sl = gf_list_get(list, i);
		gf_bs_write_int(bs, sl->size, 8*nalu_size_length);
		gf_bs_write_data(bs, sl->data, sl->size);
	}
}

static GF_List *mp4_mux_get_hevc_ps(GF_HEVCConfig *cfg, u8 type)
{
	u32 i, count = gf_list_count(cfg->param_array);
	for (i=0; i<count; i++) {
		GF_HEVCParamArray *pa = gf_list_get(cfg->param_array, i);
		if (pa->type == type) return pa->nalus;
	}
	return NULL;
}

static void mp4_mux_make_inband_header(GF_MP4MuxCtx *ctx, TrackWriter *tkw)
{
	GF_BitStream *bs;
	if (tkw->inband_hdr) gf_free(tkw->inband_hdr);

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	if (tkw->avcc || tkw->svcc) {
		if (tkw->avcc)
			mp4_mux_write_ps_list(bs, tkw->avcc->sequenceParameterSets, tkw->avcc->nal_unit_size);

		if (tkw->svcc)
			mp4_mux_write_ps_list(bs, tkw->svcc->sequenceParameterSets, tkw->svcc->nal_unit_size);

		if (tkw->avcc && tkw->avcc->sequenceParameterSetExtensions)
			mp4_mux_write_ps_list(bs, tkw->avcc->sequenceParameterSetExtensions, tkw->avcc->nal_unit_size);

		if (tkw->svcc && tkw->svcc->sequenceParameterSetExtensions)
			mp4_mux_write_ps_list(bs, tkw->svcc->sequenceParameterSetExtensions, tkw->svcc->nal_unit_size);

		if (tkw->avcc)
			mp4_mux_write_ps_list(bs, tkw->avcc->pictureParameterSets, tkw->avcc->nal_unit_size);

		if (tkw->svcc)
			mp4_mux_write_ps_list(bs, tkw->svcc->pictureParameterSets, tkw->svcc->nal_unit_size);
	}
	if (tkw->hvcc || tkw->lvcc) {
		if (tkw->hvcc)
			mp4_mux_write_ps_list(bs, mp4_mux_get_hevc_ps(tkw->hvcc, GF_HEVC_NALU_VID_PARAM), tkw->hvcc->nal_unit_size);
		if (tkw->lvcc)
			mp4_mux_write_ps_list(bs, mp4_mux_get_hevc_ps(tkw->lvcc, GF_HEVC_NALU_VID_PARAM), tkw->lvcc->nal_unit_size);
		if (tkw->hvcc)
			mp4_mux_write_ps_list(bs, mp4_mux_get_hevc_ps(tkw->hvcc, GF_HEVC_NALU_SEQ_PARAM), tkw->hvcc->nal_unit_size);
		if (tkw->lvcc)
			mp4_mux_write_ps_list(bs, mp4_mux_get_hevc_ps(tkw->lvcc, GF_HEVC_NALU_SEQ_PARAM), tkw->lvcc->nal_unit_size);
		if (tkw->hvcc)
			mp4_mux_write_ps_list(bs, mp4_mux_get_hevc_ps(tkw->hvcc, GF_HEVC_NALU_PIC_PARAM), tkw->hvcc->nal_unit_size);
		if (tkw->lvcc)
			mp4_mux_write_ps_list(bs, mp4_mux_get_hevc_ps(tkw->lvcc, GF_HEVC_NALU_PIC_PARAM), tkw->lvcc->nal_unit_size);
	}

	tkw->inband_hdr=NULL;
	gf_bs_get_content(bs, &tkw->inband_hdr, &tkw->inband_hdr_size);
	gf_bs_del(bs);
}

void mp4_mux_get_video_size(GF_MP4MuxCtx *ctx, u32 *width, u32 *height)
{
	u32 w, h, f_w, f_h, i;

	f_w = f_h = 0;
	for (i=0; i<gf_isom_get_track_count(ctx->file); i++) {
		switch (gf_isom_get_media_type(ctx->file, i+1)) {
		case GF_ISOM_MEDIA_SCENE:
		case GF_ISOM_MEDIA_VISUAL:
			gf_isom_get_visual_info(ctx->file, i+1, 1, &w, &h);
			if (w > f_w) f_w = w;
			if (h > f_h) f_h = h;
			gf_isom_get_track_layout_info(ctx->file, i+1, &w, &h, NULL, NULL, NULL);
			if (w > f_w) f_w = w;
			if (h > f_h) f_h = h;
			break;
		}
	}
	(*width) = f_w ? f_w : TEXT_DEFAULT_WIDTH;
	(*height) = f_h ? f_h : TEXT_DEFAULT_HEIGHT;
}

static GF_Err mp4_mux_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	Bool use_m4sys = GF_FALSE;
	Bool use_tx3g = GF_FALSE;
	Bool use_webvtt = GF_FALSE;
	Bool needs_track = GF_FALSE;
	u32 needs_sample_entry = 0; //1: change of codecID, 2 change of decoder config
	Bool use_gen_sample_entry = GF_FALSE;
	Bool use_3gpp_config = GF_FALSE;
	Bool use_ac3_entry = GF_FALSE;
	Bool use_avc = GF_FALSE;
	Bool use_hevc = GF_FALSE;
	Bool use_dref = GF_FALSE;
	Bool skip_dsi = GF_FALSE;
	Bool is_text_subs = GF_FALSE;
	u32 m_subtype=0;
	u32 override_stype=0;
	u32 amr_mode_set = 0;
	u32 width, height, sr, nb_chan, nb_bps, z_order, txt_fsize;
	GF_Fraction fps, sar;
	GF_List *multi_pid_stsd = NULL;
	u32 multi_pid_idx = 0;
	GF_FilterPid *orig_pid = NULL;
	u32 codec_id;
	u32 frames_per_sample_backup=0;
	Bool is_nalu_backup = GF_FALSE;
	u32 multi_pid_final_stsd_idx = 0;

	const char *lang_name = NULL;
	const char *comp_name = NULL;
	const char *imp_name = NULL;
	const char *src_url = NULL;
	const char *meta_mime = NULL;
	const char *meta_encoding = NULL;
	const char *meta_config = NULL;
	const char *meta_xmlns = NULL;
	const char *meta_schemaloc = NULL;
	const char *meta_auxmimes = NULL;
	char *txt_font = NULL;

	u32 i, count, reuse_stsd = 0;
	GF_Err e;
	const GF_PropertyValue *dsi=NULL;
	const GF_PropertyValue *enh_dsi=NULL;
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

	if (ctx->owns_mov && !ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
	}
	//copy properties at init or reconfig
	if (ctx->opid) {
		gf_filter_pid_copy_properties(ctx->opid, pid);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, NULL);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT, NULL);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, NULL);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, NULL);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_FILE) );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING("mp4") );
	}
	
	//new pid ?
	tkw = gf_filter_pid_get_udta(pid);
	if (!tkw) {
		GF_FilterEvent evt;
		GF_SAFEALLOC(tkw, TrackWriter);
		gf_list_add(ctx->tracks, tkw);
		tkw->ipid = pid;
		gf_filter_pid_set_udta(pid, tkw);

		if (!ctx->owns_mov) {
			GF_FEVT_INIT(evt, GF_FEVT_PLAY, pid);
			gf_filter_pid_send_event(pid, &evt);
		}

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
		tkw->dep_id = p->value.uint;
	}

	//check change of pid config
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (p) {
		if (p->value.uint!=tkw->stream_type) needs_sample_entry = 1;
		tkw->codecid = p->value.uint;
	}

	dsi = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
	if (dsi) {
		u32 cfg_crc = gf_crc_32(dsi->value.data.ptr, dsi->value.data.size);
		if (cfg_crc!=tkw->cfg_crc) needs_sample_entry = 2;
		tkw->cfg_crc = cfg_crc;
	} else if (tkw->cfg_crc) {
		tkw->cfg_crc = 0;
		needs_sample_entry = 2;
	}

	enh_dsi = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT);
	if (enh_dsi) {
		u32 cfg_crc = gf_crc_32(enh_dsi->value.data.ptr, enh_dsi->value.data.size);
		if (cfg_crc!=tkw->enh_cfg_crc) needs_sample_entry = 2;
		tkw->enh_cfg_crc = cfg_crc;
	} else if (tkw->cfg_crc) {
		tkw->cfg_crc = 0;
		needs_sample_entry = 2;
	}

	p = gf_filter_pid_get_info(pid, GF_PROP_PID_URL);
	if (p) src_url = p->value.string;


	p = gf_filter_pid_get_info(pid, GF_PROP_PID_DASH_MODE);
	if (p) {
		ctx->dash_mode = MP4MX_DASH_ON;
		if (p->value.uint==2) {
			e = mp4mx_setup_dash_vod(ctx, tkw);
			if (e) return e;
		}
	}

	if (needs_track) {
		u32 tkid=0;
		u32 mtype=0;

		if (ctx->init_movie_done) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MP4Mux] Cannot add track to already finalized movie in fragmented file, will request a new muxer for that track\n"));
			return GF_REQUIRES_NEW_INSTANCE;
		}

		//assign some defaults
		tkw->timescale = 1000;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
		if (!p) p = gf_filter_pid_get_property(pid, GF_PROP_PID_SAMPLE_RATE);
		if (p) tkw->timescale = p->value.uint;
		else {
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_FPS);
			if (p && p->value.frac.den) tkw->timescale = p->value.frac.den;
		}
		if (!tkw->timescale) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MP4Mux] No timescale specified, defaulting to 1000\n" ));
			tkw->timescale = 1000;
		}

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_ESID);
		if (!p) p = gf_filter_pid_get_property(pid, GF_PROP_PID_ID);
		if (p) tkid = p->value.uint;
		mtype = gf_isom_stream_type_to_media_type(tkw->stream_type, tkw->codecid);

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_ISOM_TRACK_TEMPLATE);
		if (ctx->tktpl && p && p->value.data.ptr) {
			Bool udta_only = (ctx->tktpl==2) ? GF_TRUE : GF_FALSE;
			tkw->track_num = gf_isom_new_track_from_template(ctx->file, tkid, mtype, tkw->timescale, p->value.data.ptr, p->value.data.size, udta_only);
			if (!tkw->track_num) {
				tkw->track_num = gf_isom_new_track_from_template(ctx->file, 0, mtype, tkw->timescale, p->value.data.ptr, p->value.data.size, udta_only);
			}
		} else {
			tkw->track_num = gf_isom_new_track(ctx->file, tkid, mtype, tkw->timescale);
			if (!tkw->track_num) {
				tkw->track_num = gf_isom_new_track(ctx->file, 0, mtype, tkw->timescale);
			}
		}
		if (!tkw->track_num) {
			e = gf_isom_last_error(ctx->file);
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Failed to create new track: %s\n", gf_error_to_string(e) ));
			return e;
		}
		tkw->track_id = gf_isom_get_track_id(ctx->file, tkw->track_num);
		tkw->next_is_first_sample = GF_TRUE;
		gf_isom_set_track_enabled(ctx->file, tkw->track_num, 1);
		//by default use cttsv1 (negative ctts)
		gf_isom_set_composition_offset_mode(ctx->file, tkw->track_num, GF_TRUE);

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_PROFILE_LEVEL);
		if (p) {
			tkw->media_profile_level = p->value.uint;
			if (tkw->stream_type == GF_STREAM_AUDIO) {
				gf_isom_set_pl_indication(ctx->file, GF_ISOM_PL_AUDIO, p->value.uint);
			} else if (tkw->stream_type == GF_STREAM_VISUAL) {
				gf_isom_set_pl_indication(ctx->file, GF_ISOM_PL_VISUAL, p->value.uint);
			}
		}

		if (ctx->mudta && gf_isom_get_track_count(ctx->file)==1) {
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_ISOM_UDTA);
			if (ctx->tktpl && p && p->value.data.ptr) {
				gf_isom_load_extra_boxes(ctx->file, p->value.data.ptr, p->value.data.size, (ctx->mudta==2) ? GF_TRUE : GF_FALSE);
			}
		}

		if (ctx->sgpd_traf)
			gf_isom_set_sample_group_in_traf(ctx->file);
	}

	codec_id = tkw->codecid;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DASH_MULTI_PID);
	if (p) {
		multi_pid_stsd = p->value.ptr;

		p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_DASH_MULTI_PID_IDX);
		assert(p);
		multi_pid_final_stsd_idx = p->value.uint;

		//should never be the case
		ctx->xps_inband = 0;
		ctx->dref = GF_FALSE;
		orig_pid = pid;
		goto multipid_stsd_setup;
	}


	//WARNING !! from this point on until the goto multipid_stsd_setup, use pid and not tkw->ipid
	//so that we setup the sample entry properly for each PIDs
sample_entry_setup:

	use_m4sys = ctx->m4sys;
	use_gen_sample_entry = GF_TRUE;
	use_dref = ctx->dref;

	//get our subtype
	switch (codec_id) {
	case GF_CODECID_MPEG_AUDIO:
	case GF_CODECID_MPEG2_PART3:
		m_subtype = GF_ISOM_SUBTYPE_MP3;
		comp_name = "MP3";
		break;
	case GF_CODECID_AAC_MPEG4:
	case GF_CODECID_AAC_MPEG2_MP:
	case GF_CODECID_AAC_MPEG2_LCP:
	case GF_CODECID_AAC_MPEG2_SSRP:
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
	case GF_CODECID_JPEG:
		m_subtype = GF_ISOM_BOX_TYPE_JPEG;
		comp_name = "JPEG";
		break;
	case GF_CODECID_PNG:
		m_subtype = GF_ISOM_BOX_TYPE_PNG;
		comp_name = "PNG";
		break;
	case GF_CODECID_J2K:
		m_subtype = GF_ISOM_BOX_TYPE_JP2K;
		comp_name = "JP2K";
		break;

	case GF_CODECID_AMR:
		m_subtype = GF_ISOM_SUBTYPE_3GP_AMR;
		comp_name = "AMR";
		use_3gpp_config = GF_TRUE;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_AMR_MODE_SET);
		if (p) amr_mode_set = p->value.uint;
		break;
	case GF_CODECID_AMR_WB:
		m_subtype = GF_ISOM_SUBTYPE_3GP_AMR_WB;
		comp_name = "AMR-WB";
		use_3gpp_config = GF_TRUE;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_AMR_MODE_SET);
		if (p) amr_mode_set = p->value.uint;
		break;
	case GF_CODECID_EVRC:
		m_subtype = GF_ISOM_SUBTYPE_3GP_EVRC;
		comp_name = "EVRC";
		use_3gpp_config = GF_TRUE;
		break;
	case GF_CODECID_SMV:
		m_subtype = GF_ISOM_SUBTYPE_3GP_SMV;
		comp_name = "SMV";
		use_3gpp_config = GF_TRUE;
		break;
	case GF_CODECID_QCELP:
		m_subtype = GF_ISOM_SUBTYPE_3GP_QCELP;
		comp_name = "QCELP";
		use_3gpp_config = GF_TRUE;
		break;
	case GF_CODECID_S263:
	case GF_CODECID_H263:
		m_subtype = GF_ISOM_SUBTYPE_3GP_H263;
		comp_name = "H263";
		use_3gpp_config = GF_TRUE;
		break;
	case GF_CODECID_AC3:
		m_subtype = GF_ISOM_SUBTYPE_AC3;
		comp_name = "AC-3";
		use_ac3_entry = GF_TRUE;
		break;
	case GF_CODECID_EAC3:
		m_subtype = GF_ISOM_SUBTYPE_AC3;
		comp_name = "EAC-3";
		use_ac3_entry = GF_TRUE;
		break;
	case GF_CODECID_MPEG4_PART2:
		m_subtype = GF_ISOM_SUBTYPE_MPEG4;
		use_m4sys = GF_TRUE;
		comp_name = "MPEG-4 Visual Part 2";
		use_gen_sample_entry = GF_FALSE;
		break;
	case GF_CODECID_AVC:
	case GF_CODECID_SVC:
		m_subtype = (ctx->xps_inband==1) ? GF_ISOM_SUBTYPE_AVC3_H264 : GF_ISOM_SUBTYPE_AVC_H264;
		use_avc = GF_TRUE;
		comp_name = (codec_id == GF_CODECID_SVC) ? "MPEG-4 SVC" : "MPEG-4 AVC";
		use_gen_sample_entry = GF_FALSE;
		if (ctx->xps_inband==1) skip_dsi = GF_TRUE;
		break;
	case GF_CODECID_HEVC:
	case GF_CODECID_LHVC:
		m_subtype = (ctx->xps_inband==1) ? GF_ISOM_SUBTYPE_HEV1  : GF_ISOM_SUBTYPE_HVC1;
		use_hevc = GF_TRUE;
		comp_name = (codec_id == GF_CODECID_LHVC) ? "L-HEVC" : "HEVC";
		use_gen_sample_entry = GF_FALSE;
		if (ctx->xps_inband==1) skip_dsi = GF_TRUE;
		break;
	case GF_CODECID_MPEG1:
	case GF_CODECID_MPEG2_422:
	case GF_CODECID_MPEG2_SNR:
	case GF_CODECID_MPEG2_HIGH:
	case GF_CODECID_MPEG2_MAIN:
	case GF_CODECID_MPEG2_SIMPLE:
	case GF_CODECID_MPEG2_SPATIAL:
		m_subtype = GF_ISOM_SUBTYPE_MPEG4;
		use_m4sys = GF_TRUE;
		comp_name = "MPEG-2 Video";
		use_gen_sample_entry = GF_FALSE;
		break;
	case 0:
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] muxing codecID %d not yet implemented - patch welcome\n", codec_id));
		return GF_NOT_SUPPORTED;

	case GF_ISOM_SUBTYPE_TX3G:
		m_subtype = GF_ISOM_SUBTYPE_TX3G;
		use_tx3g = GF_TRUE;
		comp_name = "Timed Text";
		is_text_subs = GF_TRUE;
		break;
	case GF_ISOM_SUBTYPE_WVTT:
		m_subtype = GF_ISOM_SUBTYPE_WVTT;
		use_webvtt = GF_TRUE;
		comp_name = "WebVTT";
		is_text_subs = GF_TRUE;
		break;
	case GF_CODECID_SUBPIC:
		use_m4sys = GF_TRUE;
		override_stype = GF_STREAM_ND_SUBPIC;
		comp_name = "VobSub";
		break;

	default:
		m_subtype = codec_id;
		use_gen_sample_entry = GF_TRUE;
		use_m4sys = GF_FALSE;
		tkw->skip_bitrate_update = GF_TRUE;

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
	if (!comp_name) comp_name = gf_codecid_name(codec_id);
	if (!comp_name) comp_name = gf_4cc_to_str(m_subtype);

	if (!needs_sample_entry) return GF_OK;

	//we are fragmented, init movie done, we cannot update the sample description
	if (ctx->init_movie_done) {
		if (needs_sample_entry==1) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Cannot create a new sample description entry (codec change) for finalized movie in fragmented mode\n"));
			return GF_NOT_SUPPORTED;
		}

		//for AVC and HEVC, move to inband params if config changed
		if (use_avc && dsi) {
			if (tkw->avcc) gf_odf_avc_cfg_del(tkw->avcc);

			tkw->avcc = gf_odf_avc_cfg_read(dsi->value.data.ptr, dsi->value.data.size);

			if (enh_dsi) {
				if (tkw->svcc) gf_odf_avc_cfg_del(tkw->svcc);
				tkw->svcc = gf_odf_avc_cfg_read(enh_dsi->value.data.ptr, enh_dsi->value.data.size);
			}
			if (!ctx->xps_inband) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MP4Mux] AVC config update after movie has been finalized, moving all SPS/PPS inband (file might not be compliant\n"));
				ctx->xps_inband = 2;
			}
			mp4_mux_make_inband_header(ctx, tkw);
			return GF_OK;
		}
		else if (use_hevc && dsi) {
			if (tkw->hvcc) gf_odf_hevc_cfg_del(tkw->hvcc);
			tkw->hvcc = gf_odf_hevc_cfg_read(dsi->value.data.ptr, dsi->value.data.size,  (codec_id == GF_CODECID_LHVC) ? GF_TRUE : GF_FALSE);

			if (enh_dsi) {
				if (tkw->lvcc) gf_odf_hevc_cfg_del(tkw->lvcc);
				tkw->lvcc = gf_odf_hevc_cfg_read(enh_dsi->value.data.ptr, enh_dsi->value.data.size, GF_TRUE);
			}
			if (!ctx->xps_inband) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MP4Mux] HEVC config update after movie has been finalized, moving all SPS/PPS inband (file might not be compliant\n"));
				ctx->xps_inband = 2;
			}
			mp4_mux_make_inband_header(ctx, tkw);
			return GF_OK;
		}

		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Cannot create a new sample description entry (config changed) for finalized movie in fragmented mode\n"));
		return GF_NOT_SUPPORTED;
	}


	width = height = sr = nb_chan = z_order = txt_fsize = 0;
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
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_ZORDER);
	if (p) z_order = p->value.uint;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_SAMPLE_RATE);
	if (p) sr = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_NUM_CHANNELS);
	if (p) nb_chan = p->value.uint;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_LANGUAGE);
	if (p) lang_name = p->value.string;

	if (is_text_subs && !width && !height) {
		mp4_mux_get_video_size(ctx, &width, &height);
	}

	//little optim here: if no samples were added on the stream descritpion remove it
	if (!tkw->samples_in_stsd && tkw->stsd_idx) {
		gf_isom_remove_stream_description(ctx->file, tkw->track_num, tkw->stsd_idx);
	}

	if (!use_dref) src_url = NULL;


	//nope, create sample entry
	if (use_m4sys) {
		GF_ESD *esd = gf_odf_desc_esd_new(2);
		esd->decoderConfig->streamType = override_stype ? override_stype : tkw->stream_type;
		esd->decoderConfig->objectTypeIndication = gf_codecid_oti(codec_id);
		esd->slConfig->timestampResolution = tkw->timescale;
		if (dsi && !skip_dsi) {
			esd->decoderConfig->decoderSpecificInfo->data = dsi->value.data.ptr;
			esd->decoderConfig->decoderSpecificInfo->dataLength = dsi->value.data.size;
		}

		e = gf_isom_new_mpeg4_description(ctx->file, tkw->track_num, esd, (char *)src_url, NULL, &tkw->stsd_idx);
		if (dsi && !skip_dsi) {
			esd->decoderConfig->decoderSpecificInfo->data = NULL;
			esd->decoderConfig->decoderSpecificInfo->dataLength = 0;
		}
		gf_odf_desc_del((GF_Descriptor *) esd);

		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Error creating new MPEG-4 Systems sample description for stream type %d OTI %d: %s\n", tkw->stream_type, codec_id, gf_error_to_string(e) ));
			return e;
		}

		tkw->use_dref = src_url ? GF_TRUE : GF_FALSE;
	} else if (use_avc) {
		if (tkw->avcc) gf_odf_avc_cfg_del(tkw->avcc);

		//not yet known
		if (!dsi) return GF_OK;

/*		if (!dsi) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] No decoder specific info found for AVC\n"));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
*/

		tkw->avcc = gf_odf_avc_cfg_read(dsi->value.data.ptr, dsi->value.data.size);

		if (tkw->codecid == GF_CODECID_SVC) {
			e = gf_isom_svc_config_new(ctx->file, tkw->track_num, tkw->avcc, NULL, NULL, &tkw->stsd_idx);
		} else {
			e = gf_isom_avc_config_new(ctx->file, tkw->track_num, tkw->avcc, NULL, NULL, &tkw->stsd_idx);
		}

		if (!e && enh_dsi) {
			if (tkw->svcc) gf_odf_avc_cfg_del(tkw->svcc);
			tkw->svcc = gf_odf_avc_cfg_read(enh_dsi->value.data.ptr, enh_dsi->value.data.size);
			if (tkw->svcc) {
				e = gf_isom_svc_config_update(ctx->file, tkw->track_num, tkw->stsd_idx, tkw->svcc, GF_TRUE);
				if (e) {
					gf_odf_avc_cfg_del(tkw->svcc);
					tkw->svcc = NULL;
				}

				if (!dsi && ctx->xps_inband) {
					gf_isom_avc_set_inband_config(ctx->file, tkw->track_num, tkw->stsd_idx);
				}
			}
		}
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Error creating new AVC sample description: %s\n", gf_error_to_string(e) ));
			return e;
		}

		if (dsi && ctx->xps_inband) {
			//this will cleanup all PS in avcC / svcC
			gf_isom_avc_set_inband_config(ctx->file, tkw->track_num, tkw->stsd_idx);
		} else {
			gf_odf_avc_cfg_del(tkw->avcc);
			tkw->avcc = NULL;
		}

		//gf_isom_set_pl_indication(ctx->file, GF_ISOM_PL_VISUAL, 0x7F);
		gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_AVC1, 1);
		tkw->is_nalu = GF_TRUE;

		tkw->use_dref = GF_FALSE;
	} else if (use_hevc) {
		if (tkw->hvcc) gf_odf_hevc_cfg_del(tkw->hvcc);

		if (!dsi) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] No decoder specific info found for HEVC\n"));
			return GF_NON_COMPLIANT_BITSTREAM;
		}

		tkw->hvcc = gf_odf_hevc_cfg_read(dsi->value.data.ptr, dsi->value.data.size,  (codec_id == GF_CODECID_LHVC) ? GF_TRUE : GF_FALSE);

		e = gf_isom_hevc_config_new(ctx->file, tkw->track_num, tkw->hvcc, NULL, NULL, &tkw->stsd_idx);

		gf_isom_set_brand_info(ctx->file, GF_ISOM_BRAND_ISO4, 1);
		gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_ISOM, 0);
		gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_HVC1, 1);
		tkw->is_nalu = GF_TRUE;

		if (!e && enh_dsi) {
			if (tkw->lvcc) gf_odf_hevc_cfg_del(tkw->lvcc);
			tkw->lvcc = gf_odf_hevc_cfg_read(enh_dsi->value.data.ptr, enh_dsi->value.data.size, GF_TRUE);
			if (tkw->lvcc) {
				e = gf_isom_lhvc_config_update(ctx->file, tkw->track_num, tkw->stsd_idx, tkw->lvcc, GF_ISOM_LEHVC_WITH_BASE_BACKWARD);
				if (e) {
					gf_odf_hevc_cfg_del(tkw->lvcc);
					tkw->lvcc = NULL;
				}

				if (!dsi && ctx->xps_inband) {
					gf_isom_avc_set_inband_config(ctx->file, tkw->track_num, tkw->stsd_idx);
				}
			}
		} else if (codec_id == GF_CODECID_LHVC) {
			gf_isom_lhvc_config_update(ctx->file, tkw->track_num, tkw->stsd_idx, tkw->hvcc, GF_ISOM_LEHVC_ONLY);
		}
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Error creating new HEVC sample description: %s\n", gf_error_to_string(e) ));
			return e;
		}

		if (dsi && ctx->xps_inband) {
			//this will cleanup all PS in avcC / svcC
			gf_isom_hevc_set_inband_config(ctx->file, tkw->track_num, tkw->stsd_idx);
		} else {
			gf_odf_hevc_cfg_del(tkw->hvcc);
			tkw->hvcc = NULL;
		}

		tkw->use_dref = GF_FALSE;
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

		e = gf_isom_3gp_config_new(ctx->file, tkw->track_num, &gpp_cfg, (char *) src_url, NULL, &tkw->stsd_idx);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Error creating new 3GPP audio sample description for stream type %d codecid %d: %s\n", tkw->stream_type, codec_id, gf_error_to_string(e) ));
			return e;
		}
		tkw->use_dref = src_url ? GF_TRUE : GF_FALSE;

		if (gpp_cfg.type==GF_ISOM_SUBTYPE_3GP_QCELP) {
			gf_isom_set_brand_info(ctx->file, GF_ISOM_BRAND_3G2A, 65536);
		} else if (gpp_cfg.type==GF_ISOM_SUBTYPE_3GP_H263) {
			gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_3GG6, 1);
			gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_3GG5, 1);
		}
		tkw->skip_bitrate_update = GF_TRUE;
	} else if (use_ac3_entry) {
		GF_AC3Config ac3cfg;
		memset(&ac3cfg, 0, sizeof(GF_AC3Config));

		if (dsi) {
			GF_BitStream *bs = gf_bs_new(dsi->value.data.ptr, dsi->value.data.size, GF_BITSTREAM_READ);
			ac3cfg.nb_streams = 1;
			ac3cfg.streams[0].fscod = gf_bs_read_int(bs, 2);
			ac3cfg.streams[0].bsid = gf_bs_read_int(bs, 5);
			ac3cfg.streams[0].bsmod = gf_bs_read_int(bs, 3);
			ac3cfg.streams[0].acmod = gf_bs_read_int(bs, 3);
			ac3cfg.streams[0].lfon = gf_bs_read_int(bs, 1);
			ac3cfg.brcode = gf_bs_read_int(bs, 5);
			gf_bs_del(bs);
		}
		e = gf_isom_ac3_config_new(ctx->file, tkw->track_num, &ac3cfg, (char *)src_url, NULL, &tkw->stsd_idx);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Error creating new AC3 audio sample description for stream type %d codecid %d: %s\n", tkw->stream_type, codec_id, gf_error_to_string(e) ));
			return e;
		}
		tkw->use_dref = src_url ? GF_TRUE : GF_FALSE;
	} else if (m_subtype == GF_ISOM_SUBTYPE_METX) {
		comp_name = "XML Metadata";
		e = gf_isom_new_xml_metadata_description(ctx->file, tkw->track_num, meta_xmlns, meta_schemaloc, meta_encoding, &tkw->stsd_idx);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Error creating new METX sample description: %s\n", gf_error_to_string(e) ));
			return e;
		}
	} else if (m_subtype == GF_ISOM_SUBTYPE_METT) {
		comp_name = "Text Metadata";
		e = gf_isom_new_stxt_description(ctx->file, tkw->track_num, GF_ISOM_SUBTYPE_METT, meta_mime, meta_encoding, meta_config, &tkw->stsd_idx);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Error creating new METT sample description: %s\n", gf_error_to_string(e) ));
			return e;
		}
	} else if (m_subtype == GF_ISOM_SUBTYPE_STPP) {
		if (meta_xmlns && !strcmp(meta_xmlns, "http://www.w3.org/ns/ttml")) comp_name = "TTML";
		else comp_name = "XML Subtitle";
		e = gf_isom_new_xml_subtitle_description(ctx->file, tkw->track_num, meta_xmlns, meta_schemaloc, meta_auxmimes, &tkw->stsd_idx);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Error creating new XML subtitle sample description: %s\n", gf_error_to_string(e) ));
			return e;
		}
	} else if ((m_subtype == GF_ISOM_SUBTYPE_SBTT) || (m_subtype == GF_ISOM_SUBTYPE_STXT) ) {
		comp_name = (m_subtype == GF_ISOM_SUBTYPE_STXT) ? "Simple Timed Text" : "Textual Subtitle";
		e = gf_isom_new_stxt_description(ctx->file, tkw->track_num, m_subtype, meta_mime, meta_encoding, meta_config, &tkw->stsd_idx);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Error creating new %s sample description: %s\n", gf_4cc_to_str(m_subtype), gf_error_to_string(e) ));
			return e;
		}
	} else if (use_tx3g) {
		GF_TextSampleDescriptor *txtc;
		if (!dsi) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] No decoder specific info found for TX3G\n"));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		txtc = gf_odf_tx3g_read(dsi->value.data.ptr, dsi->value.data.size);
		if (!txtc) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Failed to parse TX3G config\n"));
			return GF_NON_COMPLIANT_BITSTREAM;
		}

		if (!txtc->default_pos.right) txtc->default_pos.right = width + txtc->default_pos.left;
		if (!txtc->default_pos.bottom) txtc->default_pos.bottom = height + txtc->default_pos.top;


		e = gf_isom_new_text_description(ctx->file, tkw->track_num, txtc, NULL, NULL, &tkw->stsd_idx);
		if (e) {
			gf_odf_desc_del((GF_Descriptor *)txtc);
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Error creating new %s sample description: %s\n", gf_4cc_to_str(m_subtype), gf_error_to_string(e) ));
			return e;
		}
		if (ctx->importer) {
			txt_fsize = txtc->default_style.font_size;
			if (txtc->font_count && txtc->fonts[0].fontName) txt_font = gf_strdup(txtc->fonts[0].fontName);
		}
		gf_odf_desc_del((GF_Descriptor *)txtc);

		tkw->skip_bitrate_update = GF_TRUE;
	} else if (use_webvtt) {
		e = gf_isom_new_webvtt_description(ctx->file, tkw->track_num, NULL, NULL, &tkw->stsd_idx, dsi ? dsi->value.data.ptr : NULL);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Error creating new %s sample description: %s\n", gf_4cc_to_str(m_subtype), gf_error_to_string(e) ));
			return e;
		}
		tkw->skip_bitrate_update = GF_TRUE;
	} else if (use_gen_sample_entry) {
		u32 len = 0;
		GF_GenericSampleDescription udesc;
		memset(&udesc, 0, sizeof(GF_GenericSampleDescription));
		udesc.codec_tag = m_subtype;
		if (!comp_name) comp_name = "Unknown";
		len = (u32) strlen(comp_name);
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

		e = gf_isom_new_generic_sample_description(ctx->file, tkw->track_num, (char *)src_url, NULL, &udesc, &tkw->stsd_idx);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Error creating new sample description for stream type %d codecid %d: %s\n", tkw->stream_type, codec_id, gf_error_to_string(e) ));
			return e;
		}
		tkw->use_dref = src_url ? GF_TRUE : GF_FALSE;
	} else {
		assert(0);
	}


multipid_stsd_setup:
	if (multi_pid_stsd) {
		if (multi_pid_idx<gf_list_count(multi_pid_stsd)) {

			if (multi_pid_final_stsd_idx == multi_pid_idx) {
				frames_per_sample_backup = tkw->nb_frames_per_sample;
				is_nalu_backup = tkw->is_nalu;
			}
			pid = gf_list_get(multi_pid_stsd, multi_pid_idx);
			multi_pid_idx ++;
			//reload codecID, decoder config and enhancement decoder config
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
			if (p) codec_id = p->value.uint;
			dsi = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
			enh_dsi = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT);
			//force stsd idx to be 0 to avoid removing the stsd
			tkw->stsd_idx = 0;
			goto sample_entry_setup;
		}
		tkw->stsd_idx = multi_pid_final_stsd_idx;
		//restore input pid
		pid = orig_pid;
		codec_id = tkw->codecid;

		tkw->is_nalu = is_nalu_backup;
		tkw->nb_frames_per_sample = frames_per_sample_backup;
	}


	//final opt: we couldn't detect before if the same stsd was possible, now that we have create a new one, check again
	reuse_stsd = 0;
	//don't try to reuse STSDs in multi STSD setup for DASH
	if (multi_pid_stsd) count = 0;
	else count = gf_isom_get_sample_description_count(ctx->file, tkw->track_num);
	for (i=0; i<count; i++) {
		if (i+1 == tkw->stsd_idx) continue;

		if (gf_isom_is_same_sample_description(ctx->file, tkw->track_num, tkw->stsd_idx, ctx->file, tkw->track_num, i+1) ) {
			gf_isom_remove_stream_description(ctx->file, tkw->track_num, tkw->stsd_idx);
			tkw->stsd_idx = i+1;
			reuse_stsd = 1;
			break;
		}
	}
	if (!reuse_stsd) {
		tkw->samples_in_stsd = 0;
	} else if (use_3gpp_config && amr_mode_set) {
		GF_3GPConfig *gpp_cfg = gf_isom_3gp_config_get(ctx->file, tkw->track_num, tkw->stsd_idx);
		if (gpp_cfg->AMR_mode_set != amr_mode_set) {
			gpp_cfg->AMR_mode_set = amr_mode_set;
			gf_isom_3gp_config_update(ctx->file, tkw->track_num, gpp_cfg, tkw->stsd_idx);
		}
		gf_free(gpp_cfg);
	}

	if (sr) gf_isom_set_audio_info(ctx->file, tkw->track_num, tkw->stsd_idx, sr, nb_chan, nb_bps);
	else if (width) {
		gf_isom_set_visual_info(ctx->file, tkw->track_num, tkw->stsd_idx, width, height);
		if (sar.den && (sar.num != sar.den)) {
			gf_isom_set_pixel_aspect_ratio(ctx->file, tkw->track_num, tkw->stsd_idx, sar.num, sar.den);
			width = width * sar.num / sar.den;
		}
		gf_isom_set_track_layout_info(ctx->file, tkw->track_num, width<<16, height<<16, 0, 0, z_order);
	}

	if (lang_name) gf_isom_set_media_language(ctx->file, tkw->track_num, (char*)lang_name);

	if (ctx->importer && !tkw->import_msg_header_done) {
		tkw->import_msg_header_done = GF_TRUE;
		if (!imp_name) imp_name = comp_name;
		if (sr) {
			GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("Importing %s - SampleRate %d Num Channels %d\n", imp_name, sr, nb_chan));
		} else if (is_text_subs) {
			if (txt_fsize || txt_font) {
				GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("Importing %s - Text track %d x %d font %s (size %d) layer %d\n", imp_name, width, height, txt_font ? txt_font : "unspecified", txt_fsize, z_order));
			} else {
				GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("Importing %s - Text track %d x %d layer %d\n", imp_name, width, height, z_order));

			}
		} else if (width) {
			GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("Importing %s - Width %d Height %d FPS %d/%d SAR %d/%u\n", imp_name, width, height, fps.num, fps.den, sar.num, sar.den));
		} else {
			GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("Importing %s\n", imp_name));
		}
		if (tkw->svcc) {
			AVCState avc;
			memset(&avc, 0, sizeof(AVCState));
			count = gf_list_count(tkw->svcc->sequenceParameterSets);
			for (i=0; i<count; i++) {
				GF_AVCConfigSlot *sl = gf_list_get(tkw->svcc->sequenceParameterSets, i);
				u8 nal_type = sl->data[0] & 0x1F;
				Bool is_subseq = (nal_type == GF_AVC_NALU_SVC_SUBSEQ_PARAM) ? GF_TRUE : GF_FALSE;
				s32 ps_idx = gf_media_avc_read_sps(sl->data, sl->size, &avc, is_subseq, NULL);
				if (ps_idx>=0) {
					GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("SVC Detected - SSPS ID %d - frame size %d x %d\n", ps_idx-GF_SVC_SSPS_ID_SHIFT, avc.sps[ps_idx].width, avc.sps[ps_idx].height ));

				}
			}
		}
	}
	if (txt_font) gf_free(txt_font);
	if (!ctx->xps_inband) {
		if (tkw->svcc) {
			gf_odf_avc_cfg_del(tkw->svcc);
			tkw->svcc = NULL;
		}
		if (tkw->lvcc) {
			gf_odf_hevc_cfg_del(tkw->lvcc);
			tkw->lvcc = NULL;
		}
	} else {
		mp4_mux_make_inband_header(ctx, tkw);
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

static void mp4_mux_done(GF_MP4MuxCtx *ctx);

GF_Err mp4_mux_process_sample(GF_MP4MuxCtx *ctx, TrackWriter *tkw, GF_FilterPacket *pck, Bool for_fragment)
{
	GF_Err e;
	u64 cts, prev_dts;
	u32 prev_size=0;
	u32 duration = 0;
	u32 timescale = 0;
	const GF_PropertyValue *subs = NULL;
	GF_FilterSAPType sap_type;


	timescale = gf_filter_pck_get_timescale(pck);

	subs = gf_filter_pck_get_property(pck, GF_PROP_PCK_SUBS);

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
			return GF_NON_COMPLIANT_BITSTREAM;
		} else {
			tkw->sample.DTS = cts;
		}
	} else {
		tkw->sample.CTS_Offset = (s32) ((s64) cts - (s64) tkw->sample.DTS);
	}
	tkw->sample.IsRAP = 0;
	sap_type = gf_filter_pck_get_sap(pck);
	if (sap_type==GF_FILTER_SAP_1)
		tkw->sample.IsRAP = SAP_TYPE_1;

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
		if (for_fragment) {
			e = gf_isom_fragment_add_sample(ctx->file, tkw->track_id, &tkw->sample, tkw->stsd_idx, duration, 0, 0, GF_TRUE);
		} else {
			e = gf_isom_add_sample_shadow(ctx->file, tkw->track_num, &tkw->sample);
		}
	}
	else if (tkw->use_dref) {
		u64 data_offset = gf_filter_pck_get_byte_offset(pck);
		if (data_offset != GF_FILTER_NO_BO) {
			e = gf_isom_add_sample_reference(ctx->file, tkw->track_num, tkw->stsd_idx, &tkw->sample, data_offset);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Failed to add sample DTS "LLU" as reference: %s\n", tkw->sample.DTS, gf_error_to_string(e) ));
			}
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Cannot add sample reference at DTS "LLU" , input sample data is not continous in source\n", tkw->sample.DTS ));
		}
	} else if (tkw->nb_frames_per_sample && (tkw->nb_samples % tkw->nb_frames_per_sample)) {
		if (for_fragment) {
		 	e = gf_isom_fragment_append_data(ctx->file, tkw->track_id, tkw->sample.data, tkw->sample.dataLength, 0);
		} else {
			e = gf_isom_append_sample_data(ctx->file, tkw->track_num, tkw->sample.data, tkw->sample.dataLength);
		}
		tkw->has_append = GF_TRUE;
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Failed to append sample DTS "LLU" data: %s\n", tkw->sample.DTS, gf_error_to_string(e) ));
		}
	} else {
		if (tkw->sample.IsRAP && ctx->xps_inband) {
			char *pck_data = tkw->sample.data;
			u32 pck_data_len = tkw->sample.dataLength;
			tkw->sample.data = tkw->inband_hdr;
			tkw->sample.dataLength = tkw->inband_hdr_size;

			if (for_fragment) {
				e = gf_isom_fragment_add_sample(ctx->file, tkw->track_id, &tkw->sample, tkw->stsd_idx, duration, 0, 0, 0);
				if (!e) e = gf_isom_fragment_append_data(ctx->file, tkw->track_id, pck_data, pck_data_len, 0);

				if (subs)
					gf_isom_fragment_add_subsample(ctx->file, tkw->track_id, 0, tkw->inband_hdr_size, 0, 0, 0);

			} else {
				e = gf_isom_add_sample(ctx->file, tkw->track_num, tkw->stsd_idx, &tkw->sample);
				if (!e) e = gf_isom_append_sample_data(ctx->file, tkw->track_num, pck_data, pck_data_len);

				if (subs)
					gf_isom_add_subsample(ctx->file, tkw->track_num, tkw->nb_samples+1, 0, tkw->inband_hdr_size, 0, 0, 0);

			}
		} else if (for_fragment) {
			e = gf_isom_fragment_add_sample(ctx->file, tkw->track_id, &tkw->sample, tkw->stsd_idx, duration, 0, 0, 0);
		} else {
			e = gf_isom_add_sample(ctx->file, tkw->track_num, tkw->stsd_idx, &tkw->sample);
		}

		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Failed to add sample DTS "LLU" - prev DTS "LLU": %s\n", tkw->sample.DTS, prev_dts, gf_error_to_string(e) ));
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MP4Mux] added sample DTS "LLU" - prev DTS "LLU" - prev size %d\n", tkw->sample.DTS, prev_dts, prev_size));
		}
	}

	tkw->nb_samples++;
	tkw->samples_in_stsd++;

	if (sap_type==3) {
		if (for_fragment) {
			e = gf_isom_fragment_set_sample_rap_group(ctx->file, tkw->track_id, 0);
		} else {
			e = gf_isom_set_sample_rap_group(ctx->file, tkw->track_num, tkw->nb_samples, 0);
		}
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Failed to set sample DTS "LLU" SAP 3 in RAP group: %s\n", tkw->sample.DTS, gf_error_to_string(e) ));
		}
		tkw->has_open_gop = GF_TRUE;
	} else if (sap_type==4) {
		s16 roll = gf_filter_pck_get_roll_info(pck);
		if (for_fragment) {
			e = gf_isom_fragment_set_sample_roll_group(ctx->file, tkw->track_id, 0, roll);
		} else {
			e = gf_isom_set_sample_roll_group(ctx->file, tkw->track_num, tkw->nb_samples, roll);
		}
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Failed to set sample DTS "LLU" SAP 4 roll %s in roll group: %s\n", tkw->sample.DTS, roll, gf_error_to_string(e) ));
		}
		tkw->has_gdr = GF_TRUE;
	}

	if (subs) {
		if (!ctx->bs_r) ctx->bs_r = gf_bs_new(subs->value.data.ptr, subs->value.data.size, GF_BITSTREAM_READ);
		else gf_bs_reassign_buffer(ctx->bs_r, subs->value.data.ptr, subs->value.data.size);

		while (gf_bs_available(ctx->bs_r)) {
			u32 flags = gf_bs_read_u32(ctx->bs_r);
			u32 subs_size = gf_bs_read_u32(ctx->bs_r);
			u32 reserved = gf_bs_read_u32(ctx->bs_r);
			u8 priority = gf_bs_read_u8(ctx->bs_r);
			u8 discardable = gf_bs_read_u8(ctx->bs_r);

			if (for_fragment) {
				gf_isom_fragment_add_subsample(ctx->file, tkw->track_id, flags, subs_size, priority, reserved, discardable);
			} else {
				gf_isom_add_subsample(ctx->file, tkw->track_num, tkw->nb_samples, flags, subs_size, priority, reserved, discardable);
			}
		}
	}

	tkw->next_is_first_sample = GF_FALSE;

	if (duration && !for_fragment)
		gf_isom_set_last_sample_duration(ctx->file, tkw->track_num, duration);

	if (ctx->dur.num) {
		u64 mdur = gf_isom_get_media_duration(ctx->file, tkw->track_num);

		if (ctx->importer) {
			gf_set_progress("Import", mdur * ctx->dur.den, ((u64)tkw->timescale) * ctx->dur.num);
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
			gf_set_progress("Import", data_offset, p->value.longuint);
		} else {
			p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_DURATION);
			if (p) {
				gf_set_progress("Import", tkw->sample.DTS, p->value.frac.num);
			} else {
				gf_set_progress("Import", 0, 1);
			}

		}
	}
	return GF_OK;
}

static void mp4_mux_flush_frag(GF_MP4MuxCtx *ctx, u32 type, u64 idx_start_range, u64 idx_end_range)
{
	GF_FilterEvent evt;
	TrackWriter *tkw = NULL;

	if (ctx->dst_pck) {
		if (!ctx->single_file) {
			Bool s, e;
			gf_filter_pck_get_framing(ctx->dst_pck, &s, &e);
			gf_filter_pck_set_framing(ctx->dst_pck, s, GF_TRUE);
			ctx->first_pck_sent = GF_FALSE;
		}
		gf_filter_pck_send(ctx->dst_pck);
		ctx->dst_pck = NULL;
	}

	if (ctx->dash_mode) {
		//send event on first track only
		tkw = gf_list_get(ctx->tracks, 0);
		GF_FEVT_INIT(evt, GF_FEVT_SEGMENT_SIZE, tkw->ipid);
		evt.seg_size.seg_url = NULL;
		evt.seg_size.is_init = type ? GF_TRUE : GF_FALSE;
		if (!type || !idx_end_range) {
			evt.seg_size.media_range_start = ctx->current_offset;
			evt.seg_size.media_range_end = ctx->current_offset + ctx->current_size - 1;
		}
		evt.seg_size.idx_range_start = idx_start_range;
		evt.seg_size.idx_range_end = idx_end_range;
		gf_filter_pid_send_event(tkw->ipid, &evt);

		ctx->current_offset += ctx->current_size;
		ctx->current_size = 0;
		//changing file
		if (ctx->seg_name) {
			ctx->first_pck_sent = GF_FALSE;
		}
	}
}

static GF_Err mp4_mux_process_fragmented(GF_Filter *filter, GF_MP4MuxCtx *ctx)
{
	GF_Err e = GF_OK;
	u32 nb_eos, nb_done, i, count = gf_list_count(ctx->tracks);

	if (!ctx->file)
		return GF_EOS;

	if (ctx->flush_size) {
		GF_FilterPacket *pck;
		char *output;
		u32 nb_read, blocksize = ctx->block_size;
		if (ctx->flush_done + blocksize>ctx->flush_size) {
			blocksize = ctx->flush_size - ctx->flush_done;
		}
		if (!blocksize) return GF_EOS;
		pck = gf_filter_pck_new_alloc(ctx->opid, blocksize, &output);
		nb_read = fread(output, 1, blocksize, ctx->tmp_store);
		ctx->flush_done += blocksize;
		if (ctx->flush_done==ctx->flush_size) {
			gf_filter_pck_set_framing(pck, GF_FALSE, GF_TRUE);
			gf_filter_pck_send(pck);
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		gf_filter_pck_set_framing(pck, GF_FALSE, GF_FALSE);
		gf_filter_pck_send(pck);
		//we are not done flushing but we have no more input packets, signal we still need processing
		gf_filter_post_process_task(filter);
		return GF_OK;
	}


	//init movie not yet produced
	if (!ctx->init_movie_done) {
		TrackWriter *ref_tkw = NULL;

		ctx->single_file = GF_TRUE;
		ctx->current_offset = ctx->current_size = 0;

		//make sure we have one sample from each PID. This will trigger potential pending reconfigure
		//for filters updating the PID caps before the first packet dispatch
		for (i=0; i<count; i++) {
			const GF_PropertyValue *p;
			TrackWriter *tkw = gf_list_get(ctx->tracks, i);
			GF_FilterPacket *pck = gf_filter_pid_get_packet(tkw->ipid);
			if (!pck) return GF_OK;

			p = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENAME);
			if (p && strlen(p->value.string)) ctx->single_file = GF_FALSE;

		}
		
		//good to go, finalize for fragments
		for (i=0; i<count; i++) {
			u32 def_pck_dur;
			u32 def_is_rap;
			const GF_PropertyValue *p;
			TrackWriter *tkw = gf_list_get(ctx->tracks, i);
			GF_FilterPacket *pck = gf_filter_pid_get_packet(tkw->ipid);
			assert(pck);

			//otherwise setup fragmentation, using first sample desc as default idx
			//first pck dur as default
			def_pck_dur = gf_filter_pck_get_duration(pck);
			//and consider audio & text all RAPs, the rest not rap - this will need refinement later on
			//but won't break the generated files
			switch (tkw->stream_type) {
			case GF_STREAM_AUDIO:
			case GF_STREAM_TEXT:
				def_is_rap = GF_TRUE;
				break;
			case GF_STREAM_VISUAL:
				switch (tkw->codecid) {
				case GF_CODECID_PNG:
				case GF_CODECID_JPEG:
				case GF_CODECID_J2K:
					break;
				default:
					if (!ref_tkw) ref_tkw = tkw;
					break;
				}
				def_is_rap = GF_FALSE;
				break;

			default:
				def_is_rap = GF_FALSE;
				break;
			}

			p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_MEDIA_SKIP);
			if (p) {
				if (p->value.sint > 0) {
					gf_isom_set_edit_segment(ctx->file, tkw->track_num, 0, 0, p->value.uint, GF_ISOM_EDIT_NORMAL);
				} else if (p->value.sint < 0) {
					s64 dur = p->value.sint * -1;
					dur *= ctx->timescale;
					dur /= tkw->timescale;
					gf_isom_set_edit_segment(ctx->file, tkw->track_num, 0, dur, p->value.uint, GF_ISOM_EDIT_DWELL);
					gf_isom_set_edit_segment(ctx->file, tkw->track_num, dur, 0, 0, GF_ISOM_EDIT_NORMAL);
				}
				tkw->ts_delay = p->value.uint;
			} else {
				//this could be refine, but since we have no info on cts adjustments we may end up with wrong values
				gf_isom_set_composition_offset_mode(ctx->file, tkw->track_num, GF_TRUE);
				gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_ISO4, 1);
			}

			mp4_mux_set_hevc_groups(ctx, tkw);

			//use 1 for the default sample description index. If no multi stsd, this is always the case
			//otherwise we need to the stsd idx in the traf headers
			if (! ctx->no_def) {
				e = gf_isom_setup_track_fragment(ctx->file, tkw->track_id, 1, def_pck_dur, 0, (u8) def_is_rap, 0, 0);
			} else {
				e = gf_isom_setup_track_fragment(ctx->file, tkw->track_id, 1, 0, 0, 0, 0, 0);
			}


			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Unable to setup fragmentation for track ID %d: %s\n", tkw->track_id, gf_error_to_string(e) ));
				return e;
			}

			if (ctx->tfdt.den && ctx->tfdt.num) {
				tkw->offset_dts = ctx->tfdt.num * tkw->timescale;
				tkw->offset_dts /= ctx->tfdt.den;
			}
		}
		//if we have an explicit track reference for fragmenting, move it first in our list
		if (ref_tkw) {
			gf_list_del_item(ctx->tracks, ref_tkw);
			gf_list_insert(ctx->tracks, ref_tkw, 0);
		}
		ctx->ref_tkw = gf_list_get(ctx->tracks, 0);

		if (!ctx->abs_offset) {
			gf_isom_set_fragment_option(ctx->file, 0, GF_ISOM_TFHD_FORCE_MOOF_BASE_OFFSET, 1);
			gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_ISO5, 1);
		}

		e = gf_isom_finalize_for_fragment(ctx->file, ctx->dash_mode ? 1 : 0);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Unable to finalize moov for fragmentation: %s\n", gf_error_to_string(e) ));
			return e;
		}
		ctx->init_movie_done = GF_TRUE;
		ctx->next_frag_start = ctx->cdur;
		ctx->adjusted_next_frag_start = ctx->next_frag_start;
		ctx->fragment_started = GF_FALSE;

		if (ctx->noinit) {
			if (ctx->dst_pck) gf_filter_pck_discard(ctx->dst_pck);
			ctx->dst_pck = NULL;
			ctx->current_size = ctx->current_offset = 0;
			ctx->first_pck_sent = GF_FALSE;
		} else {
			mp4_mux_flush_frag(ctx, 1, 0, 0);
		}
		assert(!ctx->dst_pck);

		gf_isom_set_next_moof_number(ctx->file, ctx->moof_sn);

		if (ctx->dash_mode==MP4MX_DASH_VOD) {
			if (!ctx->cache && (!ctx->media_dur || !ctx->dash_dur) ) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MP4Mux] Media duration unknown, cannot use nocache mode, using temp file for VoD storage\n"));
				ctx->cache = GF_TRUE;
				e = mp4mx_setup_dash_vod(ctx, NULL);
				if (e) return e;
			}

			if (!ctx->cache) {
				GF_BitStream *bs;
				char *output, *msg;
				GF_FilterPacket *pck;
				u32 len, nb_segs = ctx->media_dur / ctx->dash_dur;
				//always add an extra segment
				nb_segs ++;
				//and safety alloc of 10%
				if (nb_segs>10)
					nb_segs += 10*nb_segs/100;
				else
					nb_segs ++;

				//max sidx size: full box + sidx fields + timing 64 bit + nb segs (each 12 bytes)
				ctx->sidx_max_size = 12 + 12 + 16 + 12 * nb_segs;
				//and a free box
				ctx->sidx_max_size += 8;
				ctx->sidx_chunk_offset = ctx->current_offset + ctx->current_size;
				//send a dummy packet
				pck = gf_filter_pck_new_alloc(ctx->opid, ctx->sidx_max_size, &output);
				gf_filter_pck_set_framing(pck, GF_FALSE, GF_FALSE);
				//format as free box for now
				bs = gf_bs_new(output, ctx->sidx_max_size, GF_BITSTREAM_WRITE);
				gf_bs_write_u32(bs, ctx->sidx_max_size);
				gf_bs_write_u32(bs, GF_ISOM_BOX_TYPE_FREE);
				msg = "GPAC " GPAC_VERSION" SIDX placeholder";
				len = strlen(msg);
				if (len+8>ctx->sidx_max_size) len = ctx->sidx_max_size - 8;
				gf_bs_write_data(bs, msg, len );
				gf_bs_del(bs);
				gf_filter_pck_send(pck);
				gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DISABLE_PROGRESSIVE, &PROP_BOOL(GF_TRUE));
			} else {
				ctx->store_output = GF_TRUE;
			}
			gf_isom_allocate_sidx(ctx->file, ctx->subs_sidx, ctx->chain_sidx, 0, NULL, NULL, NULL);
		}
	}
	assert(ctx->init_movie_done);

	if (ctx->dash_mode && !ctx->segment_started) {
		ctx->segment_started = GF_TRUE;
		ctx->insert_tfdt = GF_TRUE;
		gf_isom_start_segment(ctx->file, ctx->single_file ? NULL : "_gpac_isobmff_redirect", GF_FALSE);
	}

	if (!ctx->fragment_started) {
		e = gf_isom_start_fragment(ctx->file, ctx->moof_first);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Unable to start new fragment: %s\n", gf_error_to_string(e) ));
			return e;
		}
		//setup some default
		for (i=0; i<count; i++) {
			TrackWriter *tkw = gf_list_get(ctx->tracks, i);
			e = GF_OK;
			if (ctx->fsap && (tkw->stream_type == GF_STREAM_VISUAL)) {
				e = gf_isom_set_fragment_option(ctx->file, tkw->track_id, GF_ISOM_TRAF_RANDOM_ACCESS, 1);
				if (e) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MP4Mux] Unable set fragment options: %s\n", gf_error_to_string(e) ));
				}
			}
			tkw->fragment_done = GF_FALSE;
			tkw->insert_tfdt = ctx->insert_tfdt;
		}
		ctx->fragment_started = GF_TRUE;
		ctx->insert_tfdt = GF_FALSE;
	}

	//process pid by pid
	nb_eos=0;
	nb_done = 0;
	for (i=0; i<count; i++) {
		u64 cts, dts;
		TrackWriter *tkw = gf_list_get(ctx->tracks, i);

		if (tkw->fragment_done) {
			nb_done ++;
			continue;
		}

		while (1) {
			GF_FilterPacket *pck = gf_filter_pid_get_packet(tkw->ipid);

			if (!pck) {
				if (gf_filter_pid_is_eos(tkw->ipid)) {
					tkw->fragment_done = GF_TRUE;
					nb_done ++;
					nb_eos++;
					break;
				}
				return GF_OK;
			}
			if (tkw->aborted) {
				gf_filter_pid_drop_packet(tkw->ipid);
				nb_eos++;
				nb_done ++;
				tkw->fragment_done = GF_TRUE;
				break;
			}

			cts = gf_filter_pck_get_cts(pck);
			assert(cts != GF_FILTER_NO_TS);

			if (ctx->dash_mode) {
				//get dash segment number
				const GF_PropertyValue *p = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENUM);
				if (p) {
					//start of next segment, abort fragmentation for this track and flush all other writers
					if (ctx->dash_seg_num && (ctx->dash_seg_num != p->value.uint)) {
						tkw->fragment_done = GF_TRUE;
						nb_done ++;
						//make sure we flush until the end of the segment
						ctx->flush_seg = GF_TRUE;
						//store CTS of next packet (first in next segment) for sidx compute
						tkw->next_seg_cts = cts;

						break;
					}
					//start of current segment, remember segment number and name
					ctx->dash_seg_num = p->value.uint;
					//get file name prop if any - only send on one pid for muxed content
					p = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENAME);
					if (p && p->value.string) {
						if (ctx->seg_name) gf_free(ctx->seg_name);
						ctx->seg_name = gf_strdup(p->value.string);
					}
					//store PRFT only for reference track at segment start
					if (tkw==ctx->ref_tkw) {
						p = gf_filter_pck_get_property(pck, GF_PROP_PCK_SENDER_NTP);
						if (p) {
							gf_isom_set_fragment_reference_time(ctx->file, tkw->track_id, p->value.longuint, cts);
							GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MuxIsom] Segment %s, storing NTP TS "LLU" for CTS "LLU" at "LLU" us, at UTC "LLU"\n", ctx->seg_name ? ctx->seg_name : "singlefile", p->value.longuint, cts, gf_sys_clock_high_res(), gf_net_get_utc()));
						}
					}
				}

				dts = gf_filter_pck_get_dts(pck);
				if (dts==GF_FILTER_NO_TS) dts = cts;
				if (tkw->first_dts_in_seg > dts)
					tkw->first_dts_in_seg = dts;
			}
			tkw->cts_next = cts;
			tkw->cts_next += gf_filter_pck_get_duration(pck);


			if (!ctx->flush_seg
				&& (cts >= ((u64) (ctx->adjusted_next_frag_start * tkw->timescale)) + tkw->ts_delay)
			 ) {
				u32 sap = gf_filter_pck_get_sap(pck);
				if ((ctx->mode==MP4MX_MODE_FRAG) || (sap && sap<GF_FILTER_SAP_3)) {
					tkw->fragment_done = GF_TRUE;
					nb_done ++;
					if (ctx->mode==MP4MX_MODE_SFRAG) {
						ctx->adjusted_next_frag_start = (Double) (cts - tkw->ts_delay);
						ctx->adjusted_next_frag_start /= tkw->timescale;
					}
					break;
				}
			}
			if (tkw->insert_tfdt) {
				u64 dts = gf_filter_pck_get_dts(pck);
				if (dts==GF_FILTER_NO_TS)
					dts = gf_filter_pck_get_cts(pck);

				if (tkw->offset_dts) dts += tkw->offset_dts;

				tkw->insert_tfdt = GF_FALSE;
				gf_isom_set_traf_base_media_decode_time(ctx->file, tkw->track_id, dts);
				tkw->first_dts_in_seg = (u64) dts;
			}

			//process packet
			mp4_mux_process_sample(ctx, tkw, pck, GF_TRUE);

			//discard
			gf_filter_pid_drop_packet(tkw->ipid);
		}
		//done with this track - if single track per moof, request new fragment but don't touch the
		//fragmentation state of the track writers
		if (ctx->straf && (i+1==count)) {
			e = gf_isom_start_fragment(ctx->file, ctx->moof_first);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Unable to start new fragment: %s\n", gf_error_to_string(e) ));
				return e;
			}
		}
	}
	if (nb_done==count) {
		Bool is_eos = (count == nb_eos) ? GF_TRUE : GF_FALSE;
		u64 next_ref_ts = ctx->ref_tkw->next_seg_cts;
		if (is_eos) next_ref_ts = ctx->ref_tkw->cts_next;

		ctx->next_frag_start += ctx->cdur;
		while (ctx->next_frag_start <= ctx->adjusted_next_frag_start) {
			ctx->next_frag_start += ctx->cdur;
		}
		ctx->adjusted_next_frag_start = ctx->next_frag_start;
		ctx->fragment_started = GF_FALSE;

		//end of DASH segment
		if (ctx->dash_mode && (ctx->flush_seg || is_eos) ) {
			u64 offset = ctx->current_offset;
			u64 idx_start_range, idx_end_range, segment_size_in_bytes;
			idx_start_range = idx_end_range = 0;

			gf_isom_close_segment(ctx->file, (ctx->subs_sidx >= 0) ? ctx->subs_sidx : 0, (ctx->subs_sidx>=0) ? ctx->ref_tkw->track_id : 0, ctx->ref_tkw->first_dts_in_seg, ctx->ref_tkw->ts_delay, next_ref_ts, ctx->chain_sidx, is_eos, GF_FALSE, ctx->eos_marker, &idx_start_range, &idx_end_range, &segment_size_in_bytes);

			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[MP4Mux] Done writing segment %d - next fragment start time %g\n", ctx->dash_seg_num, ctx->next_frag_start ));

			if (ctx->dash_mode != MP4MX_DASH_VOD) {
				mp4_mux_flush_frag(ctx, 0, offset + idx_start_range, idx_end_range ? offset + idx_end_range : 0);
			}
		}
		//cannot flush in DASH mode if using sidx (vod single sidx or live 1 sidx/seg)
		else if (!ctx->dash_mode || (!ctx->subs_sidx && (ctx->dash_mode<MP4MX_DASH_VOD) )) {
			gf_isom_flush_fragments(ctx->file, GF_FALSE);
			mp4_mux_flush_frag(ctx, 0, 0, 0);

			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[MP4Mux] Done writing fragment - next fragment start time %g\n", ctx->next_frag_start ));
		}

		if (ctx->flush_seg) {
			ctx->segment_started = GF_FALSE;
			ctx->flush_seg = GF_FALSE;
			ctx->dash_seg_num = 0;
			ctx->nb_segs++;
		}
	}

	if (count == nb_eos) {
		if (ctx->dash_mode==MP4MX_DASH_VOD) {
			if (!ctx->cache) {
				ctx->final_sidx_flush = GF_TRUE;
				//flush SIDX in given space, reserve 8 bytes for free box
				gf_isom_flush_sidx(ctx->file, ctx->sidx_max_size - 8);
			} else {
				//reenable packet dispatch
				ctx->store_output = GF_FALSE;
				gf_isom_flush_sidx(ctx->file, 0);
				//flush sidx packet
				if (ctx->dst_pck) gf_filter_pck_send(ctx->dst_pck);
				ctx->dst_pck = NULL;
				mp4_mux_flush_frag(ctx, 1, ctx->current_offset, ctx->current_offset + ctx->current_size - 1);

				ctx->flush_size = gf_ftell(ctx->tmp_store);
				ctx->flush_done = 0;
				gf_fseek(ctx->tmp_store, 0, SEEK_SET);
			}
		}
		if (ctx->file) {
			gf_isom_close(ctx->file);
			ctx->file = NULL;
			if (!ctx->flush_size) gf_filter_pid_set_eos(ctx->opid);
		}
		return ctx->flush_size ? GF_OK : GF_EOS;
	}

	return GF_OK;
}

GF_Err mp4_mux_process(GF_Filter *filter)
{
	GF_MP4MuxCtx *ctx = gf_filter_get_udta(filter);
	u32 nb_eos, i, count = gf_list_count(ctx->tracks);

	nb_eos = 0;

	//in frag mode, force fetching a sample from each track before processing
	if (ctx->mode>=MP4MX_MODE_FRAG) return mp4_mux_process_fragmented(filter, ctx);


	for (i=0; i<count; i++) {
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
		mp4_mux_process_sample(ctx, tkw, pck, GF_FALSE);

		gf_filter_pid_drop_packet(tkw->ipid);
	}

	if (count == nb_eos) {
		mp4_mux_done(ctx);
		return GF_EOS;
	}

	return GF_OK;
}

static GF_Err mp4_mux_on_data(void *cbk, char *data, u32 block_size)
{
	GF_Filter *filter = (GF_Filter *) cbk;
	char *output;
	GF_MP4MuxCtx *ctx = gf_filter_get_udta(filter);

	//flush pending packet in frag mode
	if (ctx->dst_pck) gf_filter_pck_send(ctx->dst_pck);
	ctx->dst_pck = NULL;

	if (ctx->final_sidx_flush) {
		GF_FilterPacket *pck;
		GF_BitStream *bs;
		u32 free_size;
		assert(!ctx->dst_pck);
		if (block_size > ctx->sidx_max_size) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Final SIDX chunk larger than preallocated block, will not flush SIDX (output file still readable). Try disabling nocache mode\n"));
			return GF_OK;
		}
		free_size = ctx->sidx_max_size - block_size;
		pck = gf_filter_pck_new_alloc(ctx->opid, ctx->sidx_max_size, &output);
		gf_filter_pck_set_framing(pck, GF_FALSE, GF_FALSE);
		gf_filter_pck_set_byte_offset(pck, ctx->sidx_chunk_offset);
		gf_filter_pck_set_seek_flag(pck, GF_TRUE);
		bs = gf_bs_new(output, ctx->sidx_max_size, GF_BITSTREAM_WRITE);
		gf_bs_write_u32(bs, free_size);
		gf_bs_write_u32(bs, GF_ISOM_BOX_TYPE_FREE);
		gf_bs_skip_bytes(bs, free_size-8);
		gf_bs_write_data(bs, data, block_size);
		gf_bs_del(bs);
		gf_filter_pck_send(pck);
		mp4_mux_flush_frag(ctx, 1, ctx->sidx_chunk_offset+free_size, ctx->sidx_chunk_offset+free_size + block_size - 1);
		return GF_OK;
	}

	if (ctx->store_output) {
		u32 nb_write = fwrite(data, 1, block_size, ctx->tmp_store);
		if (nb_write != block_size) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Error writing to temp cache: %d bytes write instead of %d\n", nb_write, block_size));
			return GF_IO_ERR;
		}
		return GF_OK;
	}

	//allocate new one
	ctx->dst_pck = gf_filter_pck_new_alloc(ctx->opid, block_size, &output);
	if (!ctx->dst_pck) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Cannot allocate output packets, lost %d bytes\n", block_size));
	} else {
		memcpy(output, data, block_size);
		gf_filter_pck_set_framing(ctx->dst_pck, !ctx->first_pck_sent, GF_FALSE);
	}
	//set packet prop as string since we may discard the seg_name  packet before this packet is processed
	if (!ctx->first_pck_sent && ctx->seg_name) {
		gf_filter_pck_set_property(ctx->dst_pck, GF_PROP_PCK_FILENAME, &PROP_STRING(ctx->seg_name) );
	}
	ctx->first_pck_sent = GF_TRUE;
	ctx->current_size += block_size;
	//non-frag mode, send right away
	if (ctx->mode<MP4MX_MODE_FRAG) {
		if (ctx->dst_pck) gf_filter_pck_send(ctx->dst_pck);
		ctx->dst_pck = NULL;
	}
	return GF_OK;
}

static GF_Err mp4_mux_initialize(GF_Filter *filter)
{
	GF_MP4MuxCtx *ctx = gf_filter_get_udta(filter);
	gf_filter_sep_max_extra_input_pids(filter, -1);

	if (ctx->pssh_moof) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] PSSH in moof not yet supported\n"));
		return GF_NOT_SUPPORTED;
	}

	if (ctx->file) {
		if (gf_isom_get_mode(ctx->file) < GF_ISOM_OPEN_WRITE) return GF_BAD_PARAM;
		if (ctx->mode>=MP4MX_MODE_FRAG) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Cannot use fragmented output on already opened ISOBMF file\n"));
			return GF_BAD_PARAM;
		}
		ctx->owns_mov = GF_FALSE;
	} else {
		u32 open_mode = GF_ISOM_OPEN_WRITE;
		ctx->owns_mov = GF_TRUE;
		switch (ctx->mode) {
		case MP4MX_MODE_INTER:
		case MP4MX_MODE_TIGHT:
			open_mode = GF_ISOM_WRITE_EDIT;
			break;
		}
		ctx->file = gf_isom_open("_gpac_isobmff_redirect", open_mode, ctx->tmp_dir);
		if (!ctx->file) return GF_OUT_OF_MEM;
		gf_isom_set_write_callback(ctx->file, mp4_mux_on_data, filter, ctx->block_size);

		if (ctx->for_test)
		    gf_isom_no_version_date_info(ctx->file, 1);


		if (ctx->dref && (ctx->mode>=MP4MX_MODE_FRAG)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MP4Mux] Cannot use data reference in movie fragments, not supported. Ignoring it\n"));
			ctx->dref = GF_FALSE;
		}
	}
	if (!ctx->timescale) ctx->timescale=600;
	gf_isom_set_timescale(ctx->file, ctx->timescale);
	ctx->tracks = gf_list_new();

	if (ctx->m4cc) {
		if (strlen(ctx->m4cc)==4)
			ctx->eos_marker = GF_4CC(ctx->m4cc[0], ctx->m4cc[1], ctx->m4cc[2], ctx->m4cc[3]);
		else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MP4Mux] Invalid segment marker 4cc %s, ignoring\n", ctx->m4cc));
		}
	}
	return GF_OK;
}


static void mp4_mux_update_edit_list_for_bframes(GF_MP4MuxCtx *ctx, TrackWriter *tkw)
{
	u32 i, count, di;
	u64 max_cts, min_cts, doff;

	count = gf_isom_get_sample_count(ctx->file, tkw->track_num);
	max_cts = 0;
	min_cts = (u64) -1;
	for (i=0; i<count; i++) {
		GF_ISOSample *s = gf_isom_get_sample_info(ctx->file, tkw->track_num, i+1, &di, &doff);
		if (s->DTS + s->CTS_Offset > max_cts)
			max_cts = s->DTS + s->CTS_Offset;

		if (min_cts > s->DTS + s->CTS_Offset)
			min_cts = s->DTS + s->CTS_Offset;

		gf_isom_sample_del(&s);
	}

	if (min_cts) {
		max_cts -= min_cts;
		max_cts += gf_isom_get_sample_duration(ctx->file, tkw->track_num, count);

		max_cts *= ctx->timescale;
		max_cts /= tkw->timescale;
		gf_isom_set_edit_segment(ctx->file, tkw->track_num, 0, max_cts, min_cts, GF_ISOM_EDIT_NORMAL);
	}
}

//todo: move from media_import.c to here once done
void gf_media_update_bitrate(GF_ISOFile *file, u32 track);


static void mp4_mux_set_lhvc_base_layer(GF_MP4MuxCtx *ctx, TrackWriter *tkw)
{
	u32 avc_base_track, hevc_base_track, ref_track_id;
	avc_base_track = hevc_base_track = 0;
	u32 i;
	for (i=0; i < gf_isom_get_track_count(ctx->file); i++) {
		u32 subtype = gf_isom_get_media_subtype(ctx->file, i+1, 1);
		switch (subtype) {
		case GF_ISOM_SUBTYPE_AVC_H264:
		case GF_ISOM_SUBTYPE_AVC2_H264:
		case GF_ISOM_SUBTYPE_AVC3_H264:
		case GF_ISOM_SUBTYPE_AVC4_H264:
			if (!avc_base_track) {
				avc_base_track = i+1;
			} else {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MP4Mux] Warning: More than one AVC bitstream found, use track %d as base layer", avc_base_track));
			}
			break;
		case GF_ISOM_SUBTYPE_HVC1:
		case GF_ISOM_SUBTYPE_HEV1:
		case GF_ISOM_SUBTYPE_HVC2:
		case GF_ISOM_SUBTYPE_HEV2:
			if (!hevc_base_track) {
				hevc_base_track = i+1;
				if (avc_base_track) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MP4Mux] Warning: Found both AVC and HEVC tracks, using HEVC track %d as base layer\n", hevc_base_track));
				}
			} else {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MP4Mux] Warning: More than one HEVC bitstream found, use track %d as base layer\n", avc_base_track));
			}
			break;
		}
	}
	if (!hevc_base_track && !avc_base_track) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MP4Mux] Using LHVC external base layer, but no base layer not found - NOT SETTING SBAS TRACK REFERENCE!\n"));
	} else {
		ref_track_id = gf_isom_get_track_id(ctx->file, hevc_base_track ? hevc_base_track : avc_base_track);
		gf_isom_set_track_reference(ctx->file, tkw->track_num, GF_ISOM_REF_BASE, ref_track_id);
	}
}

static void mp4_mux_set_hevc_groups(GF_MP4MuxCtx *ctx, TrackWriter *tkw)
{
	const GF_PropertyValue *p = gf_filter_pid_get_property_str(tkw->ipid, "hevc:oinf");
	if (p) {
		u32 gi=0;
		gf_isom_add_sample_group_info(ctx->file, tkw->track_num, GF_ISOM_SAMPLE_GROUP_OINF, p->value.data.ptr, p->value.data.size, GF_TRUE, &gi);

		p = gf_filter_pid_get_property_str(tkw->ipid, "hevc:linf");
		if (p) {
			gf_isom_add_sample_group_info(ctx->file, tkw->track_num, GF_ISOM_SAMPLE_GROUP_LINF, p->value.data.ptr, p->value.data.size, GF_TRUE, &gi);
		}

		//sets track in group of type group_type and id track_group_id. If do_add is GF_FALSE, track is removed from that group
		gf_isom_set_track_group(ctx->file, tkw->track_num, 1000+gf_isom_get_track_id(ctx->file, tkw->track_num), GF_ISOM_BOX_TYPE_CSTG, GF_TRUE);
	}

	p = gf_filter_pid_get_property_str(tkw->ipid, "hevc:min_lid");
	if (p && p->value.uint) {
		mp4_mux_set_lhvc_base_layer(ctx, tkw);
	}

}

static void mp4_mux_done(GF_MP4MuxCtx *ctx)
{
	u32 i, count;
	const GF_PropertyValue *p;

	count = gf_list_count(ctx->tracks);
	for (i=0; i<count; i++) {
		Bool has_bframes = GF_FALSE;
		TrackWriter *tkw = gf_list_get(ctx->tracks, i);

		//keep the old importer behaviour: use ctts v0
		if (tkw->min_neg_ctts<0) {
			gf_isom_set_cts_packing(ctx->file, tkw->track_num, GF_TRUE);
			gf_isom_shift_cts_offset(ctx->file, tkw->track_num, (s32) tkw->min_neg_ctts);
			gf_isom_set_cts_packing(ctx->file, tkw->track_num, GF_FALSE);
			gf_isom_set_composition_offset_mode(ctx->file, tkw->track_num, GF_FALSE);

			if (! ctx->noedit)
				mp4_mux_update_edit_list_for_bframes(ctx, tkw);

			has_bframes = GF_TRUE;
		} else if (tkw->has_ctts && (tkw->stream_type==GF_STREAM_VISUAL) ) {
			if (! ctx->noedit)
				mp4_mux_update_edit_list_for_bframes(ctx, tkw);

			has_bframes = GF_TRUE;
		}

		/*this is plain ugly but since some encoders (divx) don't use the video PL correctly
		 we force the system video_pl to ASP@L5 since we have I, P, B in base layer*/
		if (tkw->codecid == GF_CODECID_MPEG4_PART2) {
			Bool force_rewrite = GF_FALSE;
			u32 PL = tkw->media_profile_level;
			if (!PL) PL = 0x01;

			if (ctx->importer) {
				GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("Indicated Profile: %s\n", gf_m4v_get_profile_name((u8) PL) ));
			}

			if (has_bframes && (tkw->media_profile_level <= 3)) {
				PL = 0xF5;
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MP4Mux] Indicated profile doesn't include B-VOPs - forcing %s", gf_m4v_get_profile_name((u8) PL) ));
				force_rewrite = GF_TRUE;
			}
			if (PL != tkw->media_profile_level) {
				if (force_rewrite) {
					GF_ESD *esd = gf_isom_get_esd(ctx->file, tkw->track_num, tkw->stsd_idx);
					assert(esd);
					gf_m4v_rewrite_pl(&esd->decoderConfig->decoderSpecificInfo->data, &esd->decoderConfig->decoderSpecificInfo->dataLength, (u8) PL);
					gf_isom_change_mpeg4_description(ctx->file, tkw->track_num, tkw->stsd_idx, esd);
					gf_odf_desc_del((GF_Descriptor*)esd);
				}
				gf_isom_set_pl_indication(ctx->file, GF_ISOM_PL_VISUAL, PL);
			}
		}


		if (tkw->has_append)
			gf_isom_refresh_size_info(ctx->file, tkw->track_num);

		if ((tkw->nb_samples == 1) && ctx->dur.num && ctx->dur.den) {
			u32 dur = tkw->timescale * ctx->dur.num;
			dur /= ctx->dur.den;
			gf_isom_set_last_sample_duration(ctx->file, tkw->track_num, dur);
		}

		if (!tkw->skip_bitrate_update)
			gf_media_update_bitrate(ctx->file, tkw->track_num);

		if (tkw->has_open_gop) {
//			GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("OpenGOP detected - adjusting file brand\n"));
//			gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_ISO6, 1);
		}

		mp4_mux_set_hevc_groups(ctx, tkw);

		p = gf_filter_pid_get_property_str(tkw->ipid, "ttxt:rem_last");
		if (p && p->value.boolean)
			gf_isom_remove_sample(ctx->file, tkw->track_num, tkw->nb_samples);

		p = gf_filter_pid_get_property_str(tkw->ipid, "ttxt:last_dur");
		if (p)
			gf_isom_set_last_sample_duration(ctx->file, tkw->track_num, p->value.uint);


		if (tkw->is_nalu && ctx->pack_nal && (gf_isom_get_mode(ctx->file)!=GF_ISOM_OPEN_WRITE)) {
			u32 msize = 0;
			Bool do_rewrite = GF_FALSE;
			u32 i, count = gf_isom_get_sample_description_count(ctx->file, tkw->track_num);
			const GF_PropertyValue *p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_MAX_NALU_SIZE);
			msize = gf_get_bit_size(p->value.uint);
			if (msize<8) msize = 8;
			else if (msize<16) msize = 16;
			else msize = 32;

			if (msize<=0xFFFF) {
				for (i=0; i<count; i++) {
					u32 k = 8 * gf_isom_get_nalu_length_field(ctx->file, tkw->track_num, i+1);
					if (k > msize) {
						do_rewrite = GF_TRUE;
					}
				}
				if (do_rewrite) {
					GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[MP4Mux] Adjusting NALU SizeLength to %d bits\n", msize ));
					gf_media_nal_rewrite_samples(ctx->file, tkw->track_num, msize);
					msize /= 8;
					for (i=0; i<count; i++) {
						gf_isom_set_nalu_length_field(ctx->file, tkw->track_num, i+1, msize);
					}
				}
			}
		}
	}

	if (ctx->owns_mov) {
		GF_Err e;
		switch (ctx->mode) {
		case MP4MX_MODE_INTER:
			if (ctx->cdur==0) {
				e = gf_isom_set_storage_mode(ctx->file, GF_ISOM_STORE_STREAMABLE);
			} else {
				e = gf_isom_make_interleave(ctx->file, ctx->cdur);
			}
			break;
		case MP4MX_MODE_FLAT:
			e = gf_isom_set_storage_mode(ctx->file, GF_ISOM_STORE_FLAT);
			break;
		case MP4MX_MODE_TIGHT:
			e = gf_isom_set_storage_mode(ctx->file, GF_ISOM_STORE_TIGHT);
			break;
		}
		if (e) {
			GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[MP4Mux] Failed to set storage mode: %s\n", gf_error_to_string(e) ));
		} else {
			e = gf_isom_close(ctx->file);
			if (e) {
				GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[MP4Mux] Failed to set write file: %s\n", gf_error_to_string(e) ));
			}
		}
		ctx->file = NULL;
		gf_filter_pid_set_eos(ctx->opid);
	}
}

static void mp4_mux_finalize(GF_Filter *filter)
{
	GF_MP4MuxCtx *ctx = gf_filter_get_udta(filter);
	assert(!ctx->dst_pck);

	if (ctx->mode>=MP4MX_MODE_FRAG) {
		if (ctx->owns_mov) gf_isom_delete(ctx->file);
	}
	while (gf_list_count(ctx->tracks)) {
		TrackWriter *tkw = gf_list_pop_back(ctx->tracks);
		if (tkw->avcc) gf_odf_avc_cfg_del(tkw->avcc);
		if (tkw->svcc) gf_odf_avc_cfg_del(tkw->svcc);
		if (tkw->hvcc) gf_odf_hevc_cfg_del(tkw->hvcc);
		if (tkw->lvcc) gf_odf_hevc_cfg_del(tkw->lvcc);
		if (tkw->inband_hdr) gf_free(tkw->inband_hdr);
		gf_free(tkw);
	}
	gf_list_del(ctx->tracks);
	if (ctx->bs_r) gf_bs_del(ctx->bs_r);
	if (ctx->seg_name) gf_free(ctx->seg_name);
	if (ctx->tmp_store) gf_fclose(ctx->tmp_store);
}

static const GF_FilterCapability MP4MuxCaps[] =
{
	//for now don't accept files as input, although we could store them as items, to refine
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_SCENE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_OD),
	//we want framed media only
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT_STATIC_OPT, 	GF_PROP_PID_DASH_MODE, 0),
	//and any codecid
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_NONE),
	CAP_STRING(GF_CAPS_OUTPUT_STATIC,  GF_PROP_PID_FILE_EXT, "mp4|mpg4|m4a|m4i|3gp|3gpp|3g2|3gp2|iso|m4s|heif|heic|avci"),
	{0},
	//for scene and OD, we don't want raw codecid (filters modifying a scene graph we don't expose)
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_SCENE),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_OD),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_RAW),
};


#define OFFS(_n)	#_n, offsetof(GF_MP4MuxCtx, _n)
static const GF_FilterArgs MP4MuxArgs[] =
{
	{ OFFS(m4sys), "force MPEG-4 Systems signaling of tracks", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(dref), "only references data from source file - not compatible with all media sources", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(dur), "only imports the specified duration", GF_PROP_FRACTION, "0", NULL, GF_FALSE},
	{ OFFS(pack3gp), "packs a given number of 3GPP audio frames in one sample", GF_PROP_UINT, "1", NULL, GF_FALSE},
	{ OFFS(importer), "compatibility with old importer, displays import progress", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(noedit), "disable edit lists on video tracks", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(pack_nal), "repacks NALU size length to minimum possible size for AVC/HEVC/...", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(xps_inband), "Use inband param set for AVC/HEVC/.... If mix, creates non-standard files using single sample entry with first PSs found, and moves other PS inband", GF_PROP_UINT, "no", "no|all|mix", GF_FALSE},
	{ OFFS(block_size), "Target output block size, 0 for default internal value (40k)", GF_PROP_UINT, "0", NULL, GF_FALSE},
	{ OFFS(mode), "Write mode. inter uses cdur to interleave the file. flat writes a flat file, file at end. cap flushes to disk as soon as samples are added. tight uses per-sample interleaving of all tracks. frag framents the file using cdur duration. sfrag framents the file using cdur duration but adjusting to start with SAP1/3.  ", GF_PROP_UINT, "inter", "inter|flat|cap|tight|frag|sfrag", GF_FALSE},
	{ OFFS(cdur), "chunk duration for interleaving and fragmentation modes", GF_PROP_DOUBLE, "1.0", NULL, GF_FALSE},
	{ OFFS(for_test), "sets all dates and version info to 0 to enforce same binary result generation", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(timescale), "timescale to use for movie edit lists", GF_PROP_UINT, "600", NULL, GF_FALSE},
	{ OFFS(moof_first), "geenrates fragments starting with moof then mdat", GF_PROP_BOOL, "true", NULL, GF_FALSE},
	{ OFFS(abs_offset), "uses absolute file offset in fragments rather than offsets from moof", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(fsap), "splits truns in video fragments at SAPs to reduce file size", GF_PROP_BOOL, "true", NULL, GF_FALSE},
	{ OFFS(file), "pointer to a write/edit ISOBMF file used internally by importers and exporters", GF_PROP_POINTER, NULL, NULL, GF_FALSE},
	{ OFFS(subs_sidx), "number of subsegments per sidx. negative value disables sidx", GF_PROP_SINT, "-1", NULL, GF_FALSE},
	{ OFFS(m4cc), "4 character code of empty box to appen at the end of a segment", GF_PROP_STRING, NULL, NULL, GF_FALSE},
	{ OFFS(chain_sidx), "Uses daisy-chaining of SIDX", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(moof_sn), "Sets sequence number of first moof to N", GF_PROP_UINT, "1", NULL, GF_FALSE},
	{ OFFS(tfdt), "sets TFDT of first traf", GF_PROP_FRACTION64, "0", NULL, GF_FALSE},
	{ OFFS(no_def), "disables default flags in fragments", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(straf), "uses a single traf per moov (smooth streaming and co)", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(pssh_moof), "stores PSSH boxes in first moof of each segments. By default PSSH are stored in movie box", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(sgpd_traf), "stores sample group descriptions in traf (duplicated for each traf). If not used, sample group descriptions are stored in the movie box", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(cache), "Enables temp storage for VoD dash modes. When disabled, SIDX size will be estimated based on duration and DASH segment length, and padding will be used in the file before the final SIDX. When enabled, file data is stored to a cache and flushed upon completion", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(block_size), "block size used to flush files in onDemand mode when cache is used", GF_PROP_UINT, "50000", NULL, GF_FALSE},
	{ OFFS(noinit), "does not send initial moov, used for DASH bitstream switching mode", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(tktpl), "use track box from input if any as a template to create new track. no disables template, yes clones the track (except edits and decoder config), udta only loads udta", GF_PROP_UINT, "yes", "no|yes|udta", GF_FALSE},
	{ OFFS(mudta), "use udta and other moov extension boxes from input if any. no disables import, yes clones all extension boxes, udta only loads udta", GF_PROP_UINT, "yes", "no|yes|udta", GF_FALSE},
	{0}
};


GF_FilterRegister MP4MuxRegister = {
	.name = "mxisom",
	.description = "ISOBMFF muxer",
	.private_size = sizeof(GF_MP4MuxCtx),
	.args = MP4MuxArgs,
	.initialize = mp4_mux_initialize,
	.finalize = mp4_mux_finalize,
	SETCAPS(MP4MuxCaps),
	.configure_pid = mp4_mux_configure_pid,
	.process = mp4_mux_process,
};


const GF_FilterRegister *mp4_mux_register(GF_FilterSession *session)
{
	return &MP4MuxRegister;
}
