/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2019
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

enum{
	NALU_NONE,
	NALU_AVC,
	NALU_HEVC
};

typedef struct
{
	GF_FilterPid *ipid;
	u32 track_num, track_id;
	GF_ISOSample sample;

	u32 src_timescale;
	u32 tk_timescale;
	u32 stream_type;
	u32 codecid;
	Bool is_encrypted;

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

	u32 nal_unit_size;

	GF_AVCConfig *avcc, *svcc;
	GF_HEVCConfig *hvcc, *lvcc;

	u8 *inband_hdr;
	u32 inband_hdr_size;
	u32 is_nalu;
	Bool is_av1, is_vpx;
	Bool fragment_done;
	s32 ts_delay, negctts_shift;
	Bool insert_tfdt, probe_min_ctts;
	u64 first_dts_in_seg, next_seg_cts, cts_next;
	u64 offset_dts;
	u32 samples_in_frag;

	//0: not cenc, 1: needs setup of stsd entry, 2: setup done
	u32 cenc_state;
	u32 skip_byte_block, crypt_byte_block;
	u32 IV_size, constant_IV_size;
	bin128 constant_IV, KID;
	Bool cenc_subsamples;

	Bool fake_track;

	Bool has_brands;
	Bool force_inband_inject;

	u32 dur_in_frag;

	u32 amr_mode_set;
	Bool has_seig;
	u64 empty_init_dur;
	u32 raw_audio_bytes_per_sample;
	u64 dts_patch;

	Bool is_item;
	char status_type;
	u32 last_import_pc;

	u32 nb_frames;
	u64 down_bytes, down_size;
	GF_Fraction pid_dur;
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

enum
{
	MP4MX_PSSH_MOOV=0,
	MP4MX_PSSH_MOOF,
	MP4MX_PSSH_SKIP,
};

enum
{
	MP4MX_CT_EDIT=0,
	MP4MX_CT_NOEDIT,
	MP4MX_CT_NEGCTTS,
};

typedef struct
{
	//filter args
	GF_ISOFile *file;
	char *tmpd;
	Bool m4sys, dref;
	GF_Fraction idur;
	u32 pack3gp, ctmode;
	Bool importer, pack_nal, moof_first, abs_offset, fsap, tfdt_traf;
	u32 xps_inband;
	u32 block_size;
	u32 store, tktpl, mudta;
	s32 subs_sidx;
	Double cdur;
	s32 moovts;
	char *m4cc;
	Bool chain_sidx;
	u32 msn, msninc;
	GF_Fraction64 tfdt;
	Bool nofragdef, straf, strun, sgpd_traf, cache, noinit;
	u32 psshs;
	u32 tkid;
	Bool fdur;
	Bool btrt;
	Bool ssix;
	Bool ccst;
	s32 mediats;
	GF_AudioSampleEntryImportMode ase;
	char *styp;
	Bool sseg;
	Bool noroll;
	Bool saio32;

	//internal
	Bool owns_mov;
	GF_FilterPid *opid;
	Bool first_pck_sent;
	Bool mvex;

	GF_List *tracks;

	GF_BitStream *bs_r;
	//fragmentation state
	Bool init_movie_done, fragment_started, segment_started, insert_tfdt, insert_pssh, cdur_set;
	Double next_frag_start, adjusted_next_frag_start;

	u64 current_offset;
	u64 current_size;

	u32 nb_segs, nb_frags, nb_frags_in_seg;

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

	u32 *seg_sizes;
	u32 nb_seg_sizes, alloc_seg_sizes;
	Bool config_timing;

	u32 major_brand_set;
	Bool def_brand_patched;

	Bool force_play;

	Bool moov_inserted;
	Bool update_report;
	u64 total_bytes_in, total_bytes_out;
	u32 total_samples, last_mux_pc;

	u32 maxchunk;
	Bool make_qt;
	TrackWriter *prores_track;

	GF_SegmentIndexBox *cloned_sidx;
	u32 cloned_sidx_index;
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

	tkw->nal_unit_size = 0;
	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	if (tkw->avcc || tkw->svcc) {
		if (tkw->avcc) {
			mp4_mux_write_ps_list(bs, tkw->avcc->sequenceParameterSets, tkw->avcc->nal_unit_size);
			if (!tkw->nal_unit_size) tkw->nal_unit_size = tkw->avcc->nal_unit_size;
		}

		if (tkw->svcc) {
			mp4_mux_write_ps_list(bs, tkw->svcc->sequenceParameterSets, tkw->svcc->nal_unit_size);
			if (!tkw->nal_unit_size) tkw->nal_unit_size = tkw->svcc->nal_unit_size;
		}

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
		if (tkw->hvcc) {
			mp4_mux_write_ps_list(bs, mp4_mux_get_hevc_ps(tkw->hvcc, GF_HEVC_NALU_VID_PARAM), tkw->hvcc->nal_unit_size);
			if (!tkw->nal_unit_size) tkw->nal_unit_size = tkw->hvcc->nal_unit_size;
		}
		if (tkw->lvcc) {
			mp4_mux_write_ps_list(bs, mp4_mux_get_hevc_ps(tkw->lvcc, GF_HEVC_NALU_VID_PARAM), tkw->lvcc->nal_unit_size);
			if (!tkw->nal_unit_size) tkw->nal_unit_size = tkw->hvcc->nal_unit_size;
		}
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
	//we may have cases where the param sets are updated before a non-IDR/SAP3 picture, we must inject asap at least once
	tkw->force_inband_inject = GF_TRUE;
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
			//fallthrough
		case GF_ISOM_MEDIA_TEXT:
			gf_isom_get_track_layout_info(ctx->file, i+1, &w, &h, NULL, NULL, NULL);
			if (w > f_w) f_w = w;
			if (h > f_h) f_h = h;
			break;
		}
	}
	(*width) = f_w ? f_w : TEXT_DEFAULT_WIDTH;
	(*height) = f_h ? f_h : TEXT_DEFAULT_HEIGHT;
}

static void mp4_mux_track_writer_del(TrackWriter *tkw)
{
	if (tkw->avcc) gf_odf_avc_cfg_del(tkw->avcc);
	if (tkw->svcc) gf_odf_avc_cfg_del(tkw->svcc);
	if (tkw->hvcc) gf_odf_hevc_cfg_del(tkw->hvcc);
	if (tkw->lvcc) gf_odf_hevc_cfg_del(tkw->lvcc);
	if (tkw->inband_hdr) gf_free(tkw->inband_hdr);
	gf_free(tkw);
}

static void mp4_mux_write_track_refs(GF_MP4MuxCtx *ctx, TrackWriter *tkw, const char *rname, u32 rtype)
{
	u32 i;
	const GF_PropertyValue *p = gf_filter_pid_get_property_str(tkw->ipid, rname);
	if (!p) return;
	for (i=0; i<p->value.uint_list.nb_items; i++) {
		gf_isom_set_track_reference(ctx->file, tkw->track_num, rtype, p->value.uint_list.vals[i]);
	}
}


static GF_Err mp4_mux_setup_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_true_pid)
{
	Bool use_m4sys = GF_FALSE;
	Bool use_tx3g = GF_FALSE;
	Bool use_webvtt = GF_FALSE;
	Bool needs_track = GF_FALSE;
	u32 needs_sample_entry = 0; //1: change of codecID, 2 change of decoder config
	Bool use_gen_sample_entry = GF_FALSE;
	Bool use_3gpp_config = GF_FALSE;
	Bool use_ac3_entry = GF_FALSE;
	Bool use_flac_entry = GF_FALSE;
	Bool use_avc = GF_FALSE;
	Bool use_hevc = GF_FALSE;
	Bool use_hvt1 = GF_FALSE;
	Bool use_av1 = GF_FALSE;
	Bool use_vpX = GF_FALSE;
	Bool use_mj2 = GF_FALSE;
	Bool use_opus = GF_FALSE;
	Bool use_dref = GF_FALSE;
	Bool skip_dsi = GF_FALSE;
	Bool is_text_subs = GF_FALSE;
	u32 m_subtype=0;
	u32 override_stype=0;
	u32 width, height, sr, nb_chan, nb_bps, z_order, txt_fsize;
	GF_Fraction fps, sar;
	GF_List *multi_pid_stsd = NULL;
	u32 multi_pid_idx = 0;
	GF_FilterPid *orig_pid = NULL;
	u32 codec_id;
	u32 frames_per_sample_backup=0;
	u32 is_nalu_backup = NALU_NONE;
	Bool is_tile_base = GF_FALSE;
	Bool unknown_generic = GF_FALSE;
	u32 multi_pid_final_stsd_idx = 0;
	u32 audio_pli=0;
	Bool force_tk_layout = GF_FALSE;
	Bool force_mix_xps = GF_FALSE;
	Bool make_inband_headers = GF_FALSE;
	Bool is_prores = GF_FALSE;

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
	const char *meta_content_encoding = NULL;
	char *txt_font = NULL;

	u32 i, count, reuse_stsd = 0;
	GF_Err e;
	const GF_PropertyValue *dsi=NULL;
	const GF_PropertyValue *enh_dsi=NULL;
	const GF_PropertyValue *p;
	GF_MP4MuxCtx *ctx = gf_filter_get_udta(filter);
	GF_AudioSampleEntryImportMode ase_mode = ctx->ase;
	TrackWriter *tkw;

	if (ctx->owns_mov && !ctx->opid) {
		char *dst;
		ctx->opid = gf_filter_pid_new(filter);

		dst = gf_filter_get_dst_name(filter);
		if (dst) {
			char *ext = gf_file_ext_start(dst);
			if (ext && (!stricmp(ext, ".mov") || !stricmp(ext, ".qt")) ) {
				ctx->make_qt = GF_TRUE;
			}
			gf_free(dst);
		}
	}
	//copy properties at init or reconfig
	if (ctx->opid && is_true_pid) {
		gf_filter_pid_copy_properties(ctx->opid, pid);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, NULL);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT, NULL);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, NULL);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, NULL);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_FILE) );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING("mp4") );

		switch (ctx->store) {
		case MP4MX_MODE_FLAT:
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DISABLE_PROGRESSIVE, &PROP_BOOL(GF_TRUE) );
			break;
		case MP4MX_MODE_INTER:
		case MP4MX_MODE_TIGHT:
			gf_filter_pid_allow_direct_dispatch(ctx->opid);
			break;
		}
	}

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_TILE_BASE);
	if (p && p->value.boolean) is_tile_base = GF_TRUE;

	if (is_true_pid && !is_tile_base) {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_DASH_MULTI_TRACK);
		if (p) {
			u32 i, count, j, count2;
			GF_List *multi_tracks = p->value.ptr;
			count = gf_list_count(multi_tracks);
			for (i=0; i<count; i++) {
				GF_FilterPid *a_ipid = gf_list_get(multi_tracks, i);
				const GF_PropertyValue *a_pidid = gf_filter_pid_get_property(a_ipid, GF_PROP_PID_ID);
				count2 = gf_list_count(ctx->tracks);
				for (j=0; j<count2; j++) {
					TrackWriter *atkw = gf_list_get(ctx->tracks, j);
					const GF_PropertyValue *c_pidid = gf_filter_pid_get_property(atkw->ipid, GF_PROP_PID_ID);
					if (gf_props_equal(a_pidid, c_pidid)) {
						a_ipid = NULL;
						break;
					}
				}
				if (a_ipid)
					mp4_mux_setup_pid(filter, a_ipid, GF_FALSE);
			}
		}
	}

	audio_pli = gf_isom_get_pl_indication(ctx->file, GF_ISOM_PL_AUDIO);

	//new pid ?
	tkw = gf_filter_pid_get_udta(pid);
	if (!tkw) {
		GF_FilterEvent evt;
		GF_SAFEALLOC(tkw, TrackWriter);
		gf_list_add(ctx->tracks, tkw);
		tkw->ipid = pid;
		tkw->fake_track = !is_true_pid;
		if (is_true_pid) {
			gf_filter_pid_set_udta(pid, tkw);

#ifdef GPAC_ENABLE_COVERAGE
			if (gf_sys_is_test_mode()) {
				gf_filter_pid_get_min_pck_duration(pid);
			}
#endif
			if (!ctx->owns_mov || ctx->force_play) {
				GF_FEVT_INIT(evt, GF_FEVT_PLAY, pid);
				gf_filter_pid_send_event(pid, &evt);
			}
			gf_filter_pid_set_framing_mode(pid, GF_TRUE);

			p = gf_filter_pid_get_property(pid, GF_PROP_PID_ITEM_ID);
			if (p) {
				tkw->is_item = GF_TRUE;
			} else {
				ctx->config_timing = GF_TRUE;
				ctx->update_report = GF_TRUE;
			}

		}
	}

	//check change of pid config
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DEPENDENCY_ID);
	if (p) {
		if (p->value.uint!=tkw->dep_id) needs_track = GF_TRUE;
		tkw->dep_id = p->value.uint;
	}

	//check change of pid config
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (p) {
		if (p->value.uint!=tkw->codecid) needs_sample_entry = 1;
		tkw->codecid = p->value.uint;
	}

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
	if (p) {
		u32 stype = p->value.uint;
		if (tkw->is_encrypted && (p->value.uint==GF_STREAM_ENCRYPTED) ) {
			stype = gf_codecid_type(tkw->codecid);
		}
		if (stype != tkw->stream_type) {
			needs_track = GF_TRUE;
			tkw->stream_type = stype;
			const char *name = gf_stream_type_name(stype);
			tkw->status_type = name ? name[0] : 'U';

		}
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
	} else if (tkw->enh_cfg_crc) {
		tkw->enh_cfg_crc = 0;
		needs_sample_entry = 2;
	}

	//TODO: try to merge PPS/SPS for AVC and HEVC rather than creating a new sample description

	switch (tkw->codecid) {
	case GF_CODECID_AAC_MPEG4:
	case GF_CODECID_AAC_MPEG2_MP:
	case GF_CODECID_AAC_MPEG2_LCP:
	case GF_CODECID_AAC_MPEG2_SSRP:
	case GF_CODECID_MPEG4_PART2:
	case GF_CODECID_AVC:
	case GF_CODECID_SVC:
	case GF_CODECID_HEVC:
	case GF_CODECID_LHVC:
		if (!dsi && !enh_dsi) return GF_OK;
		break;
	case GF_CODECID_APCH:
	case GF_CODECID_APCO:
	case GF_CODECID_APCN:
	case GF_CODECID_APCS:
	case GF_CODECID_APCF:
	case GF_CODECID_AP4X:
	case GF_CODECID_AP4H:
		if (!ctx->make_qt) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MP4Mux] ProRes track detected, muxing to QTFF even though ISOBMFF was asked\n"));
			ctx->make_qt = GF_TRUE;
		}
		if (ctx->prores_track == tkw) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] More than one ProRes track detected, result might be non compliant\n"));
		}
		is_prores = GF_TRUE;
		break;
	default:
		break;
	}
	if (!tkw->track_num) {
		needs_sample_entry = 1;
		needs_track = GF_TRUE;
	}

	if (ctx->make_qt) {
		gf_isom_remove_root_od(ctx->file);
		gf_isom_set_brand_info(ctx->file, GF_ISOM_BRAND_QT, 512);
		gf_isom_reset_alt_brands(ctx->file);
		tkw->has_brands = GF_TRUE;
		ctx->major_brand_set = GF_ISOM_BRAND_QT;
		ctx->btrt = GF_FALSE;

		if (is_prores && !ctx->prores_track) {
			ctx->prores_track = tkw;
		}
	}

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_URL);
	if (p) src_url = p->value.string;


	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DASH_MODE);
	if (p) {
		ctx->dash_mode = MP4MX_DASH_ON;
		if (p->value.uint==2) {
			e = mp4mx_setup_dash_vod(ctx, tkw);
			if (e) return e;
		}
	}
	//we consider that when muxing single segments, we are always in DASH, not VoD mode
	else if (ctx->noinit) {
		ctx->dash_mode = MP4MX_DASH_ON;
	}

	if (!ctx->cdur_set) {
		ctx->cdur_set = GF_TRUE;
		if (ctx->cdur<0) {
			if (ctx->make_qt)
				ctx->cdur = 0.5;
			else {
				ctx->cdur = 1.0;
				if (ctx->dash_mode)
					ctx->fdur = GF_FALSE;
			}
		} else if (ctx->dash_mode)
			ctx->fdur = GF_TRUE;
	}

	if (needs_track) {
		if (ctx->init_movie_done) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MP4Mux] Cannot add track to already finalized movie in fragmented file, will request a new muxer for that track\n"));
			return GF_REQUIRES_NEW_INSTANCE;
		}
		if (tkw->is_item) {
			needs_track = GF_FALSE;
		}
	}

	if (needs_track) {
		u32 tkid=0;
		u32 mtype=0;
		u32 target_timescale = 0;

		if (ctx->make_qt && (tkw->stream_type==GF_STREAM_VISUAL)) {
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_FPS);
			if (p) {
				u32 ts=p->value.frac.num, inc=p->value.frac.den;
				if (inc * 24000 == ts * 1001) target_timescale = 24000;
				else if (inc * 2400 == ts * 100) target_timescale = 2400;
				else if (inc * 2500 == ts * 100) target_timescale = 2500;
				else if (inc * 30000 == ts * 1001) target_timescale = 30000;
				else if (inc * 3000 == ts * 100) target_timescale = 3000;
				else if (inc * 5000 == ts * 100) target_timescale = 5000;
				else if (inc * 60000 == ts * 1001) target_timescale = 60000;
				else if (is_prores) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[ProRes] Unrecognized frame rate %g\n", ((Double)ts)/inc ));
					return GF_NON_COMPLIANT_BITSTREAM;
				}
			}
			if (!ctx->prores_track)
				ctx->prores_track = tkw;
		}

		if (!ctx->moov_inserted) {
			if (target_timescale) {
				ctx->moovts = target_timescale;
				gf_isom_set_timescale(ctx->file, target_timescale);
			} else if (ctx->moovts>=0) {
				gf_isom_set_timescale(ctx->file, ctx->moovts);
			}
		}

		//assign some defaults
		tkw->src_timescale = 0;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
		if (p) tkw->src_timescale = p->value.uint;

		u32 mtimescale = 1000;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_SAMPLE_RATE);
		if (p) mtimescale = p->value.uint;
		else {
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_FPS);
			if (p && p->value.frac.den) mtimescale = p->value.frac.den;
		}
		if (!tkw->src_timescale) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MP4Mux] No timescale specified, guessing from media: %d\n", mtimescale));
			tkw->src_timescale = mtimescale;
		}
		if (target_timescale) tkw->tk_timescale = target_timescale;
		else if (ctx->mediats>0) tkw->tk_timescale = ctx->mediats;
		else if (ctx->mediats<0) tkw->tk_timescale = mtimescale;
		else tkw->tk_timescale = tkw->src_timescale;

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_ESID);
		if (!p) p = gf_filter_pid_get_property(pid, GF_PROP_PID_ID);
		if (p) tkid = p->value.uint;

		if (ctx->tkid) tkid = ctx->tkid;

		if (tkw->stream_type == GF_STREAM_ENCRYPTED) {
			tkw->is_encrypted = GF_TRUE;
			tkw->stream_type = gf_codecid_type(tkw->codecid);
		}
		mtype = gf_isom_stream_type_to_media_type(tkw->stream_type, tkw->codecid);

		if (ctx->moovts<0) {
			ctx->moovts = tkw->tk_timescale;
			gf_isom_set_timescale(ctx->file, (u32) ctx->moovts);
		}

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_ISOM_TRACK_TEMPLATE);
		if (ctx->tktpl && p && p->value.data.ptr) {
			Bool udta_only = (ctx->tktpl==2) ? GF_TRUE : GF_FALSE;


			tkw->track_num = gf_isom_new_track_from_template(ctx->file, tkid, mtype, tkw->tk_timescale, p->value.data.ptr, p->value.data.size, udta_only);
			if (!tkw->track_num) {
				tkw->track_num = gf_isom_new_track_from_template(ctx->file, 0, mtype, tkw->tk_timescale, p->value.data.ptr, p->value.data.size, udta_only);
			}

			if (!ctx->btrt) {
				gf_isom_update_bitrate(ctx->file, tkw->track_num, 0, 0, 0, 0);
			}
		} else {
			if (!mtype) {
				mtype = GF_4CC('u','n','k','n');
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MP4Mux] Unable to find ISOM media type for stream type %s codec %s\n", gf_stream_type_name(tkw->stream_type), gf_codecid_name(tkw->codecid) ));
			}
			tkw->track_num = gf_isom_new_track(ctx->file, tkid, mtype, tkw->tk_timescale);
			if (!tkw->track_num) {
				tkw->track_num = gf_isom_new_track(ctx->file, 0, mtype, tkw->tk_timescale);
			}
			//FIXME once we finally merge to filters, there is an old bug in isobmff initializing the width and height to 320x240 which breaks text import
			//this should be removed and hashes regenerated
			gf_isom_set_track_layout_info(ctx->file, tkw->track_num, 0, 0, 0, 0, 0);

			if (!gf_sys_is_test_mode()) {
				p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_URL);
				if (tkw->track_num && p && p->value.string) {
					char szHName[1025];
					char *f = gf_file_basename(p->value.string);
					szHName[1024]=0;
					snprintf(szHName, 1024, "*%s@GPAC%s", f ? f : "", gf_gpac_version() );
					gf_isom_set_handler_name(ctx->file, tkw->track_num, szHName);
				}
			}
		}

		if (!tkw->track_num) {
			e = gf_isom_last_error(ctx->file);
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Failed to create new track: %s\n", gf_error_to_string(e) ));
			return e;
		}
		tkw->track_id = gf_isom_get_track_id(ctx->file, tkw->track_num);
		tkw->next_is_first_sample = GF_TRUE;

		p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_ISOM_TRACK_FLAGS);
		if (p) {
			gf_isom_set_track_flags(ctx->file, tkw->track_num, p->value.uint, GF_ISOM_TKFLAGS_SET);
		} else {
			gf_isom_set_track_enabled(ctx->file, tkw->track_num, 1);
		}

		//if we have a subtype set for the pid, use it
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_SUBTYPE);
		if (p) gf_isom_set_media_type(ctx->file, tkw->track_num, p->value.uint);

		p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_ISOM_HANDLER);
		if (p && p->value.string) {
			gf_isom_set_handler_name(ctx->file, tkw->track_num, p->value.string);
		}

		p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_ISOM_TRACK_MATRIX);
		if (p && (p->value.uint_list.nb_items==9)) {
			gf_isom_set_track_matrix(ctx->file, tkw->track_num, (s32 *) p->value.uint_list.vals);
		}

		//by default use cttsv1 (negative ctts)
		gf_isom_set_composition_offset_mode(ctx->file, tkw->track_num, GF_TRUE);

		p = ctx->make_qt ? NULL : gf_filter_pid_get_property(pid, GF_PROP_PID_PROFILE_LEVEL);
		if (p) {
			tkw->media_profile_level = p->value.uint;
			if (tkw->stream_type == GF_STREAM_AUDIO) {
				//patch to align old arch (IOD not written in dash) with new
				if (!ctx->dash_mode) {
					gf_isom_set_pl_indication(ctx->file, GF_ISOM_PL_AUDIO, p->value.uint);
				}
			} else if (tkw->stream_type == GF_STREAM_VISUAL) {
				//patch to align old arch (IOD not written in dash) with new
				if (!ctx->dash_mode) {
					gf_isom_set_pl_indication(ctx->file, GF_ISOM_PL_VISUAL, p->value.uint);
				}
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

	if (!tkw->has_brands) {
		Bool is_isom = GF_FALSE;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_ISOM_MBRAND);
		if (p) {
			if (!ctx->major_brand_set) {
				gf_isom_set_brand_info(ctx->file, p->value.uint, 1);
				ctx->major_brand_set = p->value.uint;
			} else {
				gf_isom_modify_alternate_brand(ctx->file, p->value.uint, GF_TRUE);
			}
			if (p->value.uint == GF_ISOM_BRAND_ISOM) is_isom = GF_TRUE;
		}
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_ISOM_BRANDS);
		if (p && p->value.uint_list.nb_items) {
			tkw->has_brands = GF_TRUE;
			if (!ctx->major_brand_set) {
				ctx->major_brand_set = p->value.uint_list.vals[0];
				gf_isom_set_brand_info(ctx->file, p->value.uint_list.vals[0], 1);
			}

			for (i=0; i<p->value.uint_list.nb_items; i++) {
				gf_isom_modify_alternate_brand(ctx->file, p->value.uint_list.vals[i], GF_TRUE);
				if (p->value.uint_list.vals[i] == GF_ISOM_BRAND_ISOM) is_isom = GF_TRUE;
			}
		}
		if (!ctx->m4sys && !is_isom && !ctx->def_brand_patched) {
			//remove default brand
			gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_ISOM, 0);
			ctx->def_brand_patched = GF_TRUE;
		}
	}

	width = height = sr = nb_chan = z_order = txt_fsize = 0;
	nb_bps = 16;
	fps.num = 25;
	fps.den = 1;
	sar.num = sar.den = 1;
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
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_AUDIO_BPS);
	if (p) nb_bps = p->value.uint;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_LANGUAGE);
	if (p) lang_name = p->value.string;

	if (is_true_pid) {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_NB_FRAMES);
		if (p) tkw->nb_frames = p->value.uint;
		else tkw->nb_frames = 0;
	}

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
		m_subtype = GF_ISOM_BOX_TYPE_MJP2;
		comp_name = "JPEG2000";
		use_mj2 = GF_TRUE;
		break;

	case GF_CODECID_AMR:
		m_subtype = GF_ISOM_SUBTYPE_3GP_AMR;
		comp_name = "AMR";
		use_3gpp_config = GF_TRUE;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_AMR_MODE_SET);
		if (p && (p->value.uint!=tkw->amr_mode_set)) {
			tkw->amr_mode_set = p->value.uint;
			needs_sample_entry = 2;
		}
		break;
	case GF_CODECID_AMR_WB:
		m_subtype = GF_ISOM_SUBTYPE_3GP_AMR_WB;
		comp_name = "AMR-WB";
		use_3gpp_config = GF_TRUE;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_AMR_MODE_SET);
		if (p && (p->value.uint!=tkw->amr_mode_set)) {
			tkw->amr_mode_set = p->value.uint;
			needs_sample_entry = 2;
		}
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
	case GF_CODECID_FLAC:
		m_subtype = GF_ISOM_SUBTYPE_FLAC;
		comp_name = "FLAC";
		use_flac_entry = GF_TRUE;
		break;
	case GF_CODECID_OPUS:
		m_subtype = GF_ISOM_SUBTYPE_OPUS;
		comp_name = "Opus";
		use_opus = GF_TRUE;
		break;
	case GF_CODECID_MPEG4_PART2:
		m_subtype = GF_ISOM_SUBTYPE_MPEG4;
		use_m4sys = GF_TRUE;
		comp_name = "MPEG-4 Visual Part 2";
		use_gen_sample_entry = GF_FALSE;
		break;
	case GF_CODECID_AVC:
	case GF_CODECID_SVC:
		m_subtype = ((ctx->xps_inband==1) || (ctx->xps_inband==2)) ? GF_ISOM_SUBTYPE_AVC3_H264 : GF_ISOM_SUBTYPE_AVC_H264;
		use_avc = GF_TRUE;
		comp_name = (codec_id == GF_CODECID_SVC) ? "MPEG-4 SVC" : "MPEG-4 AVC";
		use_gen_sample_entry = GF_FALSE;
		if (ctx->xps_inband==1) skip_dsi = GF_TRUE;
		break;
	case GF_CODECID_HEVC:
	case GF_CODECID_LHVC:
		m_subtype = ((ctx->xps_inband==1) || (ctx->xps_inband==2)) ? GF_ISOM_SUBTYPE_HEV1  : GF_ISOM_SUBTYPE_HVC1;
		use_hevc = GF_TRUE;
		comp_name = (codec_id == GF_CODECID_LHVC) ? "L-HEVC" : "HEVC";
		use_gen_sample_entry = GF_FALSE;
		if (ctx->xps_inband==1) skip_dsi = GF_TRUE;
		if (codec_id==GF_CODECID_HEVC_TILES) {
			m_subtype = GF_ISOM_SUBTYPE_HVT1;
			skip_dsi = GF_TRUE;
		}
		break;
	case GF_CODECID_HEVC_TILES:
		m_subtype = GF_ISOM_SUBTYPE_HVT1;
		skip_dsi = GF_TRUE;
		use_hvt1 = GF_TRUE;
		comp_name = "HEVC Tiles";
		use_gen_sample_entry = GF_FALSE;
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

	case GF_CODECID_AV1:
		use_gen_sample_entry = GF_FALSE;
		m_subtype = GF_ISOM_SUBTYPE_AV01;
		use_av1 = GF_TRUE;
		comp_name = "AOM AV1 Video";
		break;

	case GF_CODECID_VP8:
		use_gen_sample_entry = GF_FALSE;
		m_subtype = GF_ISOM_SUBTYPE_VP08;
		use_vpX = GF_TRUE;
		comp_name = "VP8 Video";
		break;
	case GF_CODECID_VP9:
		use_gen_sample_entry = GF_FALSE;
		m_subtype = GF_ISOM_SUBTYPE_VP09;
		use_vpX = GF_TRUE;
		comp_name = "VP9 Video";
		break;
	case GF_CODECID_VP10:
		use_gen_sample_entry = GF_FALSE;
		m_subtype = GF_ISOM_SUBTYPE_VP10;
		use_vpX = GF_TRUE;
		comp_name = "VP10 Video";
		break;

	case GF_CODECID_VORBIS:
	case GF_CODECID_THEORA:
		use_m4sys = GF_TRUE;
		break;

	case GF_CODECID_BIFS:
/* ==  GF_CODECID_OD_V1:*/
	case GF_CODECID_BIFS_V2:
/*	== GF_CODECID_OD_V2:*/
	case GF_CODECID_BIFS_EXTENDED:
	case GF_CODECID_LASER:
		use_m4sys = GF_TRUE;
		break;

	case GF_CODECID_RAW:
		m_subtype = codec_id;
		unknown_generic = GF_TRUE;
		use_gen_sample_entry = GF_TRUE;
		use_m4sys = GF_FALSE;
		if (tkw->stream_type == GF_STREAM_AUDIO) {
			u32 req_non_planar_type = 0;
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_AUDIO_FORMAT);
			if (!p) break;
			comp_name = "RawAudio";
			//todo: we currently only set QTFF-style raw media, add ISOBMFF raw audio support
			switch (p->value.uint) {
			case GF_AUDIO_FMT_U8P:
			 	req_non_planar_type = GF_AUDIO_FMT_U8;
			case GF_AUDIO_FMT_U8:
				m_subtype = GF_QT_SUBTYPE_RAW;
				break;
			case GF_AUDIO_FMT_S16P:
			 	req_non_planar_type = GF_AUDIO_FMT_S16;
			case GF_AUDIO_FMT_S16:
				m_subtype = GF_QT_SUBTYPE_SOWT;
				break;
			case GF_AUDIO_FMT_S24P:
			 	req_non_planar_type = GF_AUDIO_FMT_S24;
			case GF_AUDIO_FMT_S24:
				m_subtype = GF_QT_SUBTYPE_IN24;
				break;
			case GF_AUDIO_FMT_S32P:
			 	req_non_planar_type = GF_AUDIO_FMT_S32P;
			case GF_AUDIO_FMT_S32:
				m_subtype = GF_QT_SUBTYPE_IN32;
				break;
			case GF_AUDIO_FMT_FLTP:
			 	req_non_planar_type = GF_AUDIO_FMT_FLTP;
			case GF_AUDIO_FMT_FLT:
				m_subtype = GF_QT_SUBTYPE_FL32;
				break;
			case GF_AUDIO_FMT_DBLP:
			 	req_non_planar_type = GF_AUDIO_FMT_DBL;
			case GF_AUDIO_FMT_DBL:
				m_subtype = GF_QT_SUBTYPE_FL64;
				break;
			default:
				return GF_NOT_SUPPORTED;
			}
			if (req_non_planar_type) {
				if (is_true_pid)
					gf_filter_pid_negociate_property(pid, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT(GF_AUDIO_FMT_S16));
				else {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] raw audio format planar in DASH multi-stsd mode is not supported, try assigning a resampler before the dasher\n"));
					return GF_NOT_SUPPORTED;
				}
			}
			unknown_generic = GF_FALSE;
			tkw->raw_audio_bytes_per_sample = gf_audio_fmt_bit_depth(p->value.uint);
			tkw->raw_audio_bytes_per_sample *= nb_chan;
			tkw->raw_audio_bytes_per_sample /= 8;
		}
		break;

	default:
		m_subtype = codec_id;
		unknown_generic = GF_TRUE;
		use_gen_sample_entry = GF_TRUE;
		use_m4sys = GF_FALSE;

		p = gf_filter_pid_get_property_str(pid, "meta:mime");
		if (p) meta_mime = p->value.string;
		p = gf_filter_pid_get_property_str(pid, "meta:encoding");
		if (p) meta_encoding = p->value.string;
		p = gf_filter_pid_get_property_str(pid, "meta:content_encoding");
		if (p) meta_content_encoding = p->value.string;
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

	if (dsi)
		meta_config = dsi->value.data.ptr;

	if (is_text_subs && !width && !height) {
		mp4_mux_get_video_size(ctx, &width, &height);
	}

	if (!ctx->init_movie_done && !tkw->nb_samples && (ctx->mediats<0) && (tkw->tk_timescale==1000)) {
		if (sr) {
			tkw->tk_timescale = sr;
			gf_isom_set_media_timescale(ctx->file, tkw->track_num, sr, GF_TRUE);
		}
		else if (width && fps.den) {
			tkw->tk_timescale = fps.den;
			gf_isom_set_media_timescale(ctx->file, tkw->track_num, fps.den, GF_TRUE);
		}
	}
	if (!needs_sample_entry || tkw->is_item) {
		goto sample_entry_done;
	}

	//we are fragmented, init movie done, we cannot update the sample description
	if (ctx->init_movie_done) {
		if (needs_sample_entry==1) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Cannot create a new sample description entry (codec change) for finalized movie in fragmented mode\n"));
			return GF_NOT_SUPPORTED;
		}
		force_mix_xps = GF_TRUE;
	} else if (ctx->store < MP4MX_MODE_FRAG) {
		if ((needs_sample_entry==2) && (ctx->xps_inband==2)) {
			force_mix_xps = GF_TRUE;
		}
		else if ((needs_sample_entry==2) && ((ctx->xps_inband==1)||(ctx->xps_inband==3)) ) {
			needs_sample_entry = 0;
			make_inband_headers = GF_TRUE;
		}
	}
	
	if (force_mix_xps) {

		//for AVC and HEVC, move to inband params if config changed
		if (use_avc && dsi) {
			if (tkw->avcc) gf_odf_avc_cfg_del(tkw->avcc);

			tkw->avcc = gf_odf_avc_cfg_read(dsi->value.data.ptr, dsi->value.data.size);

			if (enh_dsi) {
				if (tkw->svcc) gf_odf_avc_cfg_del(tkw->svcc);
				tkw->svcc = gf_odf_avc_cfg_read(enh_dsi->value.data.ptr, enh_dsi->value.data.size);
			}
			if (!ctx->xps_inband) {
				if (ctx->init_movie_done) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MP4Mux] AVC config update after movie has been finalized, moving all SPS/PPS inband (file might not be compliant\n"));
				}
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
				if (ctx->init_movie_done) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MP4Mux] HEVC config update after movie has been finalized, moving all SPS/PPS inband (file might not be compliant\n"));
				}
				ctx->xps_inband = 2;
			}
			mp4_mux_make_inband_header(ctx, tkw);
			return GF_OK;
		}

		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Cannot create a new sample description entry (config changed) for finalized movie in fragmented mode\n"));
		return GF_NOT_SUPPORTED;
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
		if (!esd->decoderConfig->objectTypeIndication) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Codec %s does not have an official MPEG-4 systems mapping, cannot mux\n", gf_codecid_name(codec_id) ));
			return GF_NOT_SUPPORTED;

		}
		esd->slConfig->timestampResolution = tkw->tk_timescale;
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

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_IN_IOD);
		if (p && p->value.boolean)
			gf_isom_add_track_to_root_od(ctx->file, tkw->track_num);


		if (dsi && (tkw->stream_type==GF_STREAM_AUDIO)) {
			GF_M4ADecSpecInfo acfg;
			gf_m4a_get_config(dsi->value.data.ptr, dsi->value.data.size, &acfg);
			audio_pli = acfg.audioPL;
		}
		//patch to align old arch (IOD not written in dash) with new
		if (audio_pli && !ctx->dash_mode)
			gf_isom_set_pl_indication(ctx->file, GF_ISOM_PL_AUDIO, audio_pli);

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

		if (needs_sample_entry) {

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
						gf_isom_avc_set_inband_config(ctx->file, tkw->track_num, tkw->stsd_idx, (ctx->xps_inband==2) ? GF_TRUE : GF_FALSE);
					}
				}
			}
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Error creating new AVC sample description: %s\n", gf_error_to_string(e) ));
				return e;
			}
		}
		
		if (dsi && ctx->xps_inband) {
			//this will cleanup all PS in avcC / svcC
			gf_isom_avc_set_inband_config(ctx->file, tkw->track_num, tkw->stsd_idx, (ctx->xps_inband==2) ? GF_TRUE : GF_FALSE);
			if (ctx->xps_inband==2) make_inband_headers = GF_TRUE;
		} else {
			gf_odf_avc_cfg_del(tkw->avcc);
			tkw->avcc = NULL;
		}
		//patch to align old arch with filters
		if (!ctx->dash_mode && !ctx->make_qt && !gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_ISOM_TREX_TEMPLATE) )
			gf_isom_set_pl_indication(ctx->file, GF_ISOM_PL_VISUAL, 0x7F);

		if (!tkw->has_brands)
			gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_AVC1, 1);

		tkw->is_nalu = NALU_AVC;

		tkw->use_dref = GF_FALSE;

	} else if (use_hvt1) {
		if (tkw->hvcc) gf_odf_hevc_cfg_del(tkw->hvcc);
		tkw->hvcc = gf_odf_hevc_cfg_new();
		e = gf_isom_hevc_config_new(ctx->file, tkw->track_num, tkw->hvcc, NULL, NULL, &tkw->stsd_idx);
		if (!e) {
			gf_isom_hevc_set_tile_config(ctx->file, tkw->track_num, tkw->stsd_idx, NULL, GF_FALSE);
		}
		gf_odf_hevc_cfg_del(tkw->hvcc);
		tkw->hvcc = NULL;
		tkw->is_nalu = NALU_HEVC;
		tkw->use_dref = GF_FALSE;
		if (!tkw->has_brands)
			gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_HVTI, 1);
	} else if (use_hevc) {
		if (tkw->hvcc) gf_odf_hevc_cfg_del(tkw->hvcc);

		if (!dsi && !enh_dsi) {
			//not yet known
			return GF_OK;
		}
		if (dsi) {
			tkw->hvcc = gf_odf_hevc_cfg_read(dsi->value.data.ptr, dsi->value.data.size,  (codec_id == GF_CODECID_LHVC) ? GF_TRUE : GF_FALSE);
		} else {
			tkw->hvcc = gf_odf_hevc_cfg_new();
		}
		tkw->is_nalu = NALU_HEVC;

		if (needs_sample_entry) {
			e = gf_isom_hevc_config_new(ctx->file, tkw->track_num, tkw->hvcc, NULL, NULL, &tkw->stsd_idx);

			if (!tkw->has_brands) {
				gf_isom_set_brand_info(ctx->file, GF_ISOM_BRAND_ISO4, 1);
				gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_ISOM, 0);
			}
			//pacth for old arch
			else if (ctx->dash_mode) {
				Bool force_brand=GF_FALSE;
				if (((ctx->major_brand_set>>24)=='i') && (((ctx->major_brand_set>>16)&0xFF)=='s') && (((ctx->major_brand_set>>8)&0xFF)=='o')) {
					if ( (ctx->major_brand_set&0xFF) <'6') force_brand=GF_TRUE;
				}

				if (!force_brand && ctx->major_brand_set) {
					gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_ISO6, 1);
				} else {
					gf_isom_set_brand_info(ctx->file, GF_ISOM_BRAND_ISO6, 1);
					gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_ISOM, 0);
				}
			}

			if (!e && enh_dsi) {
				if (tkw->lvcc) gf_odf_hevc_cfg_del(tkw->lvcc);
				tkw->lvcc = gf_odf_hevc_cfg_read(enh_dsi->value.data.ptr, enh_dsi->value.data.size, GF_TRUE);
				if (tkw->lvcc) {
					e = gf_isom_lhvc_config_update(ctx->file, tkw->track_num, tkw->stsd_idx, tkw->lvcc, dsi ? GF_ISOM_LEHVC_WITH_BASE_BACKWARD : GF_ISOM_LEHVC_ONLY);
					if (e) {
						gf_odf_hevc_cfg_del(tkw->lvcc);
						tkw->lvcc = NULL;
					}

					if (!dsi && ctx->xps_inband) {
						gf_isom_hevc_set_inband_config(ctx->file, tkw->track_num, tkw->stsd_idx, (ctx->xps_inband==2) ? GF_TRUE : GF_FALSE);
					}
				}
			} else if (codec_id == GF_CODECID_LHVC) {
				gf_isom_lhvc_config_update(ctx->file, tkw->track_num, tkw->stsd_idx, tkw->hvcc, GF_ISOM_LEHVC_ONLY);
			} else if (is_tile_base) {
				gf_isom_lhvc_config_update(ctx->file, tkw->track_num, tkw->stsd_idx, tkw->hvcc, GF_ISOM_HEVC_TILE_BASE);
			}
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Error creating new HEVC sample description: %s\n", gf_error_to_string(e) ));
				return e;
			}
		}

		if (dsi && ctx->xps_inband) {
			//this will cleanup all PS in avcC / svcC
			gf_isom_hevc_set_inband_config(ctx->file, tkw->track_num, tkw->stsd_idx, (ctx->xps_inband==2) ? GF_TRUE : GF_FALSE);
		} else {
			gf_odf_hevc_cfg_del(tkw->hvcc);
			tkw->hvcc = NULL;
		}

		tkw->use_dref = GF_FALSE;

	} else if (use_av1) {
		GF_AV1Config *av1c;

		if (!dsi) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] No decoder specific info found for AV1\n"));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		av1c = gf_odf_av1_cfg_read(dsi->value.data.ptr, dsi->value.data.size);
		if (!av1c) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Failed to parser AV1 decoder specific info\n"));
			return GF_NON_COMPLIANT_BITSTREAM;
		}

		e = gf_isom_av1_config_new(ctx->file, tkw->track_num, av1c, (char *) src_url, NULL, &tkw->stsd_idx);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Error creating new AV1 sample description: %s\n", gf_error_to_string(e) ));
			return e;
		}
		tkw->is_av1 = GF_TRUE;

		if (!tkw->has_brands) {
			gf_isom_set_brand_info(ctx->file, GF_ISOM_BRAND_ISO4, 1);
			gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_ISOM, 0);
			gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_AV01, 1);
		}

		gf_odf_av1_cfg_del(av1c);
	} else if (use_vpX) {
		GF_VPConfig *vpc;

		if (!dsi) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] No decoder specific info found for %s\n", gf_4cc_to_str(codec_id) ));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		vpc = gf_odf_vp_cfg_read(dsi->value.data.ptr, dsi->value.data.size);
		if (!vpc) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Failed to parser %s decoder specific info\n", gf_4cc_to_str(codec_id)));
			return GF_NON_COMPLIANT_BITSTREAM;
		}

		e = gf_isom_vp_config_new(ctx->file, tkw->track_num, vpc, (char *) src_url, NULL, &tkw->stsd_idx, m_subtype);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Error creating new %s sample description: %s\n", gf_4cc_to_str(codec_id), gf_error_to_string(e) ));
			return e;
		}
		tkw->is_vpx = GF_TRUE;
		gf_odf_vp_cfg_del(vpc);
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
		gpp_cfg.AMR_mode_set = tkw->amr_mode_set;
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

		if (!tkw->has_brands) {
			if (gpp_cfg.type==GF_ISOM_SUBTYPE_3GP_QCELP) {
				gf_isom_set_brand_info(ctx->file, GF_ISOM_BRAND_3G2A, 65536);
			} else if (gpp_cfg.type==GF_ISOM_SUBTYPE_3GP_H263) {
				gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_3GG6, 1);
				gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_3GG5, 1);
			}
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
	} else if (use_flac_entry) {
		e = gf_isom_flac_config_new(ctx->file, tkw->track_num, dsi ? dsi->value.data.ptr : NULL, dsi ? dsi->value.data.size : 0, (char *)src_url, NULL, &tkw->stsd_idx);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Error creating new FLAC audio sample description for stream type %d codecid %d: %s\n", tkw->stream_type, codec_id, gf_error_to_string(e) ));
			return e;
		}
		tkw->use_dref = src_url ? GF_TRUE : GF_FALSE;
	} else if (use_opus) {
		GF_OpusSpecificBox *opus_cfg = NULL;
		GF_BitStream *bs;

		if (!dsi) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] No decoder specific info found for opus\n" ));
			return GF_NON_COMPLIANT_BITSTREAM;
		}

		bs = gf_bs_new(dsi->value.data.ptr, dsi->value.data.size, GF_BITSTREAM_READ);
		e = gf_isom_box_parse((GF_Box**)&opus_cfg, bs);
		gf_bs_del(bs);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Error parsing opus configuration data: %s\n", gf_error_to_string(e) ));
			return e;
		}

		e = gf_isom_opus_config_new(ctx->file, tkw->track_num, opus_cfg, (char *)src_url, NULL, &tkw->stsd_idx);
		if (opus_cfg) gf_isom_box_del((GF_Box*)opus_cfg);
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
		e = gf_isom_new_stxt_description(ctx->file, tkw->track_num, m_subtype, meta_mime, meta_content_encoding, meta_config, &tkw->stsd_idx);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Error creating new %s sample description: %s\n", gf_4cc_to_str(m_subtype), gf_error_to_string(e) ));
			return e;
		}
		if (m_subtype == GF_ISOM_SUBTYPE_STXT) force_tk_layout = GF_TRUE;
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
	} else if (use_mj2) {
		e = gf_isom_new_mj2k_description(ctx->file, tkw->track_num, NULL, NULL, &tkw->stsd_idx, dsi ? dsi->value.data.ptr : NULL, dsi ? dsi->value.data.size : 0);
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
		len = (u32) strlen(comp_name);
		if (len>32) len = 32;
		udesc.compressor_name[0] = len;
		memcpy(udesc.compressor_name+1, comp_name, len);
		udesc.samplerate = sr;
		udesc.nb_channels = nb_chan;
		if (codec_id==GF_CODECID_RAW) {
			udesc.is_qtff = GF_TRUE;
			udesc.version = 1;
			ase_mode = GF_IMPORT_AUDIO_SAMPLE_ENTRY_v1_QTFF;
		}

		udesc.width = width;
		udesc.height = height;
		if (width) {
			udesc.v_res = 72;
			udesc.h_res = 72;
			udesc.depth = 24;
		}
		if (unknown_generic) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MP4Mux] muxing unknown codecID %s, using generic sample entry with 4CC \"%s\"\n", gf_codecid_name(codec_id), gf_4cc_to_str(m_subtype) ));
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

	if (ctx->btrt) {
		u32 avg_rate, max_rate, dbsize;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_BITRATE);
		avg_rate = p ? p->value.uint : 0;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_MAXRATE);
		max_rate = p ? p->value.uint : 0;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_DBSIZE);
		dbsize = p ? p->value.uint : 0;

		if (avg_rate && max_rate) {
			gf_isom_update_bitrate(ctx->file, tkw->track_num, tkw->stsd_idx, avg_rate, max_rate, dbsize);
		}
	} else {
		gf_isom_update_bitrate(ctx->file, tkw->track_num, tkw->stsd_idx, 0, 0, 0);
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
	if (needs_sample_entry) {
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
		} else if (use_3gpp_config && tkw->amr_mode_set) {
			GF_3GPConfig *gpp_cfg = gf_isom_3gp_config_get(ctx->file, tkw->track_num, tkw->stsd_idx);
			if (gpp_cfg->AMR_mode_set != tkw->amr_mode_set) {
				gpp_cfg->AMR_mode_set = tkw->amr_mode_set;
				gf_isom_3gp_config_update(ctx->file, tkw->track_num, gpp_cfg, tkw->stsd_idx);
			}
			gf_free(gpp_cfg);
		}
	}

	if (tkw->is_encrypted) {
		const char *scheme_uri=NULL;
		const char *kms_uri=NULL;
		u32 scheme_version=0;
		u32 scheme_type = 0;
		Bool is_sel_enc = GF_FALSE;
		u32 KI_length=0;
		u32 IV_length=0;
		/*todo !*/
		const char *oma_contentID=0;
		u32 oma_encryption_type=0;
		u64 oma_plainTextLength=0;
		const char *oma_textual_headers=NULL;
		u32 textual_headers_len=0;

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_PROTECTION_SCHEME_TYPE);
		if (p) scheme_type = p->value.uint;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_PROTECTION_SCHEME_VERSION);
		if (p) scheme_version = p->value.uint;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_PROTECTION_SCHEME_URI);
		if (p) scheme_uri = p->value.string;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_PROTECTION_KMS_URI);
		if (p) kms_uri = p->value.string;

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_ISMA_SELECTIVE_ENC);
		if (p) is_sel_enc = p->value.boolean;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_ISMA_IV_LENGTH);
		if (p) IV_length = p->value.uint;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_ISMA_KI_LENGTH);
		if (p) KI_length = p->value.uint;

		switch (scheme_type) {
		case GF_ISOM_ISMACRYP_SCHEME:
			gf_isom_set_ismacryp_protection(ctx->file, tkw->track_num, tkw->stsd_idx, scheme_type, scheme_version, (char *) scheme_uri, (char *) kms_uri, is_sel_enc, KI_length, IV_length);
			break;
		case GF_ISOM_OMADRM_SCHEME:
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_OMA_CRYPT_TYPE);
			if (p) oma_encryption_type = p->value.uint;
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_OMA_CID);
			if (p) oma_contentID = p->value.string;
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_OMA_TXT_HDR);
			if (p) oma_textual_headers = p->value.string;
			if (oma_textual_headers) textual_headers_len = (u32) strlen(oma_textual_headers);
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_OMA_CLEAR_LEN);
			if (p) oma_plainTextLength = p->value.longuint;
			gf_isom_set_oma_protection(ctx->file, tkw->track_num, tkw->stsd_idx, (char *) oma_contentID, (char*) kms_uri, oma_encryption_type, oma_plainTextLength, (char*)oma_textual_headers, textual_headers_len,
                                  is_sel_enc, KI_length, IV_length);

			break;
		case GF_ISOM_ADOBE_SCHEME:
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_ADOBE_CRYPT_META);
			gf_isom_set_adobe_protection(ctx->file, tkw->track_num, tkw->stsd_idx, scheme_type, 1/*scheme_version*/, 1/*is_sel_enc*/,p ? p->value.data.ptr : NULL, p ? p->value.data.size : 0);
			break;
		case GF_ISOM_CENC_SCHEME:
		case GF_ISOM_CENS_SCHEME:
		case GF_ISOM_CBC_SCHEME:
		case GF_ISOM_CBCS_SCHEME:
			tkw->cenc_state = 1;
			if (tkw->is_nalu || tkw->is_av1 || tkw->is_vpx) tkw->cenc_subsamples = GF_TRUE;
			break;
		default:
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MP4Mux] Unrecognized protection scheme type %s, using generic signaling\n", gf_4cc_to_str(tkw->stream_type) ));
			switch (tkw->stream_type) {
			case GF_STREAM_VISUAL:
				gf_isom_set_media_type(ctx->file, tkw->track_num, GF_ISOM_BOX_TYPE_ENCV);
				break;
			case GF_STREAM_AUDIO:
				gf_isom_set_media_type(ctx->file, tkw->track_num, GF_ISOM_BOX_TYPE_ENCA);
				break;
			case GF_STREAM_TEXT:
				gf_isom_set_media_type(ctx->file, tkw->track_num, GF_ISOM_BOX_TYPE_ENCT);
				break;
			case GF_STREAM_FONT:
				gf_isom_set_media_type(ctx->file, tkw->track_num, GF_ISOM_BOX_TYPE_ENCF);
				break;
			default:
				gf_isom_set_media_type(ctx->file, tkw->track_num, GF_ISOM_BOX_TYPE_ENCS);
				break;
			}
			gf_isom_set_generic_protection(ctx->file, tkw->track_num, tkw->stsd_idx, scheme_type, scheme_version, (char*)scheme_uri, (char*)kms_uri);
		}
	} else {
		//in case we used track template
		gf_isom_remove_samp_enc_box(ctx->file, tkw->track_num);
		gf_isom_remove_samp_group_box(ctx->file, tkw->track_num);
	}

	if (is_true_pid) {
		mp4_mux_write_track_refs(ctx, tkw, "isom:scal", GF_ISOM_REF_SCAL);
		mp4_mux_write_track_refs(ctx, tkw, "isom:sabt", GF_ISOM_REF_SABT);
		mp4_mux_write_track_refs(ctx, tkw, "isom:tbas", GF_ISOM_REF_TBAS);
	} else if (codec_id==GF_CODECID_HEVC_TILES) {
		mp4_mux_write_track_refs(ctx, tkw, "isom:tbas", GF_ISOM_REF_TBAS);
	}

	if (is_true_pid && ctx->dash_mode && is_tile_base) {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_DASH_MULTI_TRACK);
		if (p) {
			u32 i, count;
			GF_List *multi_tracks = p->value.ptr;
			count = gf_list_count(multi_tracks);
			for (i=0; i<count; i++) {
				GF_FilterPid *a_ipid = gf_list_get(multi_tracks, i);
				mp4_mux_setup_pid(filter, a_ipid, GF_FALSE);
			}
		}
	}

	if (width) {
		if (ctx->ccst) {
			e = gf_isom_set_image_sequence_coding_constraints(ctx->file, tkw->track_num, tkw->stsd_idx, GF_FALSE, GF_FALSE, GF_TRUE, 15);
			if (e) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MP4Mux] Failed to set coding constraints parameter: %s\n", gf_error_to_string(e) ));
			}
		}
		p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_ALPHA);
		if (p && p->value.boolean) {
			e = gf_isom_set_image_sequence_alpha(ctx->file, tkw->track_num, tkw->stsd_idx, GF_FALSE);
			if (e) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MP4Mux] Failed to set alpha config: %s\n", gf_error_to_string(e) ));
			}
		}
	}

sample_entry_done:
	if (!tkw->is_item) {
		if (ctx->maxchunk)
			gf_isom_hint_max_chunk_size(ctx->file, tkw->track_num, ctx->maxchunk);

		if (sr) {
			if (use_flac_entry) {
				while (sr>65535) {
					u32 val = sr/2;
					if (val*2 != sr) {
						sr=65535;
						break;
					}
					sr = val;
				}
			}
			gf_isom_set_audio_info(ctx->file, tkw->track_num, tkw->stsd_idx, sr, nb_chan, nb_bps, ctx->make_qt ? GF_IMPORT_AUDIO_SAMPLE_ENTRY_v1_QTFF : ase_mode);
		}
		else if (width) {
			u32 colour_type=0;
			u16 colour_primaries=0, transfer_characteristics=0, matrix_coefficients=0;
			Bool full_range_flag=GF_FALSE;

			gf_isom_set_visual_info(ctx->file, tkw->track_num, tkw->stsd_idx, width, height);
			if (sar.den && (sar.num != sar.den)) {
				gf_isom_set_pixel_aspect_ratio(ctx->file, tkw->track_num, tkw->stsd_idx, sar.num, sar.den, GF_FALSE);
				width = width * sar.num / sar.den;
			}

			gf_isom_set_track_layout_info(ctx->file, tkw->track_num, width<<16, height<<16, 0, 0, z_order);
			if (codec_id==GF_CODECID_HEVC_TILES) {
				p = gf_filter_pid_get_property(pid, GF_PROP_PID_ORIG_SIZE);
				if (p) {
					gf_isom_set_track_layout_info(ctx->file, tkw->track_num, p->value.vec2i.x<<16, p->value.vec2i.y<<16, 0, 0, z_order);
				}
			}

			p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_COLR_PRIMARIES);
			if (p) colour_primaries = p->value.uint;
			p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_COLR_TRANSFER);
			if (p) transfer_characteristics = p->value.uint;
			p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_COLR_MX);
			if (p) matrix_coefficients = p->value.uint;
			p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_COLR_RANGE);
			if (p) full_range_flag = p->value.boolean;

			if (ctx->prores_track == tkw) {
				u32 chunk_size;

				//other conditions were set above, here we force 1:1 pasp box even if no sar or 1:1
				if (!sar.den || (sar.num == 1)) {
					gf_isom_set_pixel_aspect_ratio(ctx->file, tkw->track_num, tkw->stsd_idx, -1, -1, GF_TRUE);
				}

				if (colour_primaries || transfer_characteristics || matrix_coefficients) {
					gf_isom_set_visual_color_info(ctx->file, tkw->track_num, tkw->stsd_idx, GF_4CC('n','c','l','c'), colour_primaries, transfer_characteristics, matrix_coefficients, GF_FALSE, NULL, 0);
				} else {
					e = gf_isom_get_color_info(ctx->file, tkw->track_num, tkw->stsd_idx, &colour_type, &colour_primaries, &transfer_characteristics, &matrix_coefficients, &full_range_flag);
					if (e==GF_NOT_FOUND) {
						colour_primaries = 1;
						transfer_characteristics = 1;
						matrix_coefficients = 1;
						full_range_flag = GF_FALSE;
						GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[ProRes] No color info present in visual track, defaulting to BT709\n"));
					}
					gf_isom_set_visual_color_info(ctx->file, tkw->track_num, tkw->stsd_idx, GF_4CC('n','c','l','c'), 1, 1, 1, GF_FALSE, NULL, 0);
				}

				if ((width<=720) && (height<=576)) chunk_size = 2000000;
				else chunk_size = 4000000;
				gf_isom_hint_max_chunk_size(ctx->file, tkw->track_num, chunk_size);

			} else {
				if (colour_primaries || transfer_characteristics || matrix_coefficients) {
					gf_isom_set_visual_color_info(ctx->file, tkw->track_num, tkw->stsd_idx, GF_4CC('n','c','l','x'), colour_primaries, transfer_characteristics, matrix_coefficients, full_range_flag, NULL, 0);
				}

			}
		}
		//default for old arch
		else if (force_tk_layout
			|| (use_m4sys && (tkw->stream_type==GF_STREAM_VISUAL) && !width && !height)
		)  {
			gf_isom_set_track_layout_info(ctx->file, tkw->track_num, 320<<16, 240<<16, 0, 0, 0);
		}

		if (lang_name) gf_isom_set_media_language(ctx->file, tkw->track_num, (char*)lang_name);

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_ISOM_STSD_TEMPLATE);
		if (ctx->tktpl && p && p->value.data.ptr) {
			gf_isom_update_sample_description_from_template(ctx->file, tkw->track_num, tkw->stsd_idx, p->value.data.ptr, p->value.data.size);
		}

	}

	if (is_true_pid && ctx->importer && !tkw->import_msg_header_done) {
		const char *dst_type = tkw->is_item ? "Track Importing" : "Item Importing";
		tkw->import_msg_header_done = GF_TRUE;
		if (!imp_name) imp_name = comp_name;
		if (sr) {
			GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("%s %s - SampleRate %d Num Channels %d\n", dst_type, imp_name, sr, nb_chan));
		} else if (is_text_subs) {
			if (txt_fsize || txt_font) {
				GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("%s %s - Text track %d x %d font %s (size %d) layer %d\n", dst_type, imp_name, width, height, txt_font ? txt_font : "unspecified", txt_fsize, z_order));
			} else {
				GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("%s %s - Text track %d x %d layer %d\n", dst_type, imp_name, width, height, z_order));

			}
		} else if (width) {
			GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("%s %s - Width %d Height %d FPS %d/%d SAR %d/%u\n", dst_type, imp_name, width, height, fps.num, fps.den, sar.num, sar.den));
		} else {
			GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("%s %s\n", dst_type, imp_name));
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
	if (!ctx->xps_inband || tkw->is_item) {
		if (tkw->svcc) {
			gf_odf_avc_cfg_del(tkw->svcc);
			tkw->svcc = NULL;
		}
		if (tkw->lvcc) {
			gf_odf_hevc_cfg_del(tkw->lvcc);
			tkw->lvcc = NULL;
		}
	} else if (needs_sample_entry || make_inband_headers) {
		mp4_mux_make_inband_header(ctx, tkw);
	}

	tkw->negctts_shift = 0;
	tkw->probe_min_ctts = GF_FALSE;
	if (is_true_pid && !tkw->nb_samples && !tkw->is_item) {
		Bool use_negccts = GF_FALSE;
		s64 moffset=0;
		ctx->config_timing = GF_TRUE;
		ctx->update_report = GF_TRUE;

		//if we have an edit list (due to track template) only providing media offset, trash it
		if (!gf_isom_get_edit_list_type(ctx->file, tkw->track_num, &moffset)) {
			gf_isom_remove_edit_segments(ctx->file, tkw->track_num);
		}
		p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_DELAY);
		if (p) {
			if (p->value.sint < 0) {
				if (ctx->ctmode==MP4MX_CT_NEGCTTS) use_negccts = GF_TRUE;
				else {
					gf_isom_set_edit_segment(ctx->file, tkw->track_num, 0, 0, -p->value.sint, GF_ISOM_EDIT_NORMAL);
				}
			} else if (p->value.sint > 0) {
				s64 dur = p->value.sint;
				dur *= (u32) ctx->moovts;
				dur /= tkw->src_timescale;
				gf_isom_set_edit_segment(ctx->file, tkw->track_num, 0, dur, 0, GF_ISOM_EDIT_DWELL);
				gf_isom_set_edit_segment(ctx->file, tkw->track_num, 0, 0, 0, GF_ISOM_EDIT_NORMAL);
			}
			tkw->ts_delay = p->value.sint;
		} else if (tkw->stream_type==GF_STREAM_VISUAL) {
			tkw->probe_min_ctts = GF_TRUE;
		}
		if (use_negccts) {
			gf_isom_set_composition_offset_mode(ctx->file, tkw->track_num, GF_TRUE);

			if (!tkw->has_brands) {
				gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_ISO4, 1);
				gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_ISOM, 0);
				gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_ISO1, 0);
				gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_ISO2, 0);
				gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_ISO3, 0);
			}

			tkw->negctts_shift = (tkw->ts_delay<0) ? -tkw->ts_delay : 0;
		} else {
			//this will remove any cslg in the track due to template
			gf_isom_set_composition_offset_mode(ctx->file, tkw->track_num, GF_FALSE);
		}
	}
	return GF_OK;
}


static GF_Err mp4_mux_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	GF_MP4MuxCtx *ctx = gf_filter_get_udta(filter);
	TrackWriter *tkw;

	if (is_remove) {
		tkw = gf_filter_pid_get_udta(pid);
		if (tkw) {
			gf_list_del_item(ctx->tracks, tkw);
			gf_free(tkw);
		}
		if (ctx->opid && !gf_list_count(ctx->tracks)) {
			gf_filter_pid_remove(ctx->opid);
			ctx->opid = NULL;
		}
		return GF_OK;
	}
	return mp4_mux_setup_pid(filter, pid, GF_TRUE);
}

static GF_Err mp4_mux_done(GF_Filter *filter, GF_MP4MuxCtx *ctx);

static Bool mp4_mux_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	GF_MP4MuxCtx *ctx = gf_filter_get_udta(filter);

	if (evt->base.on_pid && (evt->base.type==GF_FEVT_INFO_UPDATE) ) {
		TrackWriter *tkw = gf_filter_pid_get_udta(evt->base.on_pid);
		if (tkw) {
			GF_PropertyEntry *pe=NULL;
			const GF_PropertyValue *p;
			p = gf_filter_pid_get_info(tkw->ipid, GF_PROP_PID_DOWN_BYTES, &pe);
			if (p) tkw->down_bytes = p->value.longuint;

			p = gf_filter_pid_get_info(tkw->ipid, GF_PROP_PID_DOWN_SIZE, &pe);
			if (p) tkw->down_size = p->value.longuint;

			gf_filter_release_property(pe);
		}

		return GF_FALSE;
	}
	if (!evt->base.on_pid && (evt->base.type==GF_FEVT_STOP) ) {
		if (ctx->file && ctx->owns_mov) {
			mp4_mux_done(filter, ctx);
		}
	}
	if (evt->base.on_pid && (evt->base.type==GF_FEVT_PLAY) ) {
		//by default don't cancel event - to rework once we have downloading in place
		ctx->force_play = GF_TRUE;
		return GF_FALSE;
	}
	//by default don't cancel event - to rework once we have downloading in place
	return GF_FALSE;
}

enum
{
	CENC_CONFIG=0,
	CENC_ADD_NORMAL,
	CENC_ADD_FRAG,
};

static void mp4_mux_cenc_insert_pssh(GF_MP4MuxCtx *ctx, TrackWriter *tkw)
{
	bin128 *keyIDs=NULL;
	u32 max_keys = 0;
	u32 i, nb_pssh;

	//set pssh
	const GF_PropertyValue *p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_CENC_PSSH);
	if (!p) return;

	if (!ctx->bs_r) ctx->bs_r = gf_bs_new(p->value.data.ptr, p->value.data.size, GF_BITSTREAM_READ);
	else gf_bs_reassign_buffer(ctx->bs_r, p->value.data.ptr, p->value.data.size);

	nb_pssh = gf_bs_read_u32(ctx->bs_r);
	for (i = 0; i < nb_pssh; i++) {
		bin128 sysID;
		u32 j, kid_count, version=0;
		char *data;
		u32 len;

		gf_bs_read_data(ctx->bs_r, sysID, 16);
		version = gf_bs_read_u32(ctx->bs_r);
		kid_count = gf_bs_read_u32(ctx->bs_r);

		if (kid_count>=max_keys) {
			max_keys = kid_count;
			keyIDs = gf_realloc(keyIDs, sizeof(bin128)*max_keys);
		}
		for (j=0; j<kid_count; j++) {
			gf_bs_read_data(ctx->bs_r, keyIDs[j], 16);
		}
		len = gf_bs_read_u32(ctx->bs_r);
		data = p->value.data.ptr + gf_bs_get_position(ctx->bs_r);
		gf_cenc_set_pssh(ctx->file, sysID, version, kid_count, keyIDs, data, len);
		gf_bs_skip_bytes(ctx->bs_r, len);
	}
	if (keyIDs) gf_free(keyIDs);
}

static GF_Err mp4_mux_cenc_update(GF_MP4MuxCtx *ctx, TrackWriter *tkw, GF_FilterPacket *pck, u32 act_type, u32 pck_size)
{
	const GF_PropertyValue *p;
	GF_Err e;
	Bool pck_is_encrypted;
	u32 skip_byte_block=0, crypt_byte_block=0;
	u32 IV_size=0, constant_IV_size=0;
	bin128 constant_IV, KID;
	u32 scheme_type=0;
	u32 scheme_version=0;
	char *sai = NULL;
	u32 sai_size = 0;
	Bool needs_seig = GF_FALSE;
	u32 sample_num;

	p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_CENC_PATTERN);
	if (p) {
		skip_byte_block = p->value.frac.num;
		crypt_byte_block = p->value.frac.den;
	}
	p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_CENC_IV_CONST);
	if (p && p->value.data.size) {
		constant_IV_size = p->value.data.size;
		if (constant_IV_size>16) constant_IV_size=16;
		memcpy(constant_IV, p->value.data.ptr, constant_IV_size);
	}
	p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_CENC_IV_SIZE);
	if (p) IV_size = p->value.uint;

	p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_KID);
	if (p) {
		memcpy(KID, p->value.data.ptr, 16);
	}

	if (pck) {
		p = gf_filter_pck_get_property(pck, GF_PROP_PCK_CENC_SAI);
		if (p) {
			sai = p->value.data.ptr;
			sai_size = p->value.data.size;
		}
	}


	p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_PROTECTION_SCHEME_TYPE);
	if (p) scheme_type = p->value.uint;
	p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_PROTECTION_SCHEME_VERSION);
	if (p) scheme_version = p->value.uint;

	//initial setup
	if (tkw->cenc_state==1) {
		u32 container_type = GF_ISOM_BOX_TYPE_SENC;

		p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_ENCRYPTED);
		if (p) pck_is_encrypted = p->value.boolean;
		else pck_is_encrypted = GF_FALSE;


		p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_CENC_STORE);
		if (p) container_type = p->value.uint;

		tkw->cenc_state = 2;
		e = gf_isom_set_cenc_protection(ctx->file, tkw->track_num, tkw->stsd_idx, scheme_type, scheme_version, pck_is_encrypted, IV_size, KID, crypt_byte_block, skip_byte_block, constant_IV_size, constant_IV);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Failed to setup CENC information: %s\n", gf_error_to_string(e) ));
			return e;
		}

		if (ctx->psshs == MP4MX_PSSH_MOOV)
			mp4_mux_cenc_insert_pssh(ctx, tkw);

		tkw->IV_size = IV_size;
		memcpy(tkw->KID, KID, sizeof(bin128));
		tkw->crypt_byte_block = crypt_byte_block;
		tkw->skip_byte_block = skip_byte_block;
		tkw->constant_IV_size = constant_IV_size;
		memcpy(tkw->constant_IV, constant_IV, sizeof(bin128));

		if (!tkw->has_brands && (scheme_type==GF_ISOM_OMADRM_SCHEME))
			gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_OPF2, 1);


		e = gf_isom_cenc_allocate_storage(ctx->file, tkw->track_num, container_type, 0, 0, NULL);
		if (e) return e;
	}
	if (act_type==CENC_CONFIG) return GF_OK;

	pck_is_encrypted = GF_FALSE;
	if (pck)
		pck_is_encrypted = gf_filter_pck_get_crypt_flags(pck);

	//!! tkw->nb_samples / tkw->samples_in_frag not yet incremented !!
	if (act_type == CENC_ADD_FRAG)
		sample_num = tkw->samples_in_frag + 1;
	else
		sample_num = tkw->nb_samples + 1;

	if (!pck_is_encrypted) {
		bin128 dumb_IV;
		memset(dumb_IV, 0, 16);
		e = gf_isom_set_sample_cenc_group(ctx->file, tkw->track_num, sample_num, 0, 0, dumb_IV, 0, 0, 0, NULL);
		IV_size = 0;
		tkw->has_seig = GF_TRUE;
	} else {
		e = GF_OK;
		if (tkw->IV_size != IV_size) needs_seig = GF_TRUE;
		else if (memcmp(tkw->KID, KID, sizeof(bin128))) needs_seig = GF_TRUE;
		else if (tkw->crypt_byte_block != crypt_byte_block) needs_seig = GF_TRUE;
		else if (tkw->skip_byte_block != skip_byte_block) needs_seig = GF_TRUE;
		else if (tkw->constant_IV_size != constant_IV_size) needs_seig = GF_TRUE;
		else if (constant_IV_size && memcmp(tkw->constant_IV, constant_IV, sizeof(bin128))) needs_seig = GF_TRUE;

		if (needs_seig) {
			e = gf_isom_set_sample_cenc_group(ctx->file, tkw->track_num, sample_num, 1, IV_size, KID, crypt_byte_block, skip_byte_block, constant_IV_size, constant_IV);
			tkw->has_seig = GF_TRUE;
		} else if (tkw->has_seig) {
			e = gf_isom_set_sample_cenc_default_group(ctx->file, tkw->track_num, sample_num);
		}
	}
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Failed to set sample encryption group entry: %s)\n", gf_error_to_string(e) ));
		return e;
	}

	if (!sai) {
		sai_size = pck_size;
	}

	if (act_type==CENC_ADD_FRAG) {
		e = gf_isom_fragment_set_cenc_sai(ctx->file, tkw->track_id, IV_size, sai, sai_size, tkw->cenc_subsamples, ctx->saio32);
		if (e) return e;
	} else {
		if (sai) {
			e = gf_isom_track_cenc_add_sample_info(ctx->file, tkw->track_num, GF_ISOM_BOX_TYPE_SENC, IV_size, sai, sai_size, tkw->cenc_subsamples, NULL, ctx->saio32);
			if (e) return e;
		}
	}

	return GF_OK;
}


static GF_Err mp4_mux_process_sample(GF_MP4MuxCtx *ctx, TrackWriter *tkw, GF_FilterPacket *pck, Bool for_fragment)
{
	GF_Err e=GF_OK;
	u64 cts, prev_dts;
	u32 prev_size=0;
	u32 duration = 0;
	u32 timescale = 0;
	const GF_PropertyValue *subs = NULL;
	GF_FilterSAPType sap_type;
	u8 dep_flags;
	Bool insert_subsample_dsi = GF_FALSE;
	u32 first_nal_is_audelim = GF_FALSE;

	timescale = gf_filter_pck_get_timescale(pck);

	subs = gf_filter_pck_get_property(pck, GF_PROP_PCK_SUBS);

	prev_dts = tkw->nb_samples ? tkw->sample.DTS : GF_FILTER_NO_TS;
	prev_size = tkw->sample.dataLength;
	tkw->sample.CTS_Offset = 0;
	tkw->sample.data = (char *)gf_filter_pck_get_data(pck, &tkw->sample.dataLength);
	if (!tkw->sample.dataLength) return GF_OK;

	ctx->update_report = GF_TRUE;
	ctx->total_bytes_in += tkw->sample.dataLength;
	ctx->total_samples++;

	tkw->sample.DTS = gf_filter_pck_get_dts(pck);
	cts = gf_filter_pck_get_cts(pck);
	if (tkw->sample.DTS == GF_FILTER_NO_TS) {
		if (cts == GF_FILTER_NO_TS) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Sample with no DTS/CTS, cannot add (last DTS "LLU", last size %d)\n", prev_dts, prev_size ));
			return GF_NON_COMPLIANT_BITSTREAM;
		} else {
			u32 min_pck_dur = gf_filter_pid_get_min_pck_duration(tkw->ipid);
			if (min_pck_dur) {
				tkw->sample.DTS = prev_dts;
				if (timescale != tkw->tk_timescale) {
					tkw->sample.DTS *= timescale;
					tkw->sample.DTS /= tkw->tk_timescale;
				}
				tkw->sample.DTS += min_pck_dur;
			} else {
				tkw->sample.DTS = cts;
			}
		}
	} else {
		tkw->sample.CTS_Offset = (s32) ((s64) cts - (s64) tkw->sample.DTS);
	}

	duration = gf_filter_pck_get_duration(pck);
	if (timescale != tkw->tk_timescale) {
		s64 ctso;
		tkw->sample.DTS *= tkw->tk_timescale;
		tkw->sample.DTS /= timescale;
		ctso = (s64) tkw->sample.CTS_Offset;
		ctso *= tkw->tk_timescale;
		ctso /= timescale;
		tkw->sample.CTS_Offset = (s32) ctso;
		duration *= tkw->tk_timescale;
		duration /= timescale;
	}


	tkw->sample.IsRAP = 0;
	sap_type = gf_filter_pck_get_sap(pck);
	if (sap_type==GF_FILTER_SAP_1)
		tkw->sample.IsRAP = SAP_TYPE_1;

	if (tkw->ts_shift) {
		assert (tkw->sample.DTS >= tkw->ts_shift);
		tkw->sample.DTS -= tkw->ts_shift;
		cts -= tkw->ts_shift;
	}

	tkw->sample.DTS += tkw->dts_patch;
	if (tkw->nb_samples && (prev_dts >= tkw->sample.DTS) ) {
		//the fragmented API will patch the duration on the fly
		if (!for_fragment) {
			gf_isom_patch_last_sample_duration(ctx->file, tkw->track_num, prev_dts);
		}
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MP4Mux] PID %s Sample %d with DTS "LLU" less than previous sample DTS "LLU", adjusting prev sample duration\n", gf_filter_pid_get_name(tkw->ipid), tkw->nb_samples, tkw->sample.DTS, prev_dts ));

		tkw->dts_patch = prev_dts - tkw->sample.DTS;
		tkw->sample.DTS += tkw->dts_patch;
	}


	if (tkw->negctts_shift)
		tkw->sample.CTS_Offset -= tkw->negctts_shift;

	if (tkw->probe_min_ctts) {
		s32 diff = (s32) ((s64) cts - (s64) tkw->sample.DTS);
		if (diff < tkw->min_neg_ctts)
			tkw->min_neg_ctts = diff;
	}
	if (tkw->sample.CTS_Offset) tkw->has_ctts = GF_TRUE;

	if (tkw->sample.CTS_Offset < tkw->min_neg_ctts)
		tkw->min_neg_ctts = tkw->sample.CTS_Offset;

	tkw->sample.nb_pack = 0;
	if (tkw->raw_audio_bytes_per_sample) {
		tkw->sample.nb_pack = tkw->sample.dataLength / tkw->raw_audio_bytes_per_sample;
		if (tkw->sample.nb_pack) duration /= tkw->sample.nb_pack;
	}

	if (tkw->use_dref) {
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
		if ((tkw->sample.IsRAP || tkw->force_inband_inject) && ctx->xps_inband) {
			char *au_delim=NULL;
			u32 au_delim_size=0;
			char *pck_data = tkw->sample.data;
			u32 pck_data_len = tkw->sample.dataLength;
			tkw->sample.data = tkw->inband_hdr;
			tkw->sample.dataLength = tkw->inband_hdr_size;
			tkw->force_inband_inject = GF_FALSE;

			if (tkw->is_nalu==NALU_AVC) {
				char *nal = pck_data + tkw->nal_unit_size;
				if ((nal[0] & 0x1F) == GF_AVC_NALU_ACCESS_UNIT) {
					first_nal_is_audelim = au_delim_size = 2 + tkw->nal_unit_size;
					au_delim = pck_data;
				}
			} else {
				char *nal = pck_data + tkw->nal_unit_size;
				if (((nal[0] & 0x7E)>>1) == GF_HEVC_NALU_ACCESS_UNIT) {
					first_nal_is_audelim = au_delim_size = 3 + tkw->nal_unit_size;
					au_delim = pck_data;
				}
			}

			if (au_delim) {
				tkw->sample.data = au_delim;
				tkw->sample.dataLength = au_delim_size;
				pck_data += au_delim_size;
				pck_data_len -= au_delim_size;
			}

			if (for_fragment) {
				e = gf_isom_fragment_add_sample(ctx->file, tkw->track_id, &tkw->sample, tkw->stsd_idx, duration, 0, 0, 0);
				if (!e && au_delim) {
					e = gf_isom_fragment_append_data(ctx->file, tkw->track_id, tkw->inband_hdr, tkw->inband_hdr_size, 0);
				}
				if (!e) e = gf_isom_fragment_append_data(ctx->file, tkw->track_id, pck_data, pck_data_len, 0);
			} else {
				e = gf_isom_add_sample(ctx->file, tkw->track_num, tkw->stsd_idx, &tkw->sample);
				if (au_delim && !e) {
					e = gf_isom_append_sample_data(ctx->file, tkw->track_num, tkw->inband_hdr, tkw->inband_hdr_size);
				}
				if (!e) e = gf_isom_append_sample_data(ctx->file, tkw->track_num, pck_data, pck_data_len);
			}
			insert_subsample_dsi = GF_TRUE;
		} else if (for_fragment) {
			e = gf_isom_fragment_add_sample(ctx->file, tkw->track_id, &tkw->sample, tkw->stsd_idx, duration, 0, 0, 0);
		} else {
			e = gf_isom_add_sample(ctx->file, tkw->track_num, tkw->stsd_idx, &tkw->sample);
			if (!e && !duration) {
				gf_isom_set_last_sample_duration(ctx->file, tkw->track_num, 0);
			}
		}

		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Failed to add sample DTS "LLU" - prev DTS "LLU": %s\n", tkw->sample.DTS, prev_dts, gf_error_to_string(e) ));
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MP4Mux] added sample DTS "LLU" - prev DTS "LLU" - prev size %d\n", tkw->sample.DTS, prev_dts, prev_size));
		}

		if (tkw->cenc_state) {
			mp4_mux_cenc_update(ctx, tkw, pck, for_fragment ? CENC_ADD_FRAG : CENC_ADD_NORMAL, tkw->sample.dataLength);
		}
	}

	tkw->nb_samples++;
	tkw->samples_in_stsd++;
	tkw->samples_in_frag++;

	//compat with old arch: write sample to group info fro all samples
	if ((sap_type==3) || tkw->has_open_gop)  {
		if (for_fragment) {
			e = gf_isom_fragment_set_sample_rap_group(ctx->file, tkw->track_id, tkw->samples_in_frag, (sap_type==3) ? GF_TRUE : GF_FALSE, 0);
		} else if (sap_type==3) {
			e = gf_isom_set_sample_rap_group(ctx->file, tkw->track_num, tkw->nb_samples, (sap_type==3) ? GF_TRUE : GF_FALSE, 0);
		}
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Failed to set sample DTS "LLU" SAP 3 in RAP group: %s\n", tkw->sample.DTS, gf_error_to_string(e) ));
		}
		tkw->has_open_gop = GF_TRUE;
	}
	if (!ctx->noroll) {
		if ((sap_type==4) || tkw->has_gdr) {
			s16 roll = gf_filter_pck_get_roll_info(pck);
			if (for_fragment) {
				e = gf_isom_fragment_set_sample_roll_group(ctx->file, tkw->track_id, tkw->samples_in_frag, (sap_type==4) ? GF_TRUE : GF_FALSE, roll);
			} else {
				e = gf_isom_set_sample_roll_group(ctx->file, tkw->track_num, tkw->nb_samples, (sap_type==4) ? GF_TRUE : GF_FALSE, roll);
			}
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Failed to set sample DTS "LLU" SAP 4 roll %s in roll group: %s\n", tkw->sample.DTS, roll, gf_error_to_string(e) ));
			}
			tkw->has_gdr = GF_TRUE;
		}
	}
	
	if (subs) {
		//if no AUDelim nal and inband header injection, push new subsample
		if (!first_nal_is_audelim && insert_subsample_dsi) {
			if (for_fragment) {
				gf_isom_fragment_add_subsample(ctx->file, tkw->track_id, 0, tkw->inband_hdr_size, 0, 0, 0);
			} else {
				gf_isom_add_subsample(ctx->file, tkw->track_num, tkw->nb_samples, 0, tkw->inband_hdr_size, 0, 0, 0);
			}
			insert_subsample_dsi = GF_FALSE;
		}

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

			//we have AUDelim nal and inband header injection, push new subsample for inband header once we have pushed the first subsample (au delim)
			if (insert_subsample_dsi) {
				if (first_nal_is_audelim != subs_size) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] inserting inband param after AU delimiter NALU, but sample has subsample information not aligned on NALU (got %d subsample size but expecting %d) - file might be broken!\n", subs_size, first_nal_is_audelim));
				}
				if (for_fragment) {
					gf_isom_fragment_add_subsample(ctx->file, tkw->track_id, 0, tkw->inband_hdr_size, 0, 0, 0);
				} else {
					gf_isom_add_subsample(ctx->file, tkw->track_num, tkw->nb_samples, 0, tkw->inband_hdr_size, 0, 0, 0);
				}
				insert_subsample_dsi = GF_FALSE;
			}
		}
	}

	dep_flags = gf_filter_pck_get_dependency_flags(pck);
	if (dep_flags) {
		u32 is_leading = (dep_flags>>6) & 0x3;
		u32 depends_on = (dep_flags>>4) & 0x3;
		u32 depended_on = (dep_flags>>2) & 0x3;
		u32 redundant = (dep_flags) & 0x3;
		if (for_fragment) {
			gf_isom_fragment_set_sample_flags(ctx->file, tkw->track_id, is_leading, depends_on, depended_on, redundant);
		} else {
			gf_isom_set_sample_flags(ctx->file, tkw->track_num, tkw->nb_samples, is_leading, depends_on, depended_on, redundant);
		}
	}


	tkw->next_is_first_sample = GF_FALSE;

	if (duration && !for_fragment && !tkw->raw_audio_bytes_per_sample)
		gf_isom_set_last_sample_duration(ctx->file, tkw->track_num, duration);

	if (ctx->idur.num) {
		u64 mdur = gf_isom_get_media_duration(ctx->file, tkw->track_num);

		/*patch to align to old arch */
		if (tkw->stream_type==GF_STREAM_VISUAL) {
			mdur = tkw->sample.DTS;
		}

		if (ctx->importer) {
			gf_set_progress("Import", mdur * ctx->idur.den, ((u64)tkw->tk_timescale) * ctx->idur.num);
		}

		if (mdur * (u64) ctx->idur.den > tkw->tk_timescale * (u64) ctx->idur.num) {
			GF_FilterEvent evt;
			GF_FEVT_INIT(evt, GF_FEVT_STOP, tkw->ipid);
			gf_filter_pid_send_event(tkw->ipid, &evt);

			tkw->aborted = GF_TRUE;
		}
	} else if (ctx->importer) {
		u64 data_offset = gf_filter_pck_get_byte_offset(pck);
		if (data_offset == GF_FILTER_NO_BO) {
			data_offset = tkw->down_bytes;
		}
		if ((data_offset != GF_FILTER_NO_BO) && tkw->down_size) {
			gf_set_progress("Import", data_offset, tkw->down_size);
		} else {
			if (tkw->pid_dur.den && tkw->pid_dur.num) {
				gf_set_progress("Import", tkw->sample.DTS * tkw->pid_dur.den, tkw->pid_dur.num * tkw->tk_timescale);
			} else {
				gf_set_progress("Import", 0, 1);
			}

		}
	}
	return GF_OK;
}

static GF_Err mp4_mux_process_item(GF_MP4MuxCtx *ctx, TrackWriter *tkw, GF_FilterPacket *pck)
{
	GF_Err e;
	u32 meta_type, item_id, size, item_type, nb_items;
	GF_ImageItemProperties image_props;
	const char *data, *item_name=NULL;
	const GF_PropertyValue *p, *dsi, *dsi_enh;
	GF_Box *config_box = NULL;


	if (ctx->init_movie_done) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Cannot add item to a finalized movie, not supported\n"));
		return GF_NOT_SUPPORTED;
	}

	if (tkw->stream_type != GF_STREAM_VISUAL) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Cannot add item other than visual, not supported - use MP4Box for this\n"));
		return GF_NOT_SUPPORTED;
	}
	ctx->update_report = GF_TRUE;

	meta_type = gf_isom_get_meta_type(ctx->file, GF_TRUE, 0);
	if (!meta_type) {
		e = gf_isom_set_meta_type(ctx->file, GF_TRUE, 0, GF_META_ITEM_TYPE_PICT);
	} else if (meta_type != GF_META_ITEM_TYPE_PICT) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] File already has a root 'meta' box of type %s\n", gf_4cc_to_str(meta_type)));
		e= GF_BAD_PARAM;
	} else {
		e = GF_OK;
	}
	if (e) return e;

	data = (char *)gf_filter_pck_get_data(pck, &size);
	if (!data) {
		if (gf_filter_pck_get_frame_interface(pck)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Cannot add items from raw decoder outputs, not supported\n"));
			return GF_NOT_SUPPORTED;
		}
		return GF_OK;
	}
	ctx->total_bytes_in += size;
	ctx->total_samples++;


	item_id = 0;
	p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_ITEM_ID);
	if (p) item_id = p->value.uint;

	item_name = "Image";
	p = gf_filter_pid_get_property_str(tkw->ipid, "meta:name");
	if (p && p->value.string) item_name = p->value.string;

	item_type = 0;
	memset(&image_props, 0, sizeof(GF_ImageItemProperties));

	dsi = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_DECODER_CONFIG);
	dsi_enh = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT);


	switch (tkw->codecid) {
	case GF_CODECID_AVC:
	case GF_ISOM_SUBTYPE_SVC_H264:
	case GF_ISOM_SUBTYPE_MVC_H264:
		if (!dsi) return GF_OK;

		if (tkw->codecid==GF_CODECID_AVC) {
			config_box = gf_isom_box_new(GF_ISOM_BOX_TYPE_AVCC);
			item_type = GF_ISOM_SUBTYPE_AVC_H264;
		} else if (tkw->codecid==GF_CODECID_MVC) {
			config_box = gf_isom_box_new(GF_ISOM_BOX_TYPE_MVCC);
			item_type = GF_ISOM_SUBTYPE_MVC_H264;
			if (dsi_enh) dsi = dsi_enh;
		} else {
			config_box = gf_isom_box_new(GF_ISOM_BOX_TYPE_SVCC);
			item_type = GF_ISOM_SUBTYPE_SVC_H264;
			if (dsi_enh) dsi = dsi_enh;
		}

		((GF_AVCConfigurationBox *)config_box)->config = gf_odf_avc_cfg_read(dsi->value.data.ptr, dsi->value.data.size);
		if (! ((GF_AVCConfigurationBox *)config_box)->config) return GF_NON_COMPLIANT_BITSTREAM;

		image_props.num_channels = 3;
		image_props.bits_per_channel[0] = ((GF_AVCConfigurationBox *)config_box)->config->luma_bit_depth;
		image_props.bits_per_channel[1] = ((GF_AVCConfigurationBox *)config_box)->config->chroma_bit_depth;
		image_props.bits_per_channel[2] = ((GF_AVCConfigurationBox *)config_box)->config->chroma_bit_depth;
		break;


	case GF_CODECID_HEVC:
	case GF_CODECID_HEVC_TILES:
	case GF_CODECID_LHVC:
		if (tkw->codecid == GF_CODECID_LHVC) {
			if (dsi_enh) dsi = dsi_enh;
			if (!dsi) return GF_OK;
		}
		config_box = gf_isom_box_new(GF_ISOM_BOX_TYPE_HVCC);

		if (dsi_enh) {
			((GF_HEVCConfigurationBox *)config_box)->config = gf_odf_hevc_cfg_read(dsi->value.data.ptr, dsi->value.data.size, GF_TRUE);
			item_type = GF_ISOM_SUBTYPE_LHV1;
		} else {
			if ((tkw->codecid == GF_CODECID_HEVC) && !dsi) return GF_OK;

			((GF_HEVCConfigurationBox *)config_box)->config = gf_odf_hevc_cfg_read(dsi->value.data.ptr, dsi->value.data.size, GF_FALSE);
			item_type = (tkw->codecid == GF_CODECID_HEVC_TILES) ? GF_ISOM_SUBTYPE_HVT1 : GF_ISOM_SUBTYPE_HVC1;
		}
		if (! ((GF_HEVCConfigurationBox *)config_box)->config) {
			if ((tkw->codecid != GF_CODECID_HEVC_TILES) && !dsi) return GF_NON_COMPLIANT_BITSTREAM;
		}
		image_props.num_channels = 3;
		image_props.bits_per_channel[0] = ((GF_HEVCConfigurationBox *)config_box)->config->luma_bit_depth;
		image_props.bits_per_channel[1] = ((GF_HEVCConfigurationBox *)config_box)->config->chroma_bit_depth;
		image_props.bits_per_channel[2] = ((GF_HEVCConfigurationBox *)config_box)->config->chroma_bit_depth;
		//media_brand = GF_ISOM_BRAND_HEIC;
		break;
	case GF_CODECID_AV1:
		if (!dsi) return GF_OK;

		config_box = gf_isom_box_new(GF_ISOM_BOX_TYPE_AV1C);
		((GF_AV1ConfigurationBox *)config_box)->config = gf_odf_av1_cfg_read(dsi->value.data.ptr, dsi->value.data.size);

		if (! ((GF_AV1ConfigurationBox *)config_box)->config) return GF_NON_COMPLIANT_BITSTREAM;

		item_type = GF_ISOM_SUBTYPE_AV01;
		u8 depth = ((GF_AV1ConfigurationBox *)config_box)->config->high_bitdepth ? (((GF_AV1ConfigurationBox *)config_box)->config->twelve_bit ? 12 : 10 ) : 8;
		if (((GF_AV1ConfigurationBox *)config_box)->config->monochrome) {
			image_props.num_channels = 1;
			image_props.bits_per_channel[0] = depth;
			image_props.bits_per_channel[1] = 0;
			image_props.bits_per_channel[2] = 0;
		} else {
			image_props.num_channels = 3;
			image_props.bits_per_channel[0] = depth;
			image_props.bits_per_channel[1] = depth;
			image_props.bits_per_channel[2] = depth;
		}
		//media_brand = GF_ISOM_BRAND_AVIF;
		break;
	case GF_CODECID_JPEG:
		item_type = GF_ISOM_SUBTYPE_JPEG;
		break;
	case GF_CODECID_J2K:
		item_type = GF_ISOM_SUBTYPE_JP2K;
		break;
	case GF_CODECID_PNG:
		item_type = GF_ISOM_SUBTYPE_PNG;
		break;
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Error: Codec %s not supported to create HEIF image items\n", gf_codecid_name(tkw->codecid) ));
		return GF_NOT_SUPPORTED;
	}

	p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_WIDTH);
	if (p) image_props.width = p->value.uint;
	p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_HEIGHT);
	if (p) image_props.height = p->value.uint;
	p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_ALPHA);
	if (p) image_props.alpha = p->value.boolean;
	p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_SAR);
	if (p) {
		image_props.hSpacing = p->value.frac.num;
		image_props.vSpacing = p->value.frac.den;
	} else {
		image_props.hSpacing = image_props.vSpacing = 1;
	}
	image_props.config = config_box;
	p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_HIDDEN);
	if (p) image_props.hidden = p->value.boolean;

	nb_items = gf_isom_get_meta_item_count(ctx->file, GF_TRUE, 0);

	e = gf_isom_add_meta_item_memory(ctx->file, GF_TRUE, 0, item_name, item_id, item_type, NULL, NULL, &image_props, (u8 *)data, size, NULL);

	if (config_box) gf_isom_box_del(config_box);

	if (e) return e;

	//retrieve the final itemID
	gf_isom_get_meta_item_info(ctx->file, GF_TRUE, 0, nb_items+1, &item_id, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

	p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_PRIMARY_ITEM);
	if (p && p->value.boolean) {
		e = gf_isom_set_meta_primary_item(ctx->file, GF_TRUE, 0, item_id);
		if (e) return e;
	}

#if 0
	if (e == GF_OK && meta->ref_type) {
		e = gf_isom_meta_add_item_ref(file, meta->root_meta, tk, meta->item_id, meta->ref_item_id, meta->ref_type, NULL);
	}
#endif
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
			ctx->current_offset = 0;
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


static GF_Err mp4_mux_initialize_movie(GF_MP4MuxCtx *ctx)
{
	GF_Err e;
	u32 i, count = gf_list_count(ctx->tracks);
	TrackWriter *ref_tkw = NULL;
	u64 min_dts = 0;
	u32 min_dts_scale=0;
	u32 def_fake_dur=0;
	u32 def_fake_scale=0;
	GF_Fraction max_dur;
	ctx->single_file = GF_TRUE;
	ctx->current_offset = ctx->current_size = 0;
	max_dur.den = 1;
	max_dur.num = 0;

	if (ctx->sseg)
		ctx->single_file = GF_FALSE;

	if (ctx->idur.num && ctx->idur.den) {
		max_dur.num = ctx->idur.num;
		max_dur.den = ctx->idur.den;
	}

	//make sure we have one sample from each PID. This will trigger potential pending reconfigure
	//for filters updating the PID caps before the first packet dispatch
	for (i=0; i<count; i++) {
		const GF_PropertyValue *p;
		TrackWriter *tkw = gf_list_get(ctx->tracks, i);
		GF_FilterPacket *pck;
		if (tkw->fake_track) continue;

		pck = gf_filter_pid_get_packet(tkw->ipid);
		if (!pck) return GF_OK;

		if (tkw->cenc_state==1) {
			mp4_mux_cenc_update(ctx, tkw, pck, CENC_CONFIG, 0);
		}

		p = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENAME);
		if (p && strlen(p->value.string)) ctx->single_file = GF_FALSE;

		def_fake_dur = gf_filter_pck_get_duration(pck);
		def_fake_scale = tkw->src_timescale;

		p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_DURATION);
		if (p && p->value.frac.num && p->value.frac.den) {
			tkw->pid_dur = p->value.frac;
			if (max_dur.num * p->value.frac.den < max_dur.den * p->value.frac.num) {
				max_dur.num = p->value.frac.num;
				max_dur.den = p->value.frac.den;
			}
		}
	}
	//good to go, finalize for fragments
	for (i=0; i<count; i++) {
		u32 def_pck_dur;
		u32 def_is_rap, tscale;
		u64 dts;
		TrackWriter *tkw = gf_list_get(ctx->tracks, i);
		GF_FilterPacket *pck;

		if (tkw->fake_track) {
			def_is_rap = GF_FALSE;
			if (def_fake_scale) {
				def_pck_dur = def_fake_dur;
				def_pck_dur *= tkw->src_timescale;
				def_pck_dur /= def_fake_scale;
			} else {
				def_pck_dur = 0;
			}
		} else {
			pck = gf_filter_pid_get_packet(tkw->ipid);
			assert(pck);

			//otherwise setup fragmentation, using first sample desc as default idx
			//first pck dur as default
			def_pck_dur = gf_filter_pck_get_duration(pck);

			dts = gf_filter_pck_get_dts(pck);
			if (dts == GF_FILTER_NO_TS)
				dts = gf_filter_pck_get_cts(pck);
			tscale = gf_filter_pck_get_timescale(pck);

			if (!min_dts || min_dts * tscale > dts * min_dts_scale) {
				min_dts = dts;
				min_dts_scale = tscale;
			}
		}
		if (tkw->src_timescale != tkw->tk_timescale) {
			def_pck_dur *= tkw->tk_timescale;
			def_pck_dur /= tkw->src_timescale;
		}

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

		mp4_mux_set_hevc_groups(ctx, tkw);

		//use 1 for the default sample description index. If no multi stsd, this is always the case
		//otherwise we need to the stsd idx in the traf headers
		e = gf_isom_setup_track_fragment(ctx->file, tkw->track_id, 1, def_pck_dur, 0, (u8) def_is_rap, 0, 0, ctx->nofragdef ? 1 : 0);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Unable to setup fragmentation for track ID %d: %s\n", tkw->track_id, gf_error_to_string(e) ));
			return e;
		}

		if (ctx->tfdt.den && ctx->tfdt.num) {
			tkw->offset_dts = ctx->tfdt.num * tkw->tk_timescale;
			tkw->offset_dts /= ctx->tfdt.den;
		}

		if (tkw->fake_track) {
			gf_list_del_item(ctx->tracks, tkw);
			if (ref_tkw==tkw) ref_tkw=NULL;
			mp4_mux_track_writer_del(tkw);
			i--;
			count--;
		}
	}

	if (max_dur.num) {
		u64 mdur = max_dur.num;
		mdur *= (u32) ctx->moovts;
		mdur /= max_dur.den;
		gf_isom_set_movie_duration(ctx->file, mdur);
	}

	//if we have an explicit track reference for fragmenting, move it first in our list
	if (ref_tkw) {
		gf_list_del_item(ctx->tracks, ref_tkw);
		gf_list_insert(ctx->tracks, ref_tkw, 0);
	}
	ctx->ref_tkw = gf_list_get(ctx->tracks, 0);

	if (!ctx->abs_offset) {
		u32 mval = ctx->dash_mode ? '6' : '5';
		u32 mbrand, mcount, i, found=0;
		u8 szB[5];
		gf_isom_set_fragment_option(ctx->file, 0, GF_ISOM_TFHD_FORCE_MOOF_BASE_OFFSET, 1);

		gf_isom_get_brand_info(ctx->file, &mbrand, NULL, &mcount);
		strcpy(szB, gf_4cc_to_str(mbrand));
		if (!strncmp(szB, "iso", 3) && (szB[3] >= mval) && (szB[3] <= 'F') ) found = 1;
		i=0;
		while (!found && (i<mcount)) {
			i++;
			gf_isom_get_alternate_brand(ctx->file, i, &mbrand);
			strcpy(szB, gf_4cc_to_str(mbrand));
			if (!strncmp(szB, "iso", 3) && (szB[3] >= mval) && (szB[3] <= 'F') ) found = 1;
		}

		/*because of movie fragments MOOF based offset, ISOM <4 is forbidden*/
		if (!found) {
			gf_isom_set_brand_info(ctx->file, ctx->dash_mode ? GF_ISOM_BRAND_ISO6 : GF_ISOM_BRAND_ISO5, 1);
		}

		gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_ISOM, 0);
		gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_ISO1, 0);
		gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_ISO2, 0);
		gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_ISO3, 0);
		gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_ISO4, 0);
		gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_AVC1, 0);
		gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_MP41, 0);
		gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_MP42, 0);
	}

	if (ctx->dash_mode) {
		/*DASH self-init media segment*/
		if (ctx->dash_mode==MP4MX_DASH_VOD) {
			gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_DSMS, 1);
		} else {
			gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_DASH, 1);
		}
		gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_MSIX, ((ctx->dash_mode==MP4MX_DASH_VOD) && (ctx->subs_sidx>=0)) ? 1 : 0);
	}


	e = gf_isom_finalize_for_fragment(ctx->file, ctx->dash_mode ? 1 : 0, ctx->mvex);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Unable to finalize moov for fragmentation: %s\n", gf_error_to_string(e) ));
		return e;
	}
	ctx->init_movie_done = GF_TRUE;

	if (min_dts_scale) {
		ctx->next_frag_start = (Double) min_dts;
		ctx->next_frag_start /= min_dts_scale;
	}
	ctx->next_frag_start += ctx->cdur;
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

	//change major brand for segments
	if (ctx->styp && (strlen(ctx->styp)>=4)) {
		u32 styp_brand = GF_4CC(ctx->styp[0], ctx->styp[1], ctx->styp[2], ctx->styp[3]);
		u32 version = 0;
		char *sep = strchr(ctx->styp, '.');
		if (sep) version = atoi(sep+1);
		gf_isom_set_brand_info(ctx->file, styp_brand, version);
	}

	if (ctx->dash_mode==MP4MX_DASH_VOD) {
		if (!ctx->cache && (!ctx->media_dur || !ctx->dash_dur) ) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MP4Mux] Media duration unknown, cannot use nocache mode, using temp file for VoD storage\n"));
			ctx->cache = GF_TRUE;
			e = mp4mx_setup_dash_vod(ctx, NULL);
			if (e) return e;
		}

		if (!ctx->cache) {
			GF_BitStream *bs;
			u8 *output;
			char *msg;
			GF_FilterPacket *pck;
			u32 len, nb_segs = (u32) ( ctx->media_dur / ctx->dash_dur);
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
			ctx->sidx_chunk_offset = (u32) (ctx->current_offset + ctx->current_size);
			//send a dummy packet
			pck = gf_filter_pck_new_alloc(ctx->opid, ctx->sidx_max_size, &output);
			gf_filter_pck_set_framing(pck, GF_FALSE, GF_FALSE);
			//format as free box for now
			bs = gf_bs_new(output, ctx->sidx_max_size, GF_BITSTREAM_WRITE);
			gf_bs_write_u32(bs, ctx->sidx_max_size);
			gf_bs_write_u32(bs, GF_ISOM_BOX_TYPE_FREE);
			msg = "GPAC " GPAC_VERSION" SIDX placeholder";
			len = (u32) strlen(msg);
			if (len+8>ctx->sidx_max_size) len = ctx->sidx_max_size - 8;
			gf_bs_write_data(bs, msg, len );
			gf_bs_del(bs);
			gf_filter_pck_send(pck);
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DISABLE_PROGRESSIVE, &PROP_BOOL(GF_TRUE));
		} else {
			ctx->store_output = GF_TRUE;
		}
		gf_isom_allocate_sidx(ctx->file, ctx->subs_sidx, ctx->chain_sidx, 0, NULL, NULL, NULL, ctx->ssix);
	}
	return GF_OK;
}

static GF_Err mp4_mux_start_fragment(GF_MP4MuxCtx *ctx, GF_FilterPacket *pck)
{
	GF_Err e;
	u32 i, count = gf_list_count(ctx->tracks);
	Bool has_tfdt=GF_FALSE;

	//setup some default
	gf_isom_set_next_moof_number(ctx->file, ctx->msn);
	ctx->msn += ctx->msninc;

	e = gf_isom_start_fragment(ctx->file, ctx->moof_first);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Unable to start new fragment: %s\n", gf_error_to_string(e) ));
		return e;
	}
	if (pck) {
		const GF_PropertyValue *p = gf_filter_pck_get_property(pck, GF_PROP_PCK_MOOF_TEMPLATE);
		if (p && p->value.data.ptr) {
			GF_SegmentIndexBox *out_sidx = NULL;
			gf_isom_set_fragment_template(ctx->file, p->value.data.ptr, p->value.data.size, &has_tfdt, &out_sidx);
			if (out_sidx) {
				if (ctx->cloned_sidx) gf_isom_box_del((GF_Box *)ctx->cloned_sidx);
				ctx->cloned_sidx = out_sidx;
				ctx->cloned_sidx_index = 0;
			}
		}
	}

	//setup some default
	for (i=0; i<count; i++) {
		TrackWriter *tkw = gf_list_get(ctx->tracks, i);
		e = GF_OK;
		if (ctx->strun) {
			e = gf_isom_set_fragment_option(ctx->file, tkw->track_id, GF_ISOM_TRAF_RANDOM_ACCESS, 0);

		}
		//fragment at sap boundaries for video, but not in dash mode (compatibility with old arch)
		else if (ctx->fsap && (tkw->stream_type == GF_STREAM_VISUAL) && !ctx->dash_mode) {
			e = gf_isom_set_fragment_option(ctx->file, tkw->track_id, GF_ISOM_TRAF_RANDOM_ACCESS, 1);
			if (e) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MP4Mux] Unable set fragment options: %s\n", gf_error_to_string(e) ));
			}
		}
		tkw->fragment_done = GF_FALSE;
		tkw->insert_tfdt = (has_tfdt || ctx->tfdt_traf) ? GF_TRUE : ctx->insert_tfdt;
		tkw->dur_in_frag = 0;

		if (ctx->insert_pssh)
			mp4_mux_cenc_insert_pssh(ctx, tkw);

	}
	ctx->fragment_started = GF_TRUE;
	ctx->insert_tfdt = GF_FALSE;
	ctx->insert_pssh = GF_FALSE;
	return GF_OK;
}

static GF_Err mp4_mux_flush_fragmented(GF_Filter *filter, GF_MP4MuxCtx *ctx)
{
	GF_FilterPacket *pck;
	u8 *output;
	u32 nb_read, blocksize = ctx->block_size;
	if (ctx->flush_done + blocksize>ctx->flush_size) {
		blocksize = (u32) (ctx->flush_size - ctx->flush_done);
	}
	if (!blocksize) return GF_EOS;
	pck = gf_filter_pck_new_alloc(ctx->opid, blocksize, &output);
	nb_read = (u32) fread(output, 1, blocksize, ctx->tmp_store);
	ctx->flush_done += nb_read;
	if (nb_read != blocksize) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Error flushing fragmented file, read %d bytes but asked %d bytes\n", nb_read, blocksize));
	}
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

static GF_Err mp4_mux_process_fragmented(GF_Filter *filter, GF_MP4MuxCtx *ctx)
{
	GF_Err e = GF_OK;
	u32 nb_eos, nb_done, i, count;

	if (ctx->flush_size) {
		return mp4_mux_flush_fragmented(filter, ctx);
	}

	if (!ctx->file)
		return GF_EOS;

	//init movie not yet produced
	if (!ctx->init_movie_done) {
		e = mp4_mux_initialize_movie(ctx);
		if (e) return e;
	}
	/*get count after init, some tracks may have been remove*/
	count = gf_list_count(ctx->tracks);
	if (ctx->dash_mode && !ctx->segment_started) {
		//don't start a new segment if all pids are in eos
		nb_eos=0;
		for (i=0; i<count; i++) {
			TrackWriter *tkw = gf_list_get(ctx->tracks, i);
			if (gf_filter_pid_is_eos(tkw->ipid)) {
				nb_eos ++;
			}
		}
		if (nb_eos==count)
			return GF_EOS;
		ctx->segment_started = GF_TRUE;
		ctx->insert_tfdt = GF_TRUE;
		ctx->insert_pssh = (ctx->psshs == MP4MX_PSSH_MOOF) ? GF_TRUE : GF_FALSE;

		gf_isom_start_segment(ctx->file, ctx->single_file ? NULL : "_gpac_isobmff_redirect", GF_FALSE);
	}

	//process pid by pid
	nb_eos=0;
	nb_done = 0;
	for (i=0; i<count; i++) {
		u64 cts, dts, ncts;
		TrackWriter *tkw = gf_list_get(ctx->tracks, i);

		if (ctx->fragment_started && tkw->fragment_done) {
			nb_done ++;
			continue;
		}

		while (1) {
			const GF_PropertyValue *p;
			u32 orig_frag_bounds=0;
			GF_FilterPacket *pck = gf_filter_pid_get_packet(tkw->ipid);

			if (!pck) {
				if (gf_filter_pid_is_eos(tkw->ipid)) {
					tkw->fragment_done = GF_TRUE;
					if (ctx->dash_mode) ctx->flush_seg = GF_TRUE;
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
				if (ctx->dash_mode) ctx->flush_seg = GF_TRUE;
				break;
			}

			cts = gf_filter_pck_get_cts(pck);

			if (cts == GF_FILTER_NO_TS) {
				const GF_PropertyValue *p = gf_filter_pck_get_property(pck, GF_PROP_PCK_EODS);
				if (p && p->value.boolean) {
					nb_done ++;
					tkw->fragment_done = GF_TRUE;
					gf_filter_pid_drop_packet(tkw->ipid);
					ctx->flush_seg = GF_TRUE;
					tkw->next_seg_cts = tkw->cts_next;
					break;
				}
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MuxIsom] Packet with no CTS assigned, cannot store to track, ignoring\n"));
				gf_filter_pid_drop_packet(tkw->ipid);
				continue;
			}

			p = gf_filter_pck_get_property(pck, GF_PROP_PCK_FRAG_START);
			if (p) {
				orig_frag_bounds = p->value.uint;

				if (orig_frag_bounds==2) {
					if (!ctx->segment_started) {
						ctx->dash_mode = 1;
						ctx->insert_tfdt = GF_TRUE;
						gf_isom_start_segment(ctx->file, ctx->single_file ? NULL : "_gpac_isobmff_redirect", GF_FALSE);
					} else if (tkw->samples_in_frag) {
						tkw->fragment_done = GF_TRUE;
						tkw->samples_in_frag = 0;
						nb_done ++;
						//make sure we flush until the end of the segment
						ctx->flush_seg = GF_TRUE;
						//store CTS of next packet (first in next segment) for sidx compute
						tkw->next_seg_cts = cts;
					}
				}
			}

			if (!ctx->fragment_started) {
				e = mp4_mux_start_fragment(ctx, orig_frag_bounds ? pck : NULL);
				if (e) return e;

				ctx->nb_frags++;
				if (ctx->dash_mode)
					ctx->nb_frags_in_seg++;

			}


			if (ctx->dash_mode) {
				//get dash segment number
				p = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENUM);
				if (p) {
					//start of next segment, abort fragmentation for this track and flush all other writers
					if (ctx->dash_seg_num && (ctx->dash_seg_num != p->value.uint) ) {
						tkw->fragment_done = GF_TRUE;
						tkw->samples_in_frag = 0;
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
			ncts = cts + gf_filter_pck_get_duration(pck);
			if (tkw->cts_next < ncts)
				tkw->cts_next = ncts;


			if (tkw->samples_in_frag && orig_frag_bounds) {
				tkw->fragment_done = GF_TRUE;
				nb_done ++;
				tkw->samples_in_frag = 0;
				tkw->dur_in_frag = 0;
				break;
			} else if (ctx->fdur && (!ctx->dash_mode || !tkw->fragment_done) ) {
				u32 dur = gf_filter_pck_get_duration(pck);
				if (tkw->dur_in_frag && (tkw->dur_in_frag >= ctx->cdur * tkw->src_timescale)) {
					tkw->fragment_done = GF_TRUE;
					nb_done ++;
					tkw->dur_in_frag = 0;
					tkw->samples_in_frag = 0;
					break;
				}
				tkw->dur_in_frag += dur;
			} else if (!ctx->flush_seg && !ctx->dash_mode
				&& (cts >= (u64) (ctx->adjusted_next_frag_start * tkw->src_timescale) + tkw->ts_delay)
			 ) {
				u32 sap = gf_filter_pck_get_sap(pck);
				if ((ctx->store==MP4MX_MODE_FRAG) || (sap && sap<GF_FILTER_SAP_3)) {
					tkw->fragment_done = GF_TRUE;
					tkw->samples_in_frag = 0;
					nb_done ++;
					if (ctx->store==MP4MX_MODE_SFRAG) {
						ctx->adjusted_next_frag_start = (Double) (cts - tkw->ts_delay);
						ctx->adjusted_next_frag_start /= tkw->src_timescale;
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
		if (ctx->straf && (i+1 < count)) {
			e = gf_isom_start_fragment(ctx->file, ctx->moof_first);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Unable to start new fragment: %s\n", gf_error_to_string(e) ));
				return e;
			}
		}
	}
	if (nb_done==count) {
		Bool is_eos = (count == nb_eos) ? GF_TRUE : GF_FALSE;
		Double ref_start;
		u64 next_ref_ts = ctx->ref_tkw->next_seg_cts;
		if (is_eos)
			next_ref_ts = ctx->ref_tkw->cts_next;
		ref_start = (Double) next_ref_ts;
		ref_start /= ctx->ref_tkw->src_timescale;

		ctx->next_frag_start += ctx->cdur;
		while (ctx->next_frag_start <= ctx->adjusted_next_frag_start) {
			ctx->next_frag_start += ctx->cdur;
		}
		ctx->adjusted_next_frag_start = ctx->next_frag_start;
		ctx->fragment_started = GF_FALSE;

		//end of DASH segment
		if (ctx->dash_mode && (ctx->flush_seg || is_eos) ) {
			u64 offset = ctx->single_file ? ctx->current_offset : 0;
			u64 idx_start_range, idx_end_range, segment_size_in_bytes;
			s32 subs_sidx = -1;
			u32 track_ref_id = 0;

			idx_start_range = idx_end_range = 0;
			if (ctx->subs_sidx>=0) {
				subs_sidx = ctx->subs_sidx;
				track_ref_id = ctx->ref_tkw->track_id;
			}
			if (ctx->cloned_sidx && (ctx->subs_sidx!=-2) ) {
				subs_sidx = (s32) ctx->cloned_sidx->nb_refs;
				track_ref_id = ctx->cloned_sidx->reference_ID;
				gf_isom_box_del((GF_Box *)ctx->cloned_sidx);
				ctx->cloned_sidx = NULL;
			}

			gf_isom_close_segment(ctx->file, subs_sidx, track_ref_id, ctx->ref_tkw->first_dts_in_seg, ctx->ref_tkw->ts_delay, next_ref_ts, ctx->chain_sidx, ctx->ssix, ctx->sseg ? GF_FALSE : is_eos, GF_FALSE, ctx->eos_marker, &idx_start_range, &idx_end_range, &segment_size_in_bytes);

			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MP4Mux] Done writing segment %d - estimated next fragment times start %g end %g\n", ctx->dash_seg_num, ref_start, ctx->next_frag_start ));

			if (ctx->dash_mode != MP4MX_DASH_VOD) {
				mp4_mux_flush_frag(ctx, 0, offset + idx_start_range, idx_end_range ? offset + idx_end_range : 0);
			} else if (!ctx->cache) {
				mp4_mux_flush_frag(ctx, 0, 0, 0);
			} else {
				if (ctx->nb_seg_sizes == ctx->alloc_seg_sizes) {
					 ctx->alloc_seg_sizes *= 2;
					 if (!ctx->alloc_seg_sizes) ctx->alloc_seg_sizes = 10;
					 ctx->seg_sizes = gf_realloc(ctx->seg_sizes, sizeof(u32) * ctx->alloc_seg_sizes);
				}
				assert(segment_size_in_bytes);
				ctx->seg_sizes[ctx->nb_seg_sizes] = (u32) segment_size_in_bytes;
				ctx->nb_seg_sizes++;
			}
		}
		//cannot flush in DASH mode if using sidx (vod single sidx or live 1 sidx/seg)
		else if (!ctx->dash_mode || ((ctx->subs_sidx<0) && (ctx->dash_mode<MP4MX_DASH_VOD) && !ctx->cloned_sidx) ) {
			gf_isom_flush_fragments(ctx->file, GF_FALSE);

			if (!ctx->dash_mode || ctx->flush_seg)
				mp4_mux_flush_frag(ctx, 0, 0, 0);

			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MP4Mux] Done writing fragment - next fragment start time %g\n", ctx->next_frag_start ));
		}

		if (ctx->flush_seg) {
			ctx->segment_started = GF_FALSE;
			ctx->flush_seg = GF_FALSE;
			ctx->dash_seg_num = 0;
			ctx->nb_segs++;
			ctx->nb_frags_in_seg=0;
		}
	}

	if (count == nb_eos) {
		if (ctx->dash_mode==MP4MX_DASH_VOD) {
			if (!ctx->cache) {
				ctx->final_sidx_flush = GF_TRUE;
				//flush SIDX in given space, reserve 8 bytes for free box
				gf_isom_flush_sidx(ctx->file, ctx->sidx_max_size - 8);
			} else {
				u64 start_offset;
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

				if (ctx->seg_sizes) {
					start_offset = ctx->current_offset;
					for (i=0; i<ctx->nb_seg_sizes; i++) {
						ctx->current_size = ctx->seg_sizes[i];
						mp4_mux_flush_frag(ctx, 0, 0, 0);
					}
					ctx->current_offset = start_offset;
					ctx->current_size = 0;

					gf_free(ctx->seg_sizes);
					ctx->seg_sizes = NULL;
					ctx->alloc_seg_sizes = ctx->nb_seg_sizes = 0;
				}
			}
		}
		//only destroy file if not dash or not onDemand, otherwise (regular dash) the file will be needed to append further segments
		if (ctx->dash_mode!=MP4MX_DASH_ON) {
			//only delete file in vod mode
			if (ctx->file) {
				gf_isom_close(ctx->file);
				ctx->file = NULL;
			}
		}

		if (ctx->dst_pck) {
			gf_filter_pck_send(ctx->dst_pck);
			ctx->dst_pck = NULL;
		}
		if (!ctx->flush_size) gf_filter_pid_set_eos(ctx->opid);

		return ctx->flush_size ? GF_OK : GF_EOS;
	}

	return GF_OK;
}

static void mp4_mux_config_timing(GF_MP4MuxCtx *ctx)
{
	u32 i, count = gf_list_count(ctx->tracks);
	//compute min dts of first packet on each track - this assume all tracks are synchronized, might need adjustment for MPEG4 Systems
	u64 first_ts_min = (u64) -1;
	for (i=0; i<count; i++) {
		u64 ts, dts_min;
		GF_FilterPacket *pck;
		TrackWriter *tkw = gf_list_get(ctx->tracks, i);
		if (tkw->fake_track) continue;
		pck = gf_filter_pid_get_packet(tkw->ipid);
		//check this after fetching a packet since it may reconfigure the track
		if (!tkw->track_num) return;

		pck = gf_filter_pid_get_packet(tkw->ipid);
		if (!pck) {
			if (gf_filter_pid_is_eos(tkw->ipid)) {
				if (tkw->cenc_state==1)
					mp4_mux_cenc_update(ctx, tkw, NULL, CENC_CONFIG, 0);

				if (!tkw->nb_samples) {
					const GF_PropertyValue *p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_ISOM_TREX_TEMPLATE);
					if (p) {
						gf_isom_setup_track_fragment_template(ctx->file, tkw->track_id, p->value.data.ptr, p->value.data.size, ctx->nofragdef);
					}
				}
				continue;
			}
			return;
		}
		ts = gf_filter_pck_get_dts(pck);
		if (ts==GF_FILTER_NO_TS)
			ts = gf_filter_pck_get_cts(pck);


		dts_min = ts * 1000000;
		dts_min /= tkw->src_timescale;

		if (first_ts_min > dts_min) {
			first_ts_min = (u64) dts_min;
		}
		tkw->ts_shift = ts;
	}
	if (first_ts_min==(u64)-1)
		first_ts_min = 0;

	//for all packets with dts greater than min dts, we need to add a pause
	for (i=0; i<count; i++) {
		s64 dts_diff;
		TrackWriter *tkw = gf_list_get(ctx->tracks, i);

		//compute offsets
		dts_diff = first_ts_min;
		dts_diff *= tkw->src_timescale;
		dts_diff /= 1000000;
		dts_diff = (s64) tkw->ts_shift - dts_diff;
		//negative could happen due to rounding, ignore them
		if (dts_diff<=0) continue;

		//if dts_diff > 0, we need to delay the track
		if (dts_diff) {
			s64 dur = dts_diff;
			dur *= (u32) ctx->moovts;
			dur /= tkw->src_timescale;

			gf_isom_remove_edit_segments(ctx->file, tkw->track_num);

			gf_isom_set_edit_segment(ctx->file, tkw->track_num, 0, dur, dts_diff, GF_ISOM_EDIT_EMPTY);
			gf_isom_set_edit_segment(ctx->file, tkw->track_num, dur, 0, 0, GF_ISOM_EDIT_NORMAL);
			tkw->empty_init_dur = (u64) dur;
		}
	}

	ctx->config_timing = GF_FALSE;
}

void mp4_mux_format_report(GF_Filter *filter, GF_MP4MuxCtx *ctx, u64 done, u64 total)
{
	Bool status_changed=GF_FALSE;
	u32 total_pc = 0;
	char szStatus[2048], szTK[20];
	if (!gf_filter_reporting_enabled(filter))
		return;
	if (!ctx->update_report)
		return;

	ctx->update_report = GF_FALSE;

	if (ctx->config_timing) {
		sprintf(szStatus, "waiting for clock init");
		status_changed = GF_TRUE;
	} else if (total) {
		if (done>=total) {
			Double ohead = 0;
			if (ctx->total_bytes_in) ohead =  ((Double) (ctx->total_bytes_out - ctx->total_bytes_in)*100 / ctx->total_bytes_in);

			sprintf(szStatus, "done %d samples - bytes "LLU" in "LLU" out - overhead %02.02f%% (%02.02g B/sample)", ctx->total_samples, ctx->total_bytes_in, ctx->total_bytes_out, ohead, ((Double)(ctx->total_bytes_out-ctx->total_bytes_in))/ctx->total_samples);
			status_changed = GF_TRUE;
			total_pc = 10000;

		} else {
			u32 pc = (u32) ((done*10000)/total);
			if (ctx->last_mux_pc == pc + 1) return;
			ctx->last_mux_pc = pc + 1;
			sprintf(szStatus, "mux %d%%", pc);
			status_changed = GF_TRUE;
		}
	} else {
		u32 i, count = gf_list_count(ctx->tracks);
		Bool is_frag = GF_FALSE;

		if (ctx->store>=MP4MX_MODE_FRAG) {
			is_frag = GF_TRUE;
			if (ctx->dash_mode) {
				sprintf(szStatus, "mux segments %d (frags %d) next %02.02g", ctx->nb_segs, ctx->nb_frags_in_seg, ctx->next_frag_start);
			} else {
				sprintf(szStatus, "mux frags %d next %02.02g", ctx->nb_frags, ctx->next_frag_start);
			}
		} else {
			sprintf(szStatus, "%s", (ctx->store==MP4MX_MODE_FLAT) ? "mux" : "import");
		}
		for (i=0; i<count; i++) {
			u32 pc=0;
			TrackWriter *tkw = gf_list_get(ctx->tracks, i);
			if (tkw->aborted) {
				pc=10000;
			} else if (ctx->idur.num) {
				u64 mdur = gf_isom_get_media_duration(ctx->file, tkw->track_num);
				u64 tk_done = mdur * ctx->idur.den;
				u64 tk_total = ((u64)tkw->tk_timescale) * ctx->idur.num;
				pc = (u32) ((tk_done*10000)/tk_total);
			} else {
				if (tkw->nb_frames) {
					pc = (u32) ( (10000 * (u64) tkw->nb_samples) / tkw->nb_frames);
				} else {
					if (tkw->pid_dur.num && tkw->pid_dur.den) {
						pc = (u32) ((tkw->sample.DTS*10000 * tkw->pid_dur.den) / (tkw->pid_dur.num * tkw->tk_timescale));
					} else if (tkw->down_bytes && tkw->down_size) {
						pc = (u32) (((tkw->down_bytes*10000) / tkw->down_size));
					}
				}
			}
			if (pc>10000)
				pc=0;
			if (tkw->last_import_pc != pc + 1) {
				status_changed = GF_TRUE;
				tkw->last_import_pc = pc + 1;
			}
			if (!total_pc || (total_pc > pc))
				total_pc = pc;

			if (is_frag) {
				sprintf(szTK, " TK%d(%c): %d", tkw->track_id, tkw->status_type, tkw->samples_in_frag);
				strcat(szStatus, szTK);
				status_changed = GF_TRUE;
				if (pc) {
					sprintf(szTK, " %d %%", pc);
					strcat(szStatus, szTK);
				}
			} else {
				sprintf(szTK, " %s%d(%c): %d %%", tkw->is_item ? "IT" : "TK", tkw->track_id, tkw->status_type, pc/100);
				strcat(szStatus, szTK);
			}
		}
	}
	if (status_changed) {
		gf_filter_update_status(filter, total_pc, szStatus);
	}
}


GF_Err mp4_mux_process(GF_Filter *filter)
{
	GF_MP4MuxCtx *ctx = gf_filter_get_udta(filter);
	u32 nb_eos, i, count = gf_list_count(ctx->tracks);

	nb_eos = 0;

	if (ctx->config_timing) {
		mp4_mux_config_timing(ctx);
		if (ctx->config_timing) {
			mp4_mux_format_report(filter, ctx, 0, 0);
			return GF_OK;
		}
	}

	//in frag mode, force fetching a sample from each track before processing
	if (ctx->store>=MP4MX_MODE_FRAG) {
		u32 done=0;
		GF_Err e = mp4_mux_process_fragmented(filter, ctx);
		if (e==GF_EOS) done=100;
		mp4_mux_format_report(filter, ctx, done, done);
		return e;
	}

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
		if (tkw->cenc_state==1) mp4_mux_cenc_update(ctx, tkw, pck, CENC_CONFIG, 0);

		if (tkw->is_item) {
			mp4_mux_process_item(ctx, tkw, pck);
		} else {
			mp4_mux_process_sample(ctx, tkw, pck, GF_FALSE);
		}

		gf_filter_pid_drop_packet(tkw->ipid);
		if (tkw->aborted) {
			nb_eos++;
		}
	}
	mp4_mux_format_report(filter, ctx, 0, 0);

	if (count == nb_eos) {
		if (ctx->file) {
			GF_Err e = mp4_mux_done(filter, ctx);
			if (e) return e;
		}
		return GF_EOS;
	}

	return GF_OK;
}

static GF_Err mp4_mux_on_data_patch(void *cbk, u8 *data, u32 block_size, u64 file_offset)
{
	GF_Filter *filter = (GF_Filter *) cbk;
	u8 *output;
	GF_MP4MuxCtx *ctx = gf_filter_get_udta(filter);

	GF_FilterPacket *pck = gf_filter_pck_new_alloc(ctx->opid, block_size, &output);
	memcpy(output, data, block_size);
	gf_filter_pck_set_framing(pck, GF_FALSE, GF_FALSE);
	gf_filter_pck_set_seek_flag(pck, GF_TRUE);
	gf_filter_pck_set_byte_offset(pck, file_offset);
	gf_filter_pck_send(pck);
	return GF_OK;
}

static GF_Err mp4_mux_on_data(void *cbk, u8 *data, u32 block_size)
{
	GF_Filter *filter = (GF_Filter *) cbk;
	u8 *output;
	GF_MP4MuxCtx *ctx = gf_filter_get_udta(filter);

	ctx->total_bytes_out += block_size;

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
		u32 nb_write = (u32) fwrite(data, 1, block_size, ctx->tmp_store);
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
		ctx->current_offset = 0;
		gf_filter_pck_set_property(ctx->dst_pck, GF_PROP_PCK_FILENAME, &PROP_STRING(ctx->seg_name) );
	}
	ctx->first_pck_sent = GF_TRUE;
	ctx->current_size += block_size;
	//non-frag mode, send right away
	if (ctx->store<MP4MX_MODE_FRAG) {
		if (ctx->dst_pck) gf_filter_pck_send(ctx->dst_pck);
		ctx->dst_pck = NULL;
	}
	return GF_OK;
}

void mp4_mux_progress_cbk(void *udta, u64 done, u64 total)
{
	GF_Filter *filter = (GF_Filter *)udta;
	GF_MP4MuxCtx *ctx = gf_filter_get_udta(filter);
	ctx->update_report = GF_TRUE;
	mp4_mux_format_report(filter, ctx, done, total);
}

static GF_Err mp4_mux_initialize(GF_Filter *filter)
{
	GF_MP4MuxCtx *ctx = gf_filter_get_udta(filter);
	gf_filter_sep_max_extra_input_pids(filter, -1);

	if (ctx->file) {
		if (gf_isom_get_mode(ctx->file) < GF_ISOM_OPEN_WRITE) return GF_BAD_PARAM;
		if (ctx->store>=MP4MX_MODE_FRAG) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Cannot use fragmented output on already opened ISOBMF file\n"));
			return GF_BAD_PARAM;
		}
		ctx->owns_mov = GF_FALSE;
	} else {
		u32 open_mode = GF_ISOM_OPEN_WRITE;
		ctx->owns_mov = GF_TRUE;

		if (ctx->sseg) {
			ctx->noinit = GF_TRUE;
			ctx->cdur = 1000000000;
			ctx->store = MP4MX_MODE_FRAG;
		}

		switch (ctx->store) {
		case MP4MX_MODE_INTER:
		case MP4MX_MODE_TIGHT:
			open_mode = GF_ISOM_WRITE_EDIT;
			break;
		}
		ctx->file = gf_isom_open("_gpac_isobmff_redirect", open_mode, ctx->tmpd);
		if (!ctx->file) return GF_OUT_OF_MEM;

		gf_isom_set_write_callback(ctx->file, mp4_mux_on_data, mp4_mux_on_data_patch, filter, ctx->block_size);

		gf_isom_set_progress_callback(ctx->file, mp4_mux_progress_cbk, filter);

		if (ctx->dref && (ctx->store>=MP4MX_MODE_FRAG)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MP4Mux] Cannot use data reference in movie fragments, not supported. Ignoring it\n"));
			ctx->dref = GF_FALSE;
		}

	}
	if (!ctx->moovts) ctx->moovts=600;

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
	s64 moffset;

	if (ctx->ctmode > MP4MX_CT_EDIT) return;

	//if we have a complex edit list (due to track template), don't override
	if (gf_isom_get_edit_list_type(ctx->file, tkw->track_num, &moffset)) return;

	gf_isom_remove_edit_segments(ctx->file, tkw->track_num);


	count = gf_isom_get_sample_count(ctx->file, tkw->track_num);
	max_cts = 0;
	min_cts = (u64) -1;
	for (i=0; i<count; i++) {
		GF_ISOSample *s = gf_isom_get_sample_info(ctx->file, tkw->track_num, i+1, &di, &doff);
		if (!s) return;

		if (s->DTS + s->CTS_Offset > max_cts)
			max_cts = s->DTS + s->CTS_Offset;

		if (min_cts > s->DTS + s->CTS_Offset)
			min_cts = s->DTS + s->CTS_Offset;

		gf_isom_sample_del(&s);
	}

	if (min_cts || tkw->empty_init_dur) {
		max_cts -= min_cts;
		max_cts += gf_isom_get_sample_duration(ctx->file, tkw->track_num, count);

		max_cts *= ctx->moovts;
		max_cts /= tkw->tk_timescale;
		if (tkw->empty_init_dur) {

			gf_isom_set_edit_segment(ctx->file, tkw->track_num, 0, tkw->empty_init_dur, 0, GF_ISOM_EDIT_EMPTY);
			if (max_cts >= tkw->empty_init_dur) max_cts -= tkw->empty_init_dur;
			else max_cts = 0;
		}

		gf_isom_set_edit_segment(ctx->file, tkw->track_num, tkw->empty_init_dur, max_cts, min_cts, GF_ISOM_EDIT_NORMAL);
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

		if (hevc_base_track) {
			ref_track_id = gf_isom_get_track_id(ctx->file, hevc_base_track);
			gf_isom_set_track_reference(ctx->file, tkw->track_num, GF_ISOM_REF_OREF, ref_track_id);
		}
	}
}

static void mp4_mux_set_hevc_groups(GF_MP4MuxCtx *ctx, TrackWriter *tkw)
{
	GF_PropertyEntry *pe=NULL;
	const GF_PropertyValue *p = gf_filter_pid_get_info_str(tkw->ipid, "hevc:oinf", &pe);
	if (p) {
		u32 gi=0;
		gf_isom_add_sample_group_info(ctx->file, tkw->track_num, GF_ISOM_SAMPLE_GROUP_OINF, p->value.data.ptr, p->value.data.size, GF_TRUE, &gi);

		p = gf_filter_pid_get_info_str(tkw->ipid, "hevc:linf", &pe);
		if (p) {
			gf_isom_add_sample_group_info(ctx->file, tkw->track_num, GF_ISOM_SAMPLE_GROUP_LINF, p->value.data.ptr, p->value.data.size, GF_TRUE, &gi);
			gf_isom_set_track_group(ctx->file, tkw->track_num, 1000+gf_isom_get_track_id(ctx->file, tkw->track_num), GF_ISOM_BOX_TYPE_CSTG, GF_TRUE);
		}
	}

	gf_filter_release_property(pe);

	p = gf_filter_pid_get_property_str(tkw->ipid, "hevc:min_lid");
	if (p && p->value.uint) {
		mp4_mux_set_lhvc_base_layer(ctx, tkw);
	} else if (tkw->codecid==GF_CODECID_LHVC) {
		mp4_mux_set_lhvc_base_layer(ctx, tkw);
	}
}

static GF_Err mp4_mux_done(GF_Filter *filter, GF_MP4MuxCtx *ctx)
{
	GF_Err e = GF_OK;
	u32 i, count;
	GF_PropertyEntry *pe=NULL;
	const GF_PropertyValue *p;

	count = gf_list_count(ctx->tracks);
	for (i=0; i<count; i++) {
		Bool has_bframes = GF_FALSE;
		TrackWriter *tkw = gf_list_get(ctx->tracks, i);

		if (tkw->min_neg_ctts<0) {
			//use ctts v1 negative offsets
			if (ctx->ctmode==MP4MX_CT_NEGCTTS) {
				gf_isom_set_ctts_v1(ctx->file, tkw->track_num, (u32) -tkw->min_neg_ctts);
			}
			//ctts v0
			else {
				gf_isom_set_cts_packing(ctx->file, tkw->track_num, GF_TRUE);
				gf_isom_shift_cts_offset(ctx->file, tkw->track_num, (s32) tkw->min_neg_ctts);
				gf_isom_set_cts_packing(ctx->file, tkw->track_num, GF_FALSE);
				gf_isom_set_composition_offset_mode(ctx->file, tkw->track_num, GF_FALSE);

				mp4_mux_update_edit_list_for_bframes(ctx, tkw);
			}
			has_bframes = GF_TRUE;
		} else if (tkw->has_ctts && (tkw->stream_type==GF_STREAM_VISUAL)) {
			mp4_mux_update_edit_list_for_bframes(ctx, tkw);

			has_bframes = GF_TRUE;
		} else if (tkw->ts_delay) {
			gf_isom_update_edit_list_duration(ctx->file, tkw->track_num);
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
				if (!ctx->make_qt)
					gf_isom_set_pl_indication(ctx->file, GF_ISOM_PL_VISUAL, PL);
			}
		}


		if (tkw->has_append)
			gf_isom_refresh_size_info(ctx->file, tkw->track_num);

		if ((tkw->nb_samples == 1) && ctx->idur.num && ctx->idur.den) {
			u32 dur = tkw->tk_timescale * ctx->idur.num;
			dur /= ctx->idur.den;
			gf_isom_set_last_sample_duration(ctx->file, tkw->track_num, dur);
		}

		if (tkw->has_open_gop) {
			if (ctx->importer) {
				GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("OpenGOP detected - adjusting file brand\n"));
			}
			gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_ISO6, 1);
		}

		mp4_mux_set_hevc_groups(ctx, tkw);

		p = gf_filter_pid_get_info_str(tkw->ipid, "ttxt:rem_last", &pe);
		if (p && p->value.boolean)
			gf_isom_remove_sample(ctx->file, tkw->track_num, tkw->nb_samples);

		p = gf_filter_pid_get_info_str(tkw->ipid, "ttxt:last_dur", &pe);
		if (p)
			gf_isom_set_last_sample_duration(ctx->file, tkw->track_num, p->value.uint);

		if (tkw->is_nalu && ctx->pack_nal && (gf_isom_get_mode(ctx->file)!=GF_ISOM_OPEN_WRITE)) {
			u32 msize = 0;
			Bool do_rewrite = GF_FALSE;
			u32 i, count = gf_isom_get_sample_description_count(ctx->file, tkw->track_num);
			const GF_PropertyValue *p = gf_filter_pid_get_info(tkw->ipid, GF_PROP_PID_MAX_NALU_SIZE, &pe);
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

		//don't update bitrate info for single sample tracks, unless MPEG-4 Systems - compatibility with old arch
		if (ctx->btrt && !tkw->skip_bitrate_update && ((tkw->nb_samples>1) || ctx->m4sys) )
			gf_media_update_bitrate(ctx->file, tkw->track_num);
	}

	gf_filter_release_property(pe);

	if (ctx->owns_mov) {
		switch (ctx->store) {
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
				GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[MP4Mux] Failed to write file: %s\n", gf_error_to_string(e) ));
			}
		}
		ctx->file = NULL;
		gf_filter_pid_set_eos(ctx->opid);
	} else {
		ctx->file = NULL;
	}
	return e;
}

static void mp4_mux_finalize(GF_Filter *filter)
{
	GF_MP4MuxCtx *ctx = gf_filter_get_udta(filter);
	assert(!ctx->dst_pck);

	if (ctx->owns_mov && (ctx->file || (ctx->store>=MP4MX_MODE_FRAG))) {
		gf_isom_delete(ctx->file);
	}

	while (gf_list_count(ctx->tracks)) {
		TrackWriter *tkw = gf_list_pop_back(ctx->tracks);
		mp4_mux_track_writer_del(tkw);
	}
	gf_list_del(ctx->tracks);
	if (ctx->bs_r) gf_bs_del(ctx->bs_r);
	if (ctx->seg_name) gf_free(ctx->seg_name);
	if (ctx->tmp_store) gf_fclose(ctx->tmp_store);
	if (ctx->seg_sizes) gf_free(ctx->seg_sizes);
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
	CAP_STRING(GF_CAPS_OUTPUT_STATIC,  GF_PROP_PID_FILE_EXT, "mp4|mpg4|m4a|m4i|m4s|3gp|3gpp|3g2|3gp2|iso|m4s|heif|heic|avci|ismf|mj2|mov|qt"),
	{0},
	//for scene / OD / text, we don't want raw codecid (filters modifying a scene graph we don't expose)
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_SCENE),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_OD),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_RAW),
};


#define OFFS(_n)	#_n, offsetof(GF_MP4MuxCtx, _n)
static const GF_FilterArgs MP4MuxArgs[] =
{
	{ OFFS(m4sys), "force MPEG-4 Systems signaling of tracks", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(dref), "only references data from source file - not compatible with all media sources", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(ctmode), "set composition offset mode for video tracks.\n"
	"- edit: uses edit lists to shift first frame to presentation time 0\n"
	"- noedit: ignore edit lists and does not shift timeline\n"
	"- negctts: uses ctts v1 with possibly negative offsets and no edit lists", GF_PROP_UINT, "edit", "edit|noedit|negctts", GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(idur), "only import the specified duration", GF_PROP_FRACTION, "0", NULL, 0},
	{ OFFS(pack3gp), "pack a given number of 3GPP audio frames in one sample", GF_PROP_UINT, "1", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(importer), "compatibility with old importer, displays import progress", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(pack_nal), "repack NALU size length to minimum possible size for AVC/HEVC/...", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(xps_inband), "use inband (in sample data) param set for AVC/HEVC/...\n"
	"- no: paramater sets are not inband, several sample descriptions might be created\n"
	"- all: paramater sets are inband, no param sets in sample description\n"
	"- both: paramater sets are inband, signaled as inband, and also first set is kept in sample descripton\n"
	"- mix: creates non-standard files using single sample entry with first PSs found, and moves other PS inband", GF_PROP_UINT, "no", "no|all|both|mix|", 0},
	{ OFFS(store), "file storage mode\n"
	"- inter: uses cdur to interleave the file\n"
	"- flat: writes a flat file, moov at end\n"
	"- cap: flushes to disk as soon as samples are added\n"
	"- tight:  uses per-sample interleaving of all tracks\n"
	"- frag: fragments the file using cdur duration\n"
	"- sfrag: framents the file using cdur duration but adjusting to start with SAP1/3", GF_PROP_UINT, "inter", "inter|flat|cap|tight|frag|sfrag", 0},
	{ OFFS(cdur), "chunk duration for interleaving and fragmentation modes\n"
	"- 0: no specific interleaving but moov first\n"
	"- negative: defaults to 1.0 unless overriden by storage profile", GF_PROP_DOUBLE, "-1.0", NULL, 0},
	{ OFFS(moovts), "timescale to use for movie. A negative value picks the media timescale of the first track added", GF_PROP_SINT, "600", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(moof_first), "generate fragments starting with moof then mdat", GF_PROP_BOOL, "true", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(abs_offset), "use absolute file offset in fragments rather than offsets from moof", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(fsap), "split truns in video fragments at SAPs to reduce file size", GF_PROP_BOOL, "true", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(file), "pointer to a write/edit ISOBMF file used internally by importers and exporters", GF_PROP_POINTER, NULL, NULL, GF_FS_ARG_HINT_HIDE},
	{ OFFS(subs_sidx), "number of subsegments per sidx. negative value disables sidx, -2 removes sidx if present in source pid", GF_PROP_SINT, "-1", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(m4cc), "4 character code of empty box to appen at the end of a segment", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(chain_sidx), "use daisy-chaining of SIDX", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(msn), "sequence number of first moof to N", GF_PROP_UINT, "1", NULL, 0},
	{ OFFS(msninc), "sequence number increase between moofs", GF_PROP_UINT, "1", NULL, 0},
	{ OFFS(tfdt), "set TFDT of first traf", GF_PROP_FRACTION64, "0", NULL, 0},
	{ OFFS(tfdt_traf), "set TFDT in each traf", GF_PROP_BOOL, "false", NULL, 0},
	{ OFFS(nofragdef), "disable default flags in fragments", GF_PROP_BOOL, "false", NULL, 0},
	{ OFFS(straf), "use a single traf per moov (smooth streaming and co)", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(strun), "use a single traf per moov (smooth streaming and co)", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(psshs), "set PSSH boxes store mode\n"
	"- moof: in first moof of each segments\n"
	"- moov: in movie box\n"
	"- none: pssh is discarded", GF_PROP_UINT, "moov", "moov|moof|none", GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(sgpd_traf), "store sample group descriptions in traf (duplicated for each traf). If not used, sample group descriptions are stored in the movie box", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(cache), "enable temp storage for VoD dash modes - see filter help", GF_PROP_BOOL, "false", NULL, 0},
	{ OFFS(noinit), "do not produce initial moov, used for DASH bitstream switching mode", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(tktpl), "use track box from input if any as a template to create new track\n"
	"- no: disables template\n"
	"- yes: clones the track (except edits and decoder config)\n"
	"- udta: only loads udta", GF_PROP_UINT, "yes", "no|yes|udta", GF_FS_ARG_HINT_EXPERT},
	{ OFFS(mudta), "use udta and other moov extension boxes from input if any\n"
	"- no: disables import\n"
	"- yes: clones all extension boxes\n"
	"- udta: only loads udta", GF_PROP_UINT, "yes", "no|yes|udta", GF_FS_ARG_HINT_EXPERT},
	{ OFFS(tmpd), "set temp directory for intermediate file(s)", GF_PROP_STRING, NULL, NULL, 0},
	{ OFFS(mvex), "set mvex after tracks", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(tkid), "track ID of created track for single track. Default 0 uses next available trackID", GF_PROP_UINT, "0", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(fdur), "fragment based on fragment duration rather than CTS. Mostly used for MP4Box -frag option", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(btrt), "set btrt box in sample description", GF_PROP_BOOL, "true", NULL, 0},
	{ OFFS(styp), "set segment styp major brand to the given 4CC[.version]", GF_PROP_STRING, NULL, NULL, 0},
	{ OFFS(mediats), "set media timescale. A value of 0 means inherit from pid, a value of -1 means derive from samplerate or frame rate", GF_PROP_SINT, "0", NULL, 0},
	{ OFFS(ase), "set audio sample entry mode for more than stereo layouts\n"\
			"- v0: use v0 signaling but channel count from stream, recommended for backward compatibility\n"\
			"- v0s: use v0 signaling and force channel count to 2 (stereo) if more than 2 channels\n"\
			"- v1: use v1 signaling, ISOBMFF style\n"\
			"- v1qt: use v1 signaling, QTFF style"\
		, GF_PROP_UINT, "v0", "|v0|v0s|v1|v1qt", 0},
	{ OFFS(ssix), "create ssix when sidx is present, level 1 mappping I-frames byte ranges, level 0xFF mapping the rest", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(ccst), "insert coding constraint box for video tracks", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(maxchunk), "set max chunk size in bytes for runs (only used in non-fragmented mode). 0 means no constraints", GF_PROP_UINT, "0", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(noroll), "disable roll sample grouping", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(saio32), "set single segment mode for dash", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(sseg), "set single segment mode for dash", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_HIDE},

	{ OFFS(block_size), "target output block size, 0 for default internal value (10k)", GF_PROP_UINT, "10000", NULL, GF_FS_ARG_HINT_ADVANCED},
	{0}
};


GF_FilterRegister MP4MuxRegister = {
	.name = "mp4mx",
	GF_FS_SET_DESCRIPTION("ISOBMFF/QT muxer")
	GF_FS_SET_HELP("Muxes file according to ISOBMFF (14496-12 and derived specifications) or QuickTime\n"
	"\n"
	"By default all input PIDs with ItemID property set are muxed as items, otherwise they are muxed as tracks.\n"
	"To prevent source items to be muxed as items, use [-itemid](mp4dmx) option from ISOBMF demuxer.\n"
	"EX -i source.mp4:itemid=false -o file.mp4\n"
	"  \n"
	"To force non-item streams to be muxed as items, use __#ItemID__ option on that PID:\n"
	"EX -i source.jpg:#ItemID=1 -o file.mp4\n"
	"  \n"
	"The `cache` mode allows controling how DASH onDemand segments are generated:\n"
	"-  When disabled, SIDX size will be estimated based on duration and DASH segment length, and padding will be used in the file __before__ the final SIDX.\n"
	"- When enabled, file data is stored to a temporary file on disk and flushed upon completion, no padding is present.\n")
	.private_size = sizeof(GF_MP4MuxCtx),
	.args = MP4MuxArgs,
	.initialize = mp4_mux_initialize,
	.finalize = mp4_mux_finalize,
	SETCAPS(MP4MuxCaps),
	.configure_pid = mp4_mux_configure_pid,
	.process = mp4_mux_process,
	.process_event = mp4_mux_process_event
};


const GF_FilterRegister *mp4_mux_register(GF_FilterSession *session)
{
	return &MP4MuxRegister;
}
