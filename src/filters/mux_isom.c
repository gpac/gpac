/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2020
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

#ifndef GPAC_DISABLE_ISOM_WRITE

#define TEXT_DEFAULT_WIDTH	400
#define TEXT_DEFAULT_HEIGHT	60
#define TEXT_DEFAULT_FONT_SIZE	18

#define GF_VENDOR_GPAC		GF_4CC('G','P','A','C')


#define ISOM_FILE_EXT "mp4|mpg4|m4a|m4i|3gp|3gpp|3g2|3gp2|iso|ismv|m4s|heif|heic|avci|mj2|mov|qt"
#define ISOM_FILE_MIME "application/x-isomedia|application/mp4|video/mp4|audio/mp4|video/3gpp|audio/3gpp|video/3gp2|audio/3gp2|video/iso.segment|audio/iso.segment|image/heif|image/heic|image/avci|video/quicktime"

enum{
	NALU_NONE,
	NALU_AVC,
	NALU_HEVC,
	NALU_VVC
};


enum
{
	CENC_NONE=0,
	CENC_NEED_SETUP,
	CENC_SETUP_DONE,
	CENC_SETUP_ERROR
};

enum{
	TAG_NONE,
	TAG_STRICT,
	TAG_ALL
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
	u32 clear_stsd_idx;

	Bool use_dref;
	Bool aborted;
	Bool suspended;
	Bool has_append;
	Bool has_ctts;
	s64 min_neg_ctts;
	u32 nb_samples, samples_in_stsd;
	u32 nb_frames_per_sample;
	u64 ts_shift;

	Bool skip_bitrate_update;
	Bool has_open_gop;
	GF_FilterSAPType gdr_type;

	Bool next_is_first_sample;

	u32 media_profile_level;

	Bool import_msg_header_done;

	u32 nal_unit_size;

	GF_AVCConfig *avcc, *svcc;
	GF_HEVCConfig *hvcc, *lvcc;
	GF_VVCConfig *vvcc;

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
	u32 scheme_type;

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
	u32 item_id;
	char status_type;
	u32 last_import_pc;

	u32 nb_frames;
	u64 down_bytes, down_size;
	GF_Fraction64 pid_dur;
	//for import message
	u64 prog_done, prog_total;

	u32 prev_tid_group;

	Bool box_patched;
} TrackWriter;

enum
{
	MP4MX_MODE_INTER=0,
	MP4MX_MODE_FLAT,
	MP4MX_MODE_FASTSTART,
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

enum
{
	MP4MX_VODCACHE_ON=0,
	MP4MX_VODCACHE_INSERT,
	MP4MX_VODCACHE_REPLACE,
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
	Bool nofragdef, straf, strun, sgpd_traf, noinit;
	u32 vodcache;
	u32 psshs;
	u32 trackid;
	Bool fragdur;
	Bool btrt;
	Bool ssix;
	Bool ccst;
	s32 mediats;
	GF_AudioSampleEntryImportMode ase;
	char *styp;
	Bool sseg;
	Bool noroll;
	Bool saio32;
	u32 compress;
	Bool trun_inter;
	char *boxpatch;
	Bool fcomp;
	Bool deps;
	Bool mvex;
	u32 sdtp_traf;
#ifdef GF_ENABLE_CTRN
	Bool ctrn;
	Bool ctrni;
#endif
	Bool mfra;
	Bool forcesync;
	u32 tags;

	//internal
	Bool owns_mov;
	GF_FilterPid *opid;
	Bool first_pck_sent;

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
	Bool sidx_size_exact;

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
	u32 make_qt;
	TrackWriter *prores_track;

	GF_SegmentIndexBox *cloned_sidx;
	u32 cloned_sidx_index;
	Double faststart_ts_regulate;

	Bool is_rewind;
	Bool box_patched;
	u32 cur_file_idx_plus_one;
	char *cur_file_suffix;
	Bool notify_filename;

	u32 next_file_idx;
	const char *next_file_suffix;
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
		if (p && p->value.lfrac.den) {
			Double mdur = (Double) p->value.lfrac.num;
			mdur /= p->value.lfrac.den;
			if (ctx->media_dur < mdur) ctx->media_dur = mdur;
		}
	}
	ctx->dash_mode = MP4MX_DASH_VOD;
	if ((ctx->vodcache==MP4MX_VODCACHE_ON) && !ctx->tmp_store) {
		ctx->tmp_store = gf_file_temp(NULL);
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
		GF_NALUFFParam *sl = gf_list_get(list, i);
		gf_bs_write_int(bs, sl->size, 8*nalu_size_length);
		gf_bs_write_data(bs, sl->data, sl->size);
	}
}

static GF_List *mp4_mux_get_nalus_ps(GF_List *list, u8 type)
{
	u32 i, count = gf_list_count(list);
	for (i=0; i<count; i++) {
		GF_NALUFFParamArray *pa = gf_list_get(list, i);
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
			/*if (!tkw->nal_unit_size) */tkw->nal_unit_size = tkw->avcc->nal_unit_size;
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
			mp4_mux_write_ps_list(bs, mp4_mux_get_nalus_ps(tkw->hvcc->param_array, GF_HEVC_NALU_VID_PARAM), tkw->hvcc->nal_unit_size);
			if (!tkw->nal_unit_size) tkw->nal_unit_size = tkw->hvcc->nal_unit_size;
		}
		if (tkw->lvcc) {
			mp4_mux_write_ps_list(bs, mp4_mux_get_nalus_ps(tkw->lvcc->param_array, GF_HEVC_NALU_VID_PARAM), tkw->lvcc->nal_unit_size);
			if (!tkw->nal_unit_size) tkw->nal_unit_size = tkw->lvcc->nal_unit_size;
		}
		if (tkw->hvcc)
			mp4_mux_write_ps_list(bs, mp4_mux_get_nalus_ps(tkw->hvcc->param_array, GF_HEVC_NALU_SEQ_PARAM), tkw->hvcc->nal_unit_size);
		if (tkw->lvcc)
			mp4_mux_write_ps_list(bs, mp4_mux_get_nalus_ps(tkw->lvcc->param_array, GF_HEVC_NALU_SEQ_PARAM), tkw->lvcc->nal_unit_size);
		if (tkw->hvcc)
			mp4_mux_write_ps_list(bs, mp4_mux_get_nalus_ps(tkw->hvcc->param_array, GF_HEVC_NALU_PIC_PARAM), tkw->hvcc->nal_unit_size);
		if (tkw->lvcc)
			mp4_mux_write_ps_list(bs, mp4_mux_get_nalus_ps(tkw->lvcc->param_array, GF_HEVC_NALU_PIC_PARAM), tkw->lvcc->nal_unit_size);
	}

	if (tkw->vvcc) {
		mp4_mux_write_ps_list(bs, mp4_mux_get_nalus_ps(tkw->vvcc->param_array, GF_VVC_NALU_VID_PARAM), tkw->vvcc->nal_unit_size);
		if (!tkw->nal_unit_size) tkw->nal_unit_size = tkw->vvcc->nal_unit_size;
		mp4_mux_write_ps_list(bs, mp4_mux_get_nalus_ps(tkw->vvcc->param_array, GF_VVC_NALU_SEQ_PARAM), tkw->vvcc->nal_unit_size);
		mp4_mux_write_ps_list(bs, mp4_mux_get_nalus_ps(tkw->vvcc->param_array, GF_VVC_NALU_PIC_PARAM), tkw->vvcc->nal_unit_size);
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
	if (tkw->vvcc) gf_odf_vvc_cfg_del(tkw->vvcc);
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

static void mp4mux_track_reorder(void *udta, u32 old_track_num, u32 new_track_num)
{
	GF_MP4MuxCtx *ctx = (GF_MP4MuxCtx *) udta;
	u32 i, count;
	count = gf_list_count(ctx->tracks);
	for (i=0; i<count; i++) {
		TrackWriter *tkw = gf_list_get(ctx->tracks, i);
		if (tkw->track_num==old_track_num) {
			tkw->track_num = new_track_num;
			return;
		}
	}
}

static void mp4mux_reorder_tracks(GF_MP4MuxCtx *ctx)
{
	u32 i, count, prev_num, prev_pos;
	GF_List *new_tracks = gf_list_new();
	prev_num = prev_pos = 0;
	count = gf_list_count(ctx->tracks);
	for (i=0; i<count; i++) {
		TrackWriter *tkw = gf_list_get(ctx->tracks, i);
		if (tkw->track_num<prev_num) {
			gf_list_insert(new_tracks, tkw, prev_pos);
		} else {
			gf_list_add(new_tracks, tkw);
		}
		prev_pos = gf_list_count(new_tracks) - 1;
		prev_num = tkw->track_num;
	}
	if (gf_list_count(new_tracks)!=count) {
		gf_list_del(new_tracks);
		return;
	}
	gf_list_del(ctx->tracks);
	ctx->tracks = new_tracks;
}

static void mp4_mux_set_tags(GF_MP4MuxCtx *ctx, TrackWriter *tkw)
{
	u32 idx=0;
	if (ctx->tags==TAG_NONE) return;

	while (1) {
		GF_Err e;
		u32 tag_val;
		u32 prop_4cc=0;
		s32 itag;
		const char *tag_name=NULL;
		const GF_PropertyValue *tag = gf_filter_pid_enum_properties(tkw->ipid, &idx, &prop_4cc, &tag_name);
		if (!tag) break;

		if (prop_4cc==GF_PROP_PID_COVER_ART) {
			e = gf_isom_apple_set_tag(ctx->file, GF_ISOM_ITUNE_COVER_ART, tag->value.data.ptr, tag->value.data.size);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Failed to set cover art: %s\n", gf_error_to_string(e)));
			}
		}
		if (!tag_name)
			continue;

		itag = gf_itags_find_by_name(tag_name);
		if (itag>=0) {
			Bool tag_set=GF_FALSE;
			u32 val;
			char _t[8];
			u32 itype = gf_itags_get_type((u32) itag);
			itag = gf_itags_get_itag(itag);

			e = GF_OK;
			if ((itag==GF_ISOM_ITUNE_DISK) ||(itag==GF_ISOM_ITUNE_TRACKNUMBER)) {
				u32 n=0, t=0, tlen=0;
				if (tag->value.string) {
					memset(_t, 0, sizeof(char) * 8);
					tlen = (itag == GF_ISOM_ITUNE_DISK) ? 6 : 8;
					if (sscanf(tag->value.string, "%u/%u", &n, &t) == 2) {
						_t[3] = n;
						_t[2] = n >> 8;
						_t[5] = t;
						_t[4] = t >> 8;
					}
					else if (sscanf(tag->value.string, "%u", &n) == 1) {
						_t[3] = n;
						_t[2] = n >> 8;
					}
					else tlen = 0;
				}
				if (!tag->value.string || tlen) {
					e = gf_isom_apple_set_tag(ctx->file, itag, tlen ? (u8 *)_t : NULL, tlen);
					tag_set = GF_TRUE;
				}
			}
			else if (itag==GF_ISOM_ITUNE_GENRE) {
				if ((tag->type==GF_PROP_STRING) && tag->value.string) {
					val = gf_id3_get_genre_tag(tag->value.string);
					if (val) {
						e = gf_isom_apple_set_tag(ctx->file, itag, NULL, val);
						tag_set = GF_TRUE;
					}
				}
			}

			if (!tag_set) {
				switch (itype) {
				case GF_ITAG_STR:
				case GF_ITAG_SUBSTR:
					if ((tag->type==GF_PROP_STRING) && tag->value.string) {
						e = gf_isom_apple_set_tag(ctx->file, itag, tag->value.string, (u32) strlen(tag->value.string));
					} else {
						e = GF_BAD_PARAM;
					}
					break;
				case GF_ITAG_INT:
					val=0;
					if ((tag->type==GF_PROP_STRING) && tag->value.string) {
						val = atoi(tag->value.string);
					} else if (tag->type==GF_PROP_UINT) {
						val = tag->value.uint;
					}
					if (val) {
						e = gf_isom_apple_set_tag(ctx->file, itag, NULL, val);
					}
					break;

				case GF_ITAG_BOOL:
					_t[0] = 0;
					if ((tag->type==GF_PROP_STRING) && tag->value.string &&
						(!stricmp(tag->value.string, "yes") || !stricmp(tag->value.string, "true") || !stricmp(tag->value.string, "1"))
					) {
						_t[0] = 1;
					} else if (tag->value.boolean) {
						_t[0] = 1;
					}
					e = gf_isom_apple_set_tag(ctx->file, itag, _t, 1);
				}
			}
			
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Failed to set tag %s: %s\n", tag_name, gf_error_to_string(e)));
			}
			continue;
		}
		if (ctx->tags==TAG_STRICT)
			continue;

		if (strnicmp(tag_name, "tag_", 4))
			continue;

		tag_name += 4;

		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MP4Mux] Unrecognized tag %s: %s\n", tag_name, tag->value.string));
		if ((tag->type!=GF_PROP_STRING) && (tag->type!=GF_PROP_DATA) && (tag->type!=GF_PROP_CONST_DATA)) continue;

		if (strlen(tag_name)==4) {
			tag_val = GF_4CC(tag_name[0], tag_name[1], tag_name[2], tag_name[3]);
		} else {
			tag_val = gf_crc_32(tag_name, (u32) strlen(tag_name));
			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[MP4Mux] Tag name %s is not a 4CC, using CRC32 %d as value\n", tag_name, tag_val));
		}
		if (tag->type==GF_PROP_STRING) {
			if (tag->value.string) {
				if (tag_val==GF_ID3V2_FRAME_TCOP) {
					e = gf_isom_set_copyright(ctx->file, "unk", tag->value.string);
				} else {
					e = gf_isom_apple_set_tag(ctx->file, tag_val, tag->value.string, (u32) strlen(tag->value.string) );
				}
			} else {
				e = gf_isom_apple_set_tag(ctx->file, tag_val, NULL, 0);
			}
		} else {
			e = gf_isom_apple_set_tag(ctx->file, tag_val, tag->value.data.ptr , tag->value.data.size);
		}
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Failed to set tag %s: %s\n", tag_name, gf_error_to_string(e)));
		}
	}
}


static GF_Err mp4_mux_setup_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_true_pid)
{
	void mux_assign_mime_file_ext(GF_FilterPid *ipid, GF_FilterPid *opid, const char *file_exts, const char *mime_types, const char *def_ext);
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
	Bool use_vvc = GF_FALSE;
	Bool use_hvt1 = GF_FALSE;
	Bool use_av1 = GF_FALSE;
	Bool use_vpX = GF_FALSE;
	Bool use_mj2 = GF_FALSE;
	Bool use_opus = GF_FALSE;
	Bool use_dref = GF_FALSE;
	Bool skip_dsi = GF_FALSE;
	Bool is_text_subs = GF_FALSE;
	Bool force_colr = GF_FALSE;
	u32 m_subtype=0;
	u32 m_subtype_src=0;
	u32 m_subtype_alt_raw=0;
	u32 raw_bitdepth=0;
	u32 override_stype=0;
	u32 width, height, sr, nb_chan, nb_bps, z_order, txt_fsize;
	u64 ch_layout;
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
				ctx->make_qt = 1;
			}
			gf_free(dst);
		}
	}
	//copy properties at init or reconfig
	if (ctx->opid && is_true_pid) {
		gf_filter_pid_copy_properties(ctx->opid, pid);
		if (gf_list_count(ctx->tracks)>1)
			gf_filter_pid_set_name(ctx->opid, "isobmf_mux");

		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, NULL);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT, NULL);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, NULL);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, NULL);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_FILE) );

		mux_assign_mime_file_ext(pid, ctx->opid, ISOM_FILE_EXT, ISOM_FILE_MIME, NULL);
		
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DASH_MODE, NULL);

		switch (ctx->store) {
		case MP4MX_MODE_FLAT:
		case MP4MX_MODE_FASTSTART:
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DISABLE_PROGRESSIVE, &PROP_UINT(GF_PID_FILE_PATCH_INSERT) );
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
			u32 j, count2;
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
		if (!tkw) return GF_OUT_OF_MEM;
		
		gf_list_add(ctx->tracks, tkw);
		tkw->ipid = pid;
		tkw->fake_track = !is_true_pid;
		if (is_true_pid) {
			gf_filter_pid_set_udta(pid, tkw);

#ifdef GPAC_ENABLE_COVERAGE
			if (gf_sys_is_cov_mode()) {
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
	case GF_CODECID_AP4X:
	case GF_CODECID_AP4H:
		if (!ctx->make_qt) {
			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[MP4Mux] ProRes track detected, muxing to QTFF even though ISOBMFF was asked\n"));
			ctx->make_qt = 2;
		}
		if (ctx->prores_track && (ctx->prores_track != tkw)) {
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
					ctx->fragdur = GF_FALSE;
			}
		} else if (ctx->dash_mode)
			ctx->fragdur = GF_TRUE;
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
		u32 tk_idx=0;
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
				else if (inc * 2997 == ts * 100) target_timescale = 30000;
				else if (inc * 3000 == ts * 100) target_timescale = 3000;
				else if (inc * 5000 == ts * 100) target_timescale = 5000;
				else if (inc * 60000 == ts * 1001) target_timescale = 60000;
				else if (inc * 5994 == ts * 100) target_timescale = 60000;
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
				p = gf_filter_pid_get_property(pid, GF_PROP_PID_ISOM_MOVIE_TIME);
				if (p && p->value.lfrac.den) {
					gf_isom_set_timescale(ctx->file, (u32) p->value.lfrac.den);
					ctx->moovts = (u32) p->value.lfrac.den;
				} else {
					gf_isom_set_timescale(ctx->file, ctx->moovts);
				}
			}
			if (ctx->store==MP4MX_MODE_FASTSTART) {
				gf_isom_make_interleave(ctx->file, ctx->cdur);
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

		if (ctx->trackid) tkid = ctx->trackid;

		if (tkw->stream_type == GF_STREAM_ENCRYPTED) {
			tkw->is_encrypted = GF_TRUE;
			tkw->stream_type = gf_codecid_type(tkw->codecid);
		}
		mtype = gf_isom_stream_type_to_media_type(tkw->stream_type, tkw->codecid);

		if (ctx->moovts<0) {
			ctx->moovts = tkw->tk_timescale;
			gf_isom_set_timescale(ctx->file, (u32) ctx->moovts);
		}

		p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_MUX_INDEX);
		if (p) {
			tk_idx = p->value.uint;
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
			if (!tkid) tkid = tk_idx;

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
			gf_isom_set_track_enabled(ctx->file, tkw->track_num, GF_TRUE);
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

		p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_SRC_MAGIC);
		if (p) {
			gf_isom_set_track_magic(ctx->file, tkw->track_num, p->value.longuint);
		}
		if (tk_idx) {
			gf_isom_set_track_index(ctx->file, tkw->track_num, tk_idx, mp4mux_track_reorder, ctx);
			mp4mux_reorder_tracks(ctx);
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

		if (ctx->noroll) {
			gf_isom_remove_sample_group(ctx->file, tkw->track_num, GF_ISOM_SAMPLE_GROUP_ROLL);
		}


		if (ctx->dash_mode==MP4MX_DASH_VOD) {
			Bool use_cache = (ctx->vodcache == MP4MX_VODCACHE_ON) ? GF_TRUE : GF_FALSE;
			if ((ctx->vodcache == MP4MX_VODCACHE_REPLACE) && (!ctx->media_dur || !ctx->dash_dur) ) {
				use_cache = GF_TRUE;
			}

			if (ctx->vodcache==MP4MX_VODCACHE_INSERT) {
				gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DISABLE_PROGRESSIVE, &PROP_UINT(GF_PID_FILE_PATCH_INSERT) );
			}
			else if (!use_cache) {
				gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DISABLE_PROGRESSIVE, &PROP_UINT(GF_PID_FILE_PATCH_REPLACE) );
			}
		}
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
			gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_ISOM, GF_FALSE);
			ctx->def_brand_patched = GF_TRUE;
		}
	}

	width = height = sr = nb_chan = z_order = txt_fsize = 0;
	nb_bps = 16;
	ch_layout = 0;
	fps.num = 25;
	fps.den = 1;
	sar.num = sar.den = 0;
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
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CHANNEL_LAYOUT);
	if (p) ch_layout = p->value.longuint;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_LANGUAGE);
	if (p) lang_name = p->value.string;

	if (is_true_pid) {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_NB_FRAMES);
		if (p) tkw->nb_frames = p->value.uint;
		else tkw->nb_frames = 0;
	}
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_ISOM_SUBTYPE);
	if (p) m_subtype_src = p->value.uint;


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
	case GF_CODECID_MPHA:
		if ((m_subtype_src!=GF_ISOM_SUBTYPE_MH3D_MHA1) && (m_subtype_src!=GF_ISOM_SUBTYPE_MH3D_MHA2))
			m_subtype = GF_ISOM_SUBTYPE_MH3D_MHA1;
		else
			m_subtype = m_subtype_src;
		comp_name = "MPEG-H Audio";
		break;
	case GF_CODECID_MHAS:
		if ((m_subtype_src!=GF_ISOM_SUBTYPE_MH3D_MHM1) && (m_subtype_src!=GF_ISOM_SUBTYPE_MH3D_MHM2))
			m_subtype = GF_ISOM_SUBTYPE_MH3D_MHM1;
		else
			m_subtype = m_subtype_src;
		comp_name = "MPEG-H AudioMux";
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
		if (ctx->xps_inband) {
			use_m4sys = GF_FALSE;
			if (ctx->xps_inband==1) skip_dsi = GF_TRUE;
		}
		break;
	case GF_CODECID_HEVC:
	case GF_CODECID_LHVC:
		m_subtype = ((ctx->xps_inband==1) || (ctx->xps_inband==2)) ? GF_ISOM_SUBTYPE_HEV1  : GF_ISOM_SUBTYPE_HVC1;
		use_hevc = GF_TRUE;
		comp_name = (codec_id == GF_CODECID_LHVC) ? "L-HEVC" : "HEVC";
		use_gen_sample_entry = GF_FALSE;
		if (ctx->xps_inband) {
			use_m4sys = GF_FALSE;
			if (ctx->xps_inband==1) skip_dsi = GF_TRUE;
		}
		if (codec_id==GF_CODECID_HEVC_TILES) {
			m_subtype = GF_ISOM_SUBTYPE_HVT1;
			skip_dsi = GF_TRUE;
		}
		break;
	case GF_CODECID_HEVC_TILES:
		m_subtype = GF_ISOM_SUBTYPE_HVT1;
		skip_dsi = GF_TRUE;
		use_hvt1 = GF_TRUE;
		use_m4sys = GF_FALSE;
		comp_name = "HEVC Tiles";
		use_gen_sample_entry = GF_FALSE;
		break;
	case GF_CODECID_VVC:
		m_subtype = ((ctx->xps_inband==1) || (ctx->xps_inband==2)) ? GF_ISOM_SUBTYPE_VVI1  : GF_ISOM_SUBTYPE_VVC1;
		use_vvc = GF_TRUE;
		comp_name = "HEVC";
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
	case GF_CODECID_TEXT_MPEG4:
		use_m4sys = GF_TRUE;
		gf_isom_set_media_type(ctx->file, tkw->track_num, GF_ISOM_MEDIA_SCENE);
		comp_name = "MPEG4 Streaming Text";
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
		tkw->skip_bitrate_update = GF_TRUE;
		if (tkw->stream_type == GF_STREAM_AUDIO) {
			u32 req_non_planar_type = 0;
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_AUDIO_FORMAT);
			if (!p) break;
			comp_name = "RawAudio";
			unknown_generic = GF_FALSE;
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
				m_subtype_alt_raw = GF_ISOM_SUBTYPE_IPCM;
				break;
			case GF_AUDIO_FMT_S24P:
			 	req_non_planar_type = GF_AUDIO_FMT_S24;
			case GF_AUDIO_FMT_S24:
				m_subtype = GF_QT_SUBTYPE_IN24;
				m_subtype_alt_raw = GF_ISOM_SUBTYPE_IPCM;
				break;
			case GF_AUDIO_FMT_S32P:
			 	req_non_planar_type = GF_AUDIO_FMT_S32P;
			case GF_AUDIO_FMT_S32:
				m_subtype = GF_QT_SUBTYPE_IN32;
				m_subtype_alt_raw = GF_ISOM_SUBTYPE_IPCM;
				break;
			case GF_AUDIO_FMT_FLTP:
			 	req_non_planar_type = GF_AUDIO_FMT_FLTP;
			case GF_AUDIO_FMT_FLT:
				m_subtype = GF_QT_SUBTYPE_FL32;
				m_subtype_alt_raw = GF_ISOM_SUBTYPE_FPCM;
				break;
			case GF_AUDIO_FMT_DBLP:
			 	req_non_planar_type = GF_AUDIO_FMT_DBL;
			case GF_AUDIO_FMT_DBL:
				m_subtype = GF_QT_SUBTYPE_FL64;
				m_subtype_alt_raw = GF_ISOM_SUBTYPE_FPCM;
				break;
			default:
				unknown_generic = GF_TRUE;
				m_subtype = p->value.uint;
				break;
			}
			if (req_non_planar_type) {
				if (is_true_pid)
					gf_filter_pid_negociate_property(pid, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT(GF_AUDIO_FMT_S16));
				else {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] raw audio format planar in DASH multi-stsd mode is not supported, try assigning a resampler before the dasher\n"));
					return GF_NOT_SUPPORTED;
				}
			}
			raw_bitdepth = gf_audio_fmt_bit_depth(p->value.uint);
			tkw->raw_audio_bytes_per_sample = raw_bitdepth;
			tkw->raw_audio_bytes_per_sample *= nb_chan;
			tkw->raw_audio_bytes_per_sample /= 8;
		}
		else if (tkw->stream_type == GF_STREAM_VISUAL) {
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_PIXFMT);
			if (!p) break;
			comp_name = "RawVideo";
			unknown_generic = GF_FALSE;
			tkw->skip_bitrate_update = GF_TRUE;
			//we currently only set QTFF-style raw media
			switch (p->value.uint) {
			case GF_PIXEL_RGB:
				m_subtype = GF_QT_SUBTYPE_RAW;
				break;
			case GF_PIXEL_YUV422:
				m_subtype = GF_QT_SUBTYPE_YUV422;
				force_colr = GF_TRUE;
				break;
			case GF_PIXEL_YUV422_10:
				m_subtype = GF_QT_SUBTYPE_YUV422_10;
				force_colr = GF_TRUE;
				break;
			case GF_PIXEL_YUV444:
				m_subtype = GF_QT_SUBTYPE_YUV444;
				force_colr = GF_TRUE;
				break;
			case GF_PIXEL_YUV444_10:
				m_subtype = GF_QT_SUBTYPE_YUV444_10;
				force_colr = GF_TRUE;
				break;
			default:
				unknown_generic = GF_TRUE;
				m_subtype = p->value.uint;
				break;
			}
		}
		break;

	default:
		m_subtype = codec_id;
		unknown_generic = GF_TRUE;
		use_gen_sample_entry = GF_TRUE;
		use_m4sys = GF_FALSE;
		if (is_prores)
			unknown_generic = GF_FALSE;

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
			gf_isom_set_media_timescale(ctx->file, tkw->track_num, sr, 0, GF_TRUE);
		}
		else if (width && fps.den) {
			tkw->tk_timescale = fps.den;
			gf_isom_set_media_timescale(ctx->file, tkw->track_num, fps.den, 0, GF_TRUE);
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
					GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MP4Mux] AVC config update after movie has been finalized, moving all SPS/PPS inband (file might not be compliant)\n"));
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
					GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MP4Mux] HEVC config update after movie has been finalized, moving all SPS/PPS inband (file might not be compliant)\n"));
				}
				ctx->xps_inband = 2;
			}
			mp4_mux_make_inband_header(ctx, tkw);
			return GF_OK;
		}
		else if (use_vvc && dsi) {
			if (tkw->vvcc) gf_odf_vvc_cfg_del(tkw->vvcc);
			tkw->vvcc = gf_odf_vvc_cfg_read(dsi->value.data.ptr, dsi->value.data.size);

			if (!ctx->xps_inband) {
				if (ctx->init_movie_done) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MP4Mux] VVC config update after movie has been finalized, moving all SPS/PPS inband (file might not be compliant)\n"));
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

	if (use_m4sys && !gf_codecid_oti(codec_id)) {
		use_m4sys = GF_FALSE;
	}
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


#ifndef GPAC_DISABLE_AV_PARSERS
		if (dsi && (tkw->stream_type==GF_STREAM_AUDIO)) {
			GF_M4ADecSpecInfo acfg;
			gf_m4a_get_config(dsi->value.data.ptr, dsi->value.data.size, &acfg);
			audio_pli = acfg.audioPL;
		}
		//patch to align old arch (IOD not written in dash) with new
		if (audio_pli && !ctx->dash_mode)
			gf_isom_set_pl_indication(ctx->file, GF_ISOM_PL_AUDIO, audio_pli);
#endif

	} else if (use_avc) {
		if (tkw->avcc) gf_odf_avc_cfg_del(tkw->avcc);

		//not yet known
		if (!dsi) return GF_OK;

		tkw->avcc = gf_odf_avc_cfg_read(dsi->value.data.ptr, dsi->value.data.size);

		if (needs_sample_entry) {

			if (tkw->codecid == GF_CODECID_SVC) {
				e = gf_isom_svc_config_new(ctx->file, tkw->track_num, tkw->avcc, NULL, NULL, &tkw->stsd_idx);
			} else if (tkw->codecid == GF_CODECID_MVC) {
				e = gf_isom_mvc_config_new(ctx->file, tkw->track_num, tkw->avcc, NULL, NULL, &tkw->stsd_idx);
			} else {
				e = gf_isom_avc_config_new(ctx->file, tkw->track_num, tkw->avcc, NULL, NULL, &tkw->stsd_idx);
			}

			if (!e && enh_dsi) {
				if (tkw->svcc) gf_odf_avc_cfg_del(tkw->svcc);
				tkw->svcc = gf_odf_avc_cfg_read(enh_dsi->value.data.ptr, enh_dsi->value.data.size);
				if (tkw->svcc) {
					if ((tkw->svcc->AVCProfileIndication==118) || (tkw->svcc->AVCProfileIndication==128)) {
						e = gf_isom_mvc_config_update(ctx->file, tkw->track_num, tkw->stsd_idx, tkw->svcc, GF_TRUE);
					} else {
						e = gf_isom_svc_config_update(ctx->file, tkw->track_num, tkw->stsd_idx, tkw->svcc, GF_TRUE);
					}
					if (e) {
						gf_odf_avc_cfg_del(tkw->svcc);
						tkw->svcc = NULL;
					}

					if (ctx->xps_inband) {
						gf_isom_avc_set_inband_config(ctx->file, tkw->track_num, tkw->stsd_idx, (ctx->xps_inband==2) ? GF_TRUE : GF_FALSE);
					}
				}
			}
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Error creating new AVC sample description: %s\n", gf_error_to_string(e) ));
				return e;
			}
		}
		
		if (ctx->xps_inband) {
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
			gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_AVC1, GF_TRUE);

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
			gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_HVTI, GF_TRUE);
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
				gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_ISOM, GF_FALSE);
			}
			//patch for old arch
			else if (ctx->dash_mode) {
				Bool force_brand=GF_FALSE;
				if (((ctx->major_brand_set>>24)=='i') && (((ctx->major_brand_set>>16)&0xFF)=='s') && (((ctx->major_brand_set>>8)&0xFF)=='o')) {
					if ( (ctx->major_brand_set&0xFF) <'6') force_brand=GF_TRUE;
				}

				if (!force_brand && ctx->major_brand_set) {
					gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_ISO6, GF_TRUE);
				} else {
					gf_isom_set_brand_info(ctx->file, GF_ISOM_BRAND_ISO6, 1);
					gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_ISOM, GF_FALSE);
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
	} else if (use_vvc) {
		if (tkw->vvcc) gf_odf_vvc_cfg_del(tkw->vvcc);

		if (!dsi) {
			//not yet known
			return GF_OK;
		}
		tkw->vvcc = gf_odf_vvc_cfg_read(dsi->value.data.ptr, dsi->value.data.size);

		tkw->is_nalu = NALU_VVC;

		if (needs_sample_entry) {
			e = gf_isom_vvc_config_new(ctx->file, tkw->track_num, tkw->vvcc, NULL, NULL, &tkw->stsd_idx);

			if (!tkw->has_brands) {
				gf_isom_set_brand_info(ctx->file, GF_ISOM_BRAND_ISO4, 1);
				gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_ISOM, GF_FALSE);
			}
			//patch for old arch
			else if (ctx->dash_mode) {
				Bool force_brand=GF_FALSE;
				if (((ctx->major_brand_set>>24)=='i') && (((ctx->major_brand_set>>16)&0xFF)=='s') && (((ctx->major_brand_set>>8)&0xFF)=='o')) {
					if ( (ctx->major_brand_set&0xFF) <'6') force_brand=GF_TRUE;
				}

				if (!force_brand && ctx->major_brand_set) {
					gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_ISO6, GF_TRUE);
				} else {
					gf_isom_set_brand_info(ctx->file, GF_ISOM_BRAND_ISO6, 1);
					gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_ISOM, GF_FALSE);
				}
			}

			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Error creating new HEVC sample description: %s\n", gf_error_to_string(e) ));
				return e;
			}
		}

		if (dsi && ctx->xps_inband) {
			//this will cleanup all PS in vvcC
			gf_isom_vvc_set_inband_config(ctx->file, tkw->track_num, tkw->stsd_idx, (ctx->xps_inband==2) ? GF_TRUE : GF_FALSE);
		} else {
			gf_odf_vvc_cfg_del(tkw->vvcc);
			tkw->vvcc = NULL;
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
			gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_ISOM, GF_FALSE);
			gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_AV01, GF_TRUE);
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
			switch (gpp_cfg.type) {
			case GF_ISOM_SUBTYPE_3GP_QCELP:
			case GF_ISOM_SUBTYPE_3GP_EVRC:
			case GF_ISOM_SUBTYPE_3GP_SMV:
				gf_isom_set_brand_info(ctx->file, GF_ISOM_BRAND_3G2A, 65536);
				break;
			case GF_ISOM_SUBTYPE_3GP_H263:
				gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_3GG6, GF_TRUE);
				gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_3GG5, GF_TRUE);
				break;
			}
		}
		tkw->skip_bitrate_update = GF_TRUE;
	} else if (use_ac3_entry) {
		GF_AC3Config ac3cfg;
		memset(&ac3cfg, 0, sizeof(GF_AC3Config));

		if (dsi) {
			gf_isom_ac3_config_parse(dsi->value.data.ptr, dsi->value.data.size, (codec_id==GF_CODECID_EAC3) ? GF_TRUE : GF_FALSE, &ac3cfg);
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
	} else if (codec_id==GF_CODECID_TMCD) {
		u32 tmcd_flags=0, tmcd_fps_num=0, tmcd_fps_den=0;
		s32 tmcd_fpt=0;

		p = gf_filter_pid_get_property_str(pid, "tmcd:flags");
		if (p) tmcd_flags = p->value.uint;
		p = gf_filter_pid_get_property_str(pid, "tmcd:framerate");
		if (p) {
			tmcd_fps_num = p->value.frac.num;
			tmcd_fps_den = p->value.frac.den;
		}
		p = gf_filter_pid_get_property_str(pid, "tmcd:frames_per_tick");
		if (p) tmcd_fpt = p->value.uint;
		if (tkw->tk_timescale != tmcd_fps_num) {
			tmcd_fps_den *= tmcd_fps_num;
			tmcd_fps_den /= tkw->tk_timescale;
		}

		e = gf_isom_tmcd_config_new(ctx->file, tkw->track_num, tmcd_fps_num, tmcd_fps_den, tmcd_fpt, (tmcd_flags & 0x1), (tmcd_flags & 0x8), &tkw->stsd_idx);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Error creating new tmcd sample description: %s\n", gf_error_to_string(e) ));
			return e;
		}
	} else if (codec_id==GF_CODECID_DIMS) {
		GF_DIMSDescription dims_c;
		memset(&dims_c, 0, sizeof(GF_DIMSDescription));
		dims_c.contentEncoding = meta_content_encoding;
		dims_c.mime_type = meta_mime;
		dims_c.textEncoding = meta_encoding;
		dims_c.xml_schema_loc = meta_xmlns;

		p = gf_filter_pid_get_property_str(tkw->ipid, "dims:profile");
		if (p) dims_c.profile = p->value.uint;
		p = gf_filter_pid_get_property_str(tkw->ipid, "dims:level");
		if (p) dims_c.level = p->value.uint;
		p = gf_filter_pid_get_property_str(tkw->ipid, "dims:pathComponents");
		if (p) dims_c.pathComponents = p->value.uint;
		p = gf_filter_pid_get_property_str(tkw->ipid, "dims:fullRequestHost");
		if (p) dims_c.fullRequestHost = p->value.uint;
		p = gf_filter_pid_get_property_str(tkw->ipid, "dims:streamType");
		if (p) dims_c.streamType = p->value.boolean;
		p = gf_filter_pid_get_property_str(tkw->ipid, "dims:redundant");
		if (p) dims_c.containsRedundant = p->value.uint;
		p = gf_filter_pid_get_property_str(tkw->ipid, "dims:scriptTypes");
		if (p) dims_c.content_script_types = p->value.string;

		e = gf_isom_new_dims_description(ctx->file, tkw->track_num, &dims_c, NULL, NULL, &tkw->stsd_idx);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Error creating new DIMS sample description: %s\n", gf_error_to_string(e) ));
			return e;
		}
	} else if (use_gen_sample_entry) {
		u8 isor_ext_buf[14];
		u32 len = 0;
		GF_GenericSampleDescription udesc;
		memset(&udesc, 0, sizeof(GF_GenericSampleDescription));

		if (!comp_name) comp_name = "Unknown";
		len = (u32) strlen(comp_name);
		if (len>32) len = 32;
		udesc.compressor_name[0] = len;
		memcpy(udesc.compressor_name+1, comp_name, len);
		if ((codec_id==GF_CODECID_RAW) || unknown_generic)
			udesc.vendor_code = GF_4CC('G','P','A','C');

		udesc.samplerate = sr;
		udesc.nb_channels = nb_chan;
		if (codec_id==GF_CODECID_RAW) {
			if (ase_mode==GF_IMPORT_AUDIO_SAMPLE_ENTRY_v1_MPEG) {
				m_subtype = m_subtype_alt_raw;
				udesc.extension_buf_size = 14;
				udesc.extension_buf = isor_ext_buf;
				memset(isor_ext_buf, 0, sizeof(u8)*14);
				isor_ext_buf[3] = 14;
				isor_ext_buf[4] = 'p';
				isor_ext_buf[5] = 'c';
				isor_ext_buf[6] = 'm';
				isor_ext_buf[7] = 'C';
				isor_ext_buf[12] = 1; //little endian only for now
				isor_ext_buf[13] = raw_bitdepth; //little endian only for now
			} else {
				udesc.is_qtff = GF_TRUE;
				udesc.version = 1;
				ase_mode = GF_IMPORT_AUDIO_SAMPLE_ENTRY_v1_QTFF;
			}
		}
		udesc.codec_tag = m_subtype;
		udesc.width = width;
		udesc.height = height;
		if (width) {
			udesc.v_res = 72;
			udesc.h_res = 72;
			udesc.depth = 24;
		}
		if (unknown_generic) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MP4Mux] muxing unknown codec ID %s, using generic sample entry with 4CC \"%s\"\n", gf_codecid_name(codec_id), gf_4cc_to_str(m_subtype) ));
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

	if (ctx->btrt && !tkw->skip_bitrate_update) {
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
		} else if (use_3gpp_config) {
			GF_3GPConfig *gpp_cfg = gf_isom_3gp_config_get(ctx->file, tkw->track_num, tkw->stsd_idx);
			if (gpp_cfg) {
				gpp_cfg->AMR_mode_set = tkw->amr_mode_set;
				gf_isom_3gp_config_update(ctx->file, tkw->track_num, gpp_cfg, tkw->stsd_idx);
				gf_free(gpp_cfg);
			}
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

		tkw->scheme_type = scheme_type;
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
		case GF_ISOM_PIFF_SCHEME:
		case GF_ISOM_CENC_SCHEME:
		case GF_ISOM_CENS_SCHEME:
		case GF_ISOM_CBC_SCHEME:
		case GF_ISOM_CBCS_SCHEME:
			tkw->cenc_state = CENC_NEED_SETUP;
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
			if ((m_subtype==GF_ISOM_SUBTYPE_IPCM) || (m_subtype==GF_ISOM_SUBTYPE_FPCM)) {
				GF_AudioChannelLayout layout;
				memset(&layout, 0, sizeof(GF_AudioChannelLayout));
				layout.stream_structure = 1;
				layout.channels_count = nb_chan;
				if (ch_layout)
					layout.definedLayout = gf_audio_fmt_get_cicp_from_layout(ch_layout);
				else
					layout.definedLayout = gf_audio_fmt_get_cicp_layout(nb_chan, 0, 0);
				gf_isom_set_audio_layout(ctx->file, tkw->track_num, tkw->stsd_idx, &layout);
			}
		}
		else if (width) {
			u32 colour_type=0;
			u16 colour_primaries=0, transfer_characteristics=0, matrix_coefficients=0;
			Bool full_range_flag=GF_FALSE;

			gf_isom_set_visual_info(ctx->file, tkw->track_num, tkw->stsd_idx, width, height);
			if (sar.den) {
				if (sar.num != sar.den) {
					gf_isom_set_pixel_aspect_ratio(ctx->file, tkw->track_num, tkw->stsd_idx, sar.num, sar.den, GF_FALSE);
					width = width * sar.num / sar.den;
				}
				//old importer did not set PASP for
				else if (!gf_sys_old_arch_compat() || (codec_id!=GF_CODECID_MPEG4_PART2) ) {
					gf_isom_set_pixel_aspect_ratio(ctx->file, tkw->track_num, tkw->stsd_idx, 1, 1, GF_TRUE);
				}
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

			if ((ctx->prores_track == tkw) || force_colr) {
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
						if (ctx->make_qt==1) {
							GF_LOG(GF_LOG_WARNING, GF_LOG_AUTHOR, ("[ProRes] No color info present in visual track, defaulting to BT709\n"));
						}
					}
					gf_isom_set_visual_color_info(ctx->file, tkw->track_num, tkw->stsd_idx, GF_4CC('n','c','l','c'), 1, 1, 1, GF_FALSE, NULL, 0);
				}

				if (ctx->prores_track == tkw) {
					u32 chunk_size;
					if ((width<=720) && (height<=576)) chunk_size = 2000000;
					else chunk_size = 4000000;
					gf_isom_hint_max_chunk_size(ctx->file, tkw->track_num, chunk_size);
				}
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
#ifndef GPAC_DISABLE_LOG
		const char *dst_type = tkw->is_item ? "Item Importing" : "Track Importing";
#endif
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
#ifndef GPAC_DISABLE_AV_PARSERS
		if (tkw->svcc) {
			AVCState avc;
			memset(&avc, 0, sizeof(AVCState));
			count = gf_list_count(tkw->svcc->sequenceParameterSets);
			for (i=0; i<count; i++) {
				GF_NALUFFParam *sl = gf_list_get(tkw->svcc->sequenceParameterSets, i);
				u8 nal_type = sl->data[0] & 0x1F;
				Bool is_subseq = (nal_type == GF_AVC_NALU_SVC_SUBSEQ_PARAM) ? GF_TRUE : GF_FALSE;
				s32 ps_idx = gf_media_avc_read_sps(sl->data, sl->size, &avc, is_subseq, NULL);
				if (ps_idx>=0) {
					GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("SVC Detected - SSPS ID %d - frame size %d x %d\n", ps_idx-GF_SVC_SSPS_ID_SHIFT, avc.sps[ps_idx].width, avc.sps[ps_idx].height ));

				}
			}
		}
#endif

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
		Bool remove_edits = GF_FALSE;
		s64 moffset=0;
		ctx->config_timing = GF_TRUE;
		ctx->update_report = GF_TRUE;

		//if we have an edit list (due to track template) only providing media offset, trash it
		if (!gf_isom_get_edit_list_type(ctx->file, tkw->track_num, &moffset)) {
			if (!gf_sys_old_arch_compat()) {
				gf_isom_remove_edits(ctx->file, tkw->track_num);
			} else {
				remove_edits = GF_TRUE;
			}
		}
		p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_DELAY);
		if (p) {
			if (p->value.sint < 0) {
				if (ctx->ctmode==MP4MX_CT_NEGCTTS) use_negccts = GF_TRUE;
				else {
					if (remove_edits) {
						gf_isom_remove_edits(ctx->file, tkw->track_num);
					}
					gf_isom_set_edit(ctx->file, tkw->track_num, 0, 0, -p->value.sint, GF_ISOM_EDIT_NORMAL);
				}
			} else if (p->value.sint > 0) {
				s64 dur = p->value.sint;
				dur *= (u32) ctx->moovts;
				dur /= tkw->src_timescale;
				if (remove_edits) {
					gf_isom_remove_edits(ctx->file, tkw->track_num);
				}
				gf_isom_set_edit(ctx->file, tkw->track_num, 0, dur, 0, GF_ISOM_EDIT_DWELL);
				gf_isom_set_edit(ctx->file, tkw->track_num, 0, 0, 0, GF_ISOM_EDIT_NORMAL);
			}
			tkw->ts_delay = p->value.sint;
		} else if (tkw->stream_type==GF_STREAM_VISUAL) {
			tkw->probe_min_ctts = GF_TRUE;
		}
		if (use_negccts) {
			gf_isom_set_composition_offset_mode(ctx->file, tkw->track_num, GF_TRUE);

			if (!tkw->has_brands) {
				gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_ISO4, GF_TRUE);
				gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_ISOM, GF_FALSE);
				gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_ISO1, GF_FALSE);
				gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_ISO2, GF_FALSE);
				gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_ISO3, GF_FALSE);
			}

			tkw->negctts_shift = (tkw->ts_delay<0) ? -tkw->ts_delay : 0;
		} else {
			//this will remove any cslg in the track due to template
			gf_isom_set_composition_offset_mode(ctx->file, tkw->track_num, GF_FALSE);
		}

		mp4_mux_set_tags(ctx, tkw);
	}
	return GF_OK;
}

static GF_Err mp4_mux_done(GF_Filter *filter, GF_MP4MuxCtx *ctx, Bool is_final);

static GF_Err mp4_mux_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	GF_MP4MuxCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		TrackWriter *tkw = gf_filter_pid_get_udta(pid);
		if (tkw) {
			gf_list_del_item(ctx->tracks, tkw);
			gf_free(tkw);
		}
		//removing last pid, flush file
		if (ctx->opid && !gf_list_count(ctx->tracks) && ctx->file && !ctx->init_movie_done) {
			return mp4_mux_done(filter, ctx, GF_TRUE);
		}
		return GF_OK;
	}
	return mp4_mux_setup_pid(filter, pid, GF_TRUE);
}

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
			mp4_mux_done(filter, ctx, GF_TRUE);
		}
	}
	if (evt->base.on_pid && (evt->base.type==GF_FEVT_PLAY) ) {
		//by default don't cancel event - to rework once we have downloading in place
		ctx->force_play = GF_TRUE;
		if (evt->play.speed<0)
			ctx->is_rewind = GF_TRUE;
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
		gf_cenc_set_pssh(ctx->file, sysID, version, kid_count, keyIDs, data, len, (tkw->scheme_type==GF_ISOM_PIFF_SCHEME) ? GF_TRUE : GF_FALSE);
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

	if (tkw->cenc_state == CENC_SETUP_ERROR)
		return GF_SERVICE_ERROR;

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
	if (tkw->cenc_state==CENC_NEED_SETUP) {
		u32 cenc_stsd_mode=0;
		u32 container_type = GF_ISOM_BOX_TYPE_SENC;

		p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_CENC_STSD_MODE);
		if (p) cenc_stsd_mode = p->value.uint;

		p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_ENCRYPTED);
		if (p) pck_is_encrypted = p->value.boolean;
		else pck_is_encrypted = GF_FALSE;


		p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_CENC_STORE);
		if (p && p->value.uint) container_type = p->value.uint;

		tkw->clear_stsd_idx = 0;
		if (cenc_stsd_mode) {
			u32 clone_stsd_idx;
			e = gf_isom_clone_sample_description(ctx->file, tkw->track_num, ctx->file, tkw->track_num, tkw->stsd_idx, NULL, NULL, &clone_stsd_idx);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Failed to clone sample description: %s\n", gf_error_to_string(e) ));
				return e;
			}
			//before
			if (cenc_stsd_mode==1) {
				tkw->clear_stsd_idx = tkw->stsd_idx;
				tkw->stsd_idx = clone_stsd_idx;
			}
			//after
			else {
				tkw->clear_stsd_idx = clone_stsd_idx;
			}
		}

		tkw->cenc_state = CENC_SETUP_DONE;
		e = gf_isom_set_cenc_protection(ctx->file, tkw->track_num, tkw->stsd_idx, scheme_type, scheme_version, pck_is_encrypted, IV_size, KID, crypt_byte_block, skip_byte_block, constant_IV_size, constant_IV);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Failed to setup CENC information: %s\n", gf_error_to_string(e) ));
			tkw->cenc_state = CENC_SETUP_ERROR;
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
			gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_OPF2, GF_TRUE);

		if (container_type) {
			e = gf_isom_cenc_allocate_storage(ctx->file, tkw->track_num, container_type, 0, 0, NULL);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Failed to setup CENC storage: %s\n", gf_error_to_string(e) ));
				tkw->cenc_state = CENC_SETUP_ERROR;
				return e;
			}
		}
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
		if (tkw->clear_stsd_idx) {
			if (act_type==CENC_ADD_FRAG) {
				return gf_isom_fragment_set_cenc_sai(ctx->file, tkw->track_id, 0, NULL, 0, GF_FALSE, ctx->saio32);
			} else {
				return gf_isom_track_cenc_add_sample_info(ctx->file, tkw->track_num, GF_ISOM_BOX_TYPE_SENC, 0, NULL, 0, GF_FALSE, NULL, ctx->saio32);
			}
		} else {
			bin128 dumb_IV;
			memset(dumb_IV, 0, 16);
			e = gf_isom_set_sample_cenc_group(ctx->file, tkw->track_num, sample_num, 0, 0, dumb_IV, 0, 0, 0, NULL);
			IV_size = 0;
			tkw->has_seig = GF_TRUE;
		}
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
		if (tkw->constant_IV_size && !tkw->cenc_subsamples)
			return GF_OK;
		sai_size = pck_size;
	}

	if (act_type==CENC_ADD_FRAG) {
		if (pck_is_encrypted) {
			e = gf_isom_fragment_set_cenc_sai(ctx->file, tkw->track_id, IV_size, sai, sai_size, tkw->cenc_subsamples, ctx->saio32);
		} else {
			e = gf_isom_fragment_set_cenc_sai(ctx->file, tkw->track_id, 0, NULL, 0, GF_FALSE, ctx->saio32);
		}
		if (e) return e;
	} else {
		if (sai) {
			e = gf_isom_track_cenc_add_sample_info(ctx->file, tkw->track_num, GF_ISOM_BOX_TYPE_SENC, IV_size, sai, sai_size, tkw->cenc_subsamples, NULL, ctx->saio32);
		} else if (!pck_is_encrypted) {
			e = gf_isom_track_cenc_add_sample_info(ctx->file, tkw->track_num, GF_ISOM_BOX_TYPE_SENC, 0, NULL, 0, GF_FALSE, NULL, ctx->saio32);
		}
		if (e) return e;
	}

	return GF_OK;
}

GF_FilterSAPType mp4_mux_get_sap(GF_MP4MuxCtx *ctx, GF_FilterPacket *pck)
{
	GF_FilterSAPType sap = gf_filter_pck_get_sap(pck);
	if (!sap) return sap;
	if (ctx->forcesync) return GF_FILTER_SAP_1;
	return sap;
}

static GF_Err mp4_mux_process_sample(GF_MP4MuxCtx *ctx, TrackWriter *tkw, GF_FilterPacket *pck, Bool for_fragment)
{
	GF_Err e=GF_OK;
	u64 cts, prev_dts;
	u32 prev_size=0;
	u32 duration = 0;
	u32 timescale = 0;
	const GF_PropertyValue *subs;
	GF_FilterSAPType sap_type;
	Bool insert_subsample_dsi = GF_FALSE;
	u32 first_nal_is_audelim = GF_FALSE;
	u32 sample_desc_index = tkw->stsd_idx;

	timescale = gf_filter_pck_get_timescale(pck);

	subs = gf_filter_pck_get_property(pck, GF_PROP_PCK_SUBS);

	prev_dts = tkw->nb_samples ? tkw->sample.DTS : GF_FILTER_NO_TS;
	prev_size = tkw->sample.dataLength;
	tkw->sample.CTS_Offset = 0;
	tkw->sample.data = (char *)gf_filter_pck_get_data(pck, &tkw->sample.dataLength);

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

	//tkw->ts_shift is in source timescale, apply it before rescaling TSs/duration
	if (tkw->ts_shift) {
		if (ctx->is_rewind) {
			if (tkw->sample.DTS <= tkw->ts_shift) {
				tkw->sample.DTS = tkw->ts_shift - tkw->sample.DTS;
				cts = tkw->ts_shift - cts;
			} else {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MP4Mux] broken timing in track, initial ts "LLU" less than TS "LLU"\n", tkw->ts_shift, tkw->sample.DTS));
			}
		} else {
			if (tkw->sample.DTS >= tkw->ts_shift) {
				tkw->sample.DTS -= tkw->ts_shift;
				cts -= tkw->ts_shift;
			} else {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MP4Mux] broken timing in track, initial ts "LLU" greater than TS "LLU"\n", tkw->ts_shift, tkw->sample.DTS));
			}
		}
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
	sap_type = mp4_mux_get_sap(ctx, pck);
	if (sap_type==GF_FILTER_SAP_1)
		tkw->sample.IsRAP = SAP_TYPE_1;
	else if ( (sap_type == GF_FILTER_SAP_4) && (tkw->stream_type != GF_STREAM_VISUAL) )
		tkw->sample.IsRAP = SAP_TYPE_1;

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

	if (tkw->cenc_state && tkw->clear_stsd_idx && !gf_filter_pck_get_crypt_flags(pck)) {
		sample_desc_index = tkw->clear_stsd_idx;
	}

	if (tkw->use_dref) {
		u64 data_offset = gf_filter_pck_get_byte_offset(pck);
		if (data_offset != GF_FILTER_NO_BO) {
			e = gf_isom_add_sample_reference(ctx->file, tkw->track_num, sample_desc_index, &tkw->sample, data_offset);
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
				e = gf_isom_fragment_add_sample(ctx->file, tkw->track_id, &tkw->sample, sample_desc_index, duration, 0, 0, 0);
				if (!e && au_delim) {
					e = gf_isom_fragment_append_data(ctx->file, tkw->track_id, tkw->inband_hdr, tkw->inband_hdr_size, 0);
				}
				if (!e) e = gf_isom_fragment_append_data(ctx->file, tkw->track_id, pck_data, pck_data_len, 0);
			} else {
				e = gf_isom_add_sample(ctx->file, tkw->track_num, sample_desc_index, &tkw->sample);
				if (au_delim && !e) {
					e = gf_isom_append_sample_data(ctx->file, tkw->track_num, tkw->inband_hdr, tkw->inband_hdr_size);
				}
				if (!e) e = gf_isom_append_sample_data(ctx->file, tkw->track_num, pck_data, pck_data_len);
			}
			insert_subsample_dsi = GF_TRUE;
		} else if (for_fragment) {
			e = gf_isom_fragment_add_sample(ctx->file, tkw->track_id, &tkw->sample, sample_desc_index, duration, 0, 0, 0);
		} else {
			e = gf_isom_add_sample(ctx->file, tkw->track_num, sample_desc_index, &tkw->sample);
			if (!e && !duration) {
				gf_isom_set_last_sample_duration(ctx->file, tkw->track_num, 0);
			}
		}

		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Failed to add sample DTS "LLU" - prev DTS "LLU": %s\n", tkw->sample.DTS, prev_dts, gf_error_to_string(e) ));
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MP4Mux] added sample DTS "LLU" - prev DTS "LLU" - prev size %d\n", tkw->sample.DTS, prev_dts, prev_size));
		}

		if (!e && tkw->cenc_state) {
			e = mp4_mux_cenc_update(ctx, tkw, pck, for_fragment ? CENC_ADD_FRAG : CENC_ADD_NORMAL, tkw->sample.dataLength);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Failed to set sample CENC information: %s\n", gf_error_to_string(e) ));
			}
		}
	}

	tkw->nb_samples++;
	tkw->samples_in_stsd++;
	tkw->samples_in_frag++;

	if (e) return e;

	//compat with old arch: write sample to group info for all samples
	if ((sap_type==3) || tkw->has_open_gop)  {
		if (for_fragment) {
			e = gf_isom_fragment_set_sample_rap_group(ctx->file, tkw->track_id, tkw->samples_in_frag, (sap_type==3) ? GF_TRUE : GF_FALSE, 0);
		} else if (sap_type==3) {
			e = gf_isom_set_sample_rap_group(ctx->file, tkw->track_num, tkw->nb_samples, GF_TRUE /*(sap_type==3) ? GF_TRUE : GF_FALSE*/, 0);
		}
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Failed to set sample DTS "LLU" SAP 3 in RAP group: %s\n", tkw->sample.DTS, gf_error_to_string(e) ));
		}
		tkw->has_open_gop = GF_TRUE;
	}
	if (!ctx->noroll) {
		if ((sap_type==GF_FILTER_SAP_4) || (sap_type==GF_FILTER_SAP_4_PROL) || tkw->gdr_type) {
			GF_ISOSampleRollType roll_type = 0;
			s16 roll = gf_filter_pck_get_roll_info(pck);
			if (sap_type==GF_FILTER_SAP_4) roll_type = GF_ISOM_SAMPLE_ROLL;
			else if (sap_type==GF_FILTER_SAP_4_PROL) roll_type = GF_ISOM_SAMPLE_PREROLL;
			else if (tkw->gdr_type==GF_FILTER_SAP_4_PROL) {
				roll_type = GF_ISOM_SAMPLE_PREROLL_NONE;
			}

			if (for_fragment) {
				e = gf_isom_fragment_set_sample_roll_group(ctx->file, tkw->track_id, tkw->samples_in_frag, roll_type, roll);
			} else {
				e = gf_isom_set_sample_roll_group(ctx->file, tkw->track_num, tkw->nb_samples, roll_type, roll);
			}
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Failed to set sample DTS "LLU" SAP 4 roll %s in roll group: %s\n", tkw->sample.DTS, roll, gf_error_to_string(e) ));
			}
			if (sap_type && !tkw->gdr_type)
				tkw->gdr_type = sap_type;
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

	if (ctx->deps) {
		u8 dep_flags = gf_filter_pck_get_dependency_flags(pck);
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
	}


	tkw->next_is_first_sample = GF_FALSE;

	if (duration && !for_fragment && !tkw->raw_audio_bytes_per_sample)
		gf_isom_set_last_sample_duration(ctx->file, tkw->track_num, duration);

	if (ctx->idur.num) {
		Bool abort = GF_FALSE;
		if (ctx->idur.num>0) {
			u64 mdur = gf_isom_get_media_duration(ctx->file, tkw->track_num);

			/*patch to align to old arch */
			if (gf_sys_old_arch_compat() && (tkw->stream_type==GF_STREAM_VISUAL)) {
				mdur = tkw->sample.DTS;
			}

			if (ctx->importer) {
				tkw->prog_done = mdur * ctx->idur.den;
				tkw->prog_total =  ((u64)tkw->tk_timescale) * ctx->idur.num;
			}

			/*patch to align to old arch */
			if (gf_sys_old_arch_compat()) {
				if (mdur * (u64) ctx->idur.den > tkw->tk_timescale * (u64) ctx->idur.num)
					abort = GF_TRUE;
			} else {
				if (mdur * (u64) ctx->idur.den >= tkw->tk_timescale * (u64) ctx->idur.num)
					abort = GF_TRUE;
			}
		} else {
			if ((s32) tkw->nb_samples >= -ctx->idur.num)
				abort = GF_TRUE;
		}

		if (abort) {
			GF_FilterEvent evt;
			GF_FEVT_INIT(evt, GF_FEVT_STOP, tkw->ipid);
			gf_filter_pid_send_event(tkw->ipid, &evt);

			tkw->aborted = GF_TRUE;
		}
	} else if (ctx->importer) {
		if (tkw->nb_frames) {
			tkw->prog_done = tkw->nb_samples;
			tkw->prog_total = tkw->nb_frames;
		} else {
			u64 data_offset = gf_filter_pck_get_byte_offset(pck);
			if (data_offset == GF_FILTER_NO_BO) {
				data_offset = tkw->down_bytes;
			}
			if ((data_offset != GF_FILTER_NO_BO) && tkw->down_size) {
				tkw->prog_done = data_offset;
				tkw->prog_total = tkw->down_size;
			} else {
				if (tkw->pid_dur.den && tkw->pid_dur.num) {
					tkw->prog_done = tkw->sample.DTS * tkw->pid_dur.den;
					tkw->prog_total = tkw->pid_dur.num * tkw->tk_timescale;
				} else {
					tkw->prog_done = 0;
					tkw->prog_total = 1;
				}

			}
		}
	}
	return GF_OK;
}

static GF_Err mp4_mux_process_item(GF_MP4MuxCtx *ctx, TrackWriter *tkw, GF_FilterPacket *pck)
{
	GF_Err e;
	u32 meta_type, item_id, size, item_type, nb_items, media_brand;
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
		media_brand = GF_ISOM_BRAND_AVCI;
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
		} else {
			image_props.num_channels = 3;
			image_props.bits_per_channel[0] = ((GF_HEVCConfigurationBox *)config_box)->config->luma_bit_depth;
			image_props.bits_per_channel[1] = ((GF_HEVCConfigurationBox *)config_box)->config->chroma_bit_depth;
			image_props.bits_per_channel[2] = ((GF_HEVCConfigurationBox *)config_box)->config->chroma_bit_depth;
		}
		media_brand = GF_ISOM_BRAND_HEIC;
		if (tkw->codecid==GF_CODECID_LHVC) {
			media_brand = GF_ISOM_BRAND_HEIM;
		}
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
		media_brand = GF_ISOM_BRAND_AVIF;
		break;
	case GF_CODECID_JPEG:
		item_type = GF_ISOM_SUBTYPE_JPEG;
		media_brand = GF_ISOM_SUBTYPE_JPEG /* == GF_4CC('j', 'p', 'e', 'g') */;
		break;
	case GF_CODECID_J2K:
		item_type = GF_ISOM_SUBTYPE_JP2K;
		media_brand = GF_4CC('j', '2', 'k', 'i');
		break;
	case GF_CODECID_PNG:
		item_type = GF_ISOM_SUBTYPE_PNG;
		//not defined !
		media_brand = GF_ISOM_SUBTYPE_PNG /* == GF_4CC('j', 'p', 'e', 'g') */;
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
	tkw->item_id = item_id;

	p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_PRIMARY_ITEM);
	if (p && p->value.boolean) {
		e = gf_isom_set_meta_primary_item(ctx->file, GF_TRUE, 0, item_id);
		if (e) return e;
	}
	//if primary item is not set, assign one
	else if (! gf_isom_get_meta_primary_item_id(ctx->file, GF_TRUE, 0)) {
		e = gf_isom_set_meta_primary_item(ctx->file, GF_TRUE, 0, item_id);
		if (e) return e;
	}

	if (!ctx->major_brand_set) {
		gf_isom_set_brand_info(ctx->file, GF_ISOM_BRAND_MIF1, 0);
		gf_isom_reset_alt_brands(ctx->file);
		ctx->major_brand_set = 2;
	}
	if (media_brand && (ctx->major_brand_set==2)) {
		gf_isom_modify_alternate_brand(ctx->file, media_brand, 1);
	}
#if 0
	if (e == GF_OK && meta->ref_type) {
		e = gf_isom_meta_add_item_ref(file, meta->root_meta, tk, meta->item_id, meta->ref_item_id, meta->ref_type, NULL);
	}
#endif
	return GF_OK;
}

static void mp4mux_send_output(GF_MP4MuxCtx *ctx)
{
	if (ctx->dst_pck) {
		if (ctx->notify_filename) {
			gf_filter_pck_set_framing(ctx->dst_pck, GF_TRUE, GF_FALSE);
			gf_filter_pck_set_property(ctx->dst_pck, GF_PROP_PCK_FILENUM, &PROP_UINT(ctx->cur_file_idx_plus_one-1) );
			if (ctx->cur_file_suffix) {
				gf_filter_pck_set_property(ctx->dst_pck, GF_PROP_PCK_FILESUF, &PROP_STRING_NO_COPY(ctx->cur_file_suffix) );
				ctx->cur_file_suffix = NULL;
			}
			ctx->notify_filename = 0;
		}
		gf_filter_pck_send(ctx->dst_pck);
		ctx->dst_pck = NULL;
	}
}


static void mp4_mux_flush_frag(GF_MP4MuxCtx *ctx, Bool is_init, u64 idx_start_range, u64 idx_end_range)
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
		if (is_init) {
			gf_filter_pck_set_dependency_flags(ctx->dst_pck, 0xFF);
			gf_filter_pck_set_carousel_version(ctx->dst_pck, 1);
		}
		mp4mux_send_output(ctx);
	}

	if (ctx->dash_mode) {
		//send event on first track only
		tkw = gf_list_get(ctx->tracks, 0);
		GF_FEVT_INIT(evt, GF_FEVT_SEGMENT_SIZE, tkw->ipid);
		evt.seg_size.seg_url = NULL;
		evt.seg_size.is_init = is_init ? GF_TRUE : GF_FALSE;
		if (!is_init || !idx_end_range) {
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
#ifdef GF_ENABLE_CTRN
	u32 traf_inherit_base_id=0;
#endif
	u32 nb_segments=0;
	GF_Fraction64 max_dur;
	ctx->single_file = GF_TRUE;
	ctx->current_offset = ctx->current_size = 0;
	max_dur.den = 1;
	max_dur.num = 0;

	if (ctx->sseg && ctx->noinit)
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
		if (!pck) {
			if (gf_filter_pid_is_eos(tkw->ipid)) continue;
			return GF_OK;
		}

		if (!ctx->dash_mode && !ctx->cur_file_idx_plus_one) {
			p = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENUM);
			if (p) {
				ctx->cur_file_idx_plus_one = p->value.uint + 1;
				if (!ctx->cur_file_suffix) {
					p = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILESUF);
					if (p && p->value.string) ctx->cur_file_suffix = gf_strdup(p->value.string);
				}
				ctx->notify_filename = GF_TRUE;
			}
		}

		if (tkw->cenc_state==CENC_NEED_SETUP) {
			mp4_mux_cenc_update(ctx, tkw, pck, CENC_CONFIG, 0);
		}

		p = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENAME);
		if (p && strlen(p->value.string)) ctx->single_file = GF_FALSE;

		def_fake_dur = gf_filter_pck_get_duration(pck);
		def_fake_scale = tkw->src_timescale;

		p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_DURATION);
		if (p && p->value.lfrac.num>0 && p->value.lfrac.den) {
			tkw->pid_dur = p->value.lfrac;
			if (max_dur.num * (s64) p->value.lfrac.den < (s64) max_dur.den * p->value.lfrac.num) {
				max_dur.num = p->value.lfrac.num;
				max_dur.den = p->value.lfrac.den;
			}
		}
#ifdef GF_ENABLE_CTRN
		if (tkw->codecid==GF_CODECID_HEVC)
			traf_inherit_base_id = tkw->track_id;
#endif
	}
	//good to go, finalize for fragments
	for (i=0; i<count; i++) {
		u32 def_pck_dur;
		u32 def_is_rap;
#ifdef GF_ENABLE_CTRN
		u32 inherit_traf_from_track = 0;
#endif
		u64 dts;
		const GF_PropertyValue *p;

		TrackWriter *tkw = gf_list_get(ctx->tracks, i);

		if (tkw->fake_track) {
			if (def_fake_scale) {
				def_pck_dur = def_fake_dur;
				def_pck_dur *= tkw->src_timescale;
				def_pck_dur /= def_fake_scale;
			} else {
				def_pck_dur = 0;
			}
		} else {
			GF_FilterPacket *pck = gf_filter_pid_get_packet(tkw->ipid);
			//can be null if eos
			if (pck) {
				u32 tscale;
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
			} else {
				def_pck_dur = 0;
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
			p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_HAS_SYNC);
			if (p && p->value.boolean)
				def_is_rap = GF_FALSE;
			break;
		case GF_STREAM_VISUAL:
			switch (tkw->codecid) {
			case GF_CODECID_PNG:
			case GF_CODECID_JPEG:
			case GF_CODECID_J2K:
				break;
			case GF_CODECID_HEVC_TILES:
#ifdef GF_ENABLE_CTRN
				if (ctx->ctrn && ctx->ctrni)
					inherit_traf_from_track = traf_inherit_base_id;
#endif
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
		//otherwise we need to update the stsd idx in the traf headers
		e = gf_isom_setup_track_fragment(ctx->file, tkw->track_id, tkw->stsd_idx, def_pck_dur, 0, (u8) def_is_rap, 0, 0, ctx->nofragdef ? GF_TRUE : GF_FALSE);
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
			continue;
		}

#ifdef GF_ENABLE_CTRN
		if (inherit_traf_from_track)
			gf_isom_enable_traf_inherit(ctx->file, tkw->track_id, inherit_traf_from_track);
#endif

		if (!tkw->box_patched) {
			p = gf_filter_pid_get_property_str(tkw->ipid, "boxpatch");
			if (p && p->value.string) {
				e = gf_isom_apply_box_patch(ctx->file, tkw->track_id, p->value.string, GF_FALSE);
				if (e) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Unable to apply box patch %s to track %d: %s\n",
						p->value.string, tkw->track_id, gf_error_to_string(e) ));
				}
			}
			tkw->box_patched = GF_TRUE;
		}

		p = gf_filter_pid_get_property(tkw->ipid, GF_PROP_PID_DASH_SEGMENTS);
		if (p && (p->value.uint>nb_segments))
			nb_segments = p->value.uint;
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
		u32 mbrand, mcount, found=0;
		u8 szB[GF_4CC_MSIZE];
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

		gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_ISOM, GF_FALSE);
		gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_ISO1, GF_FALSE);
		gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_ISO2, GF_FALSE);
		gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_ISO3, GF_FALSE);
		gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_ISO4, GF_FALSE);
		gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_AVC1, GF_FALSE);
		gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_MP41, GF_FALSE);
		gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_MP42, GF_FALSE);
	}

	if (ctx->dash_mode) {
		/*DASH self-init media segment*/
		if (ctx->dash_mode==MP4MX_DASH_VOD) {
			gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_DSMS, GF_TRUE);
		} else {
			gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_DASH, GF_TRUE);
		}
		gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_MSIX, ((ctx->dash_mode==MP4MX_DASH_VOD) && (ctx->subs_sidx>=0)) ? GF_TRUE : GF_FALSE);
	}

	if (ctx->boxpatch && !ctx->box_patched) {
		e = gf_isom_apply_box_patch(ctx->file, 0, ctx->boxpatch, GF_FALSE);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Unable to apply box patch %s: %s\n", ctx->boxpatch, gf_error_to_string(e) ));
		}
		ctx->box_patched = GF_TRUE;
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
		mp4_mux_flush_frag(ctx, GF_TRUE, 0, 0);
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
		if ((ctx->vodcache==MP4MX_VODCACHE_REPLACE) && !nb_segments && (!ctx->media_dur || !ctx->dash_dur) ) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MP4Mux] Media duration unknown, cannot use replace mode of vodcache, using temp file for VoD storage\n"));
			ctx->vodcache = MP4MX_VODCACHE_ON;
			e = mp4mx_setup_dash_vod(ctx, NULL);
			if (e) return e;
		}

		if (ctx->vodcache==MP4MX_VODCACHE_REPLACE) {
			GF_BitStream *bs;
			u8 *output;
			char *msg;
			GF_FilterPacket *pck;
			u32 len;
			Bool exact_sidx = GF_TRUE;

			if (!nb_segments) {
				exact_sidx = GF_FALSE;
				nb_segments = (u32) ( ctx->media_dur / ctx->dash_dur);
				//always add an extra segment
				nb_segments ++;
				//and safety alloc of 10%
				if (nb_segments>10)
					nb_segments += 10*nb_segments/100;
				else
					nb_segments ++;
			}

			//max sidx size: full box + sidx fields + timing 64 bit + nb segs (each 12 bytes)
			ctx->sidx_max_size = 12 + (12 + 16) + 12 * nb_segments;

			//we produce an ssix, add full box + nb subsegs + nb_segments * (range_count=2 + 2*(range+size))
			if (ctx->ssix) {
				ctx->sidx_max_size += 12 + 4 + nb_segments * 12;
			}

			if (!exact_sidx) {
				//and a free box
				ctx->sidx_max_size += 8;
				ctx->sidx_size_exact = GF_FALSE;
			} else {
				ctx->sidx_size_exact = GF_TRUE;
			}
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
		} else if (ctx->vodcache==MP4MX_VODCACHE_ON) {
			ctx->store_output = GF_TRUE;
		} else {
			ctx->store_output = GF_FALSE;
			ctx->sidx_chunk_offset = (u32) (ctx->current_offset + ctx->current_size);
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
	GF_ISOStartFragmentFlags flags=0;

	//setup some default
	gf_isom_set_next_moof_number(ctx->file, ctx->msn);
	ctx->msn += ctx->msninc;

	if (ctx->moof_first) flags |= GF_ISOM_FRAG_MOOF_FIRST;
#ifdef GF_ENABLE_CTRN
	if (ctx->ctrn) flags |= GF_ISOM_FRAG_USE_COMPACT;
#endif

	e = gf_isom_start_fragment(ctx->file, flags);
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
		}
		if (e) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MP4Mux] Unable set fragment options: %s\n", gf_error_to_string(e) ));
		}
		tkw->fragment_done = GF_FALSE;
		tkw->insert_tfdt = (has_tfdt || ctx->tfdt_traf) ? GF_TRUE : ctx->insert_tfdt;
		tkw->dur_in_frag = 0;

		if (ctx->trun_inter) {
			gf_isom_set_fragment_option(ctx->file, tkw->track_id, GF_ISOM_TRUN_SET_INTERLEAVE_ID, 0);
		}

		if (ctx->sdtp_traf)
			gf_isom_set_fragment_option(ctx->file, tkw->track_id, GF_ISOM_TRAF_USE_SAMPLE_DEPS_BOX, ctx->sdtp_traf);


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
	nb_read = (u32) gf_fread(output, blocksize, ctx->tmp_store);
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

static void mp4mx_frag_box_patch(GF_MP4MuxCtx *ctx)
{
	GF_Err e;
	u32 i, count = gf_list_count(ctx->tracks);
	for (i=0; i<count; i++) {
		const GF_PropertyValue *p;
		TrackWriter *tkw = gf_list_get(ctx->tracks, i);
		if (!tkw->track_id) continue;
		//no box patched set (todo, do we want to allow changing boxpatch props ?)
		if (!tkw->box_patched) continue;

		p = gf_filter_pid_get_property_str(tkw->ipid, "boxpatch");
		if (p && p->value.string) {
			e = gf_isom_apply_box_patch(ctx->file, tkw->track_id ? tkw->track_id : tkw->item_id, p->value.string, GF_TRUE);
			if (e) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MP4Mux] Unable to apply box patch %s to track fragment %d: %s\n",
					p->value.string, tkw->track_id, gf_error_to_string(e) ));
			}
		}
	}

	if (ctx->boxpatch) {
		e = gf_isom_apply_box_patch(ctx->file, 0, ctx->boxpatch, GF_TRUE);
		if (e) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MP4Mux] Unable to apply box patch %s to fragment: %s\n", ctx->boxpatch, gf_error_to_string(e) ));
		}
		ctx->box_patched = GF_TRUE;
	}
}


static GF_Err mp4_mux_initialize(GF_Filter *filter);

GF_Err mp4mx_reload_output(GF_Filter *filter, GF_MP4MuxCtx *ctx)
{
	GF_Err e;
	u32 i, count = gf_list_count(ctx->tracks);

	//done with the file
	if (ctx->file) {
		e = mp4_mux_done(filter, ctx, GF_FALSE);
		if (e) return e;
		ctx->file = NULL;
	}
	ctx->init_movie_done = GF_FALSE;
	e = mp4_mux_initialize(filter);
	if (e) return e;
	ctx->config_timing = GF_TRUE;

	for (i=0; i<count; i++) {
		TrackWriter *tkw = gf_list_get(ctx->tracks, i);
		tkw->suspended = GF_FALSE;
		tkw->track_num = 0;
		tkw->nb_samples = 0;
		e = mp4_mux_configure_pid(filter, tkw->ipid, GF_FALSE);
		if (e) return e;
		tkw->nb_samples = 0;
		tkw->sample.DTS = 0;
		tkw->sample.CTS_Offset = 0;
		tkw->samples_in_stsd = 0;
		tkw->samples_in_frag = 0;
	}
	assert(ctx->next_file_idx);
	ctx->cur_file_idx_plus_one = ctx->next_file_idx;
	ctx->next_file_idx = 0;
	ctx->notify_filename = GF_TRUE;
	assert(!ctx->cur_file_suffix);
	if (ctx->next_file_suffix) {
		ctx->cur_file_suffix = gf_strdup(ctx->next_file_suffix);
		ctx->next_file_suffix = NULL;
	}
	return GF_OK;
}


static GF_Err mp4_mux_process_fragmented(GF_Filter *filter, GF_MP4MuxCtx *ctx)
{
	GF_Err e = GF_OK;
	u32 nb_eos, nb_done, nb_suspended, i, count;

	if (ctx->flush_size) {
		return mp4_mux_flush_fragmented(filter, ctx);
	}

	if (!ctx->file)
		return GF_EOS;

	//init movie not yet produced
	if (!ctx->init_movie_done) {
		e = mp4_mux_initialize_movie(ctx);
		if (e) return e;
		if (!ctx->init_movie_done)
			return GF_OK;
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
			goto check_eos;

		ctx->segment_started = GF_TRUE;
		ctx->insert_tfdt = GF_TRUE;
		ctx->insert_pssh = (ctx->psshs == MP4MX_PSSH_MOOF) ? GF_TRUE : GF_FALSE;

		gf_isom_start_segment(ctx->file, ctx->single_file ? NULL : "_gpac_isobmff_redirect", GF_FALSE);
	}

	//process pid by pid
	nb_eos=0;
	nb_done = 0;
	nb_suspended = 0;
	for (i=0; i<count; i++) {
		u64 cts, dts, ncts;
		TrackWriter *tkw = gf_list_get(ctx->tracks, i);

		if (ctx->fragment_started && tkw->fragment_done) {
			nb_done ++;
			continue;
		}
		if (tkw->suspended) {
			if (ctx->fragment_started) nb_done++;
			nb_suspended++;
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
					if (ctx->next_file_idx)
						nb_suspended++;
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
				p = gf_filter_pck_get_property(pck, GF_PROP_PCK_EODS);
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

			//get dash/file segment number
			p = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENUM);

			//not dash and file end, we need to wait for all streams and resetup
			if (!ctx->dash_mode && p) {
				if (!ctx->cur_file_idx_plus_one) {
					ctx->cur_file_idx_plus_one = p->value.uint + 1;
					if (!ctx->cur_file_suffix) {
						p = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILESUF);
						if (p && p->value.string) ctx->cur_file_suffix = gf_strdup(p->value.string);
					}
					ctx->notify_filename = GF_TRUE;
				} else if (ctx->cur_file_idx_plus_one == p->value.uint+1) {
				} else if (!tkw->suspended) {
					tkw->suspended = GF_TRUE;
					nb_suspended++;
					ctx->next_file_idx =  p->value.uint + 1;
					p = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILESUF);
					if (p && p->value.string)
						ctx->next_file_suffix = p->value.string;
					break;
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
			} else if (ctx->fragdur && (!ctx->dash_mode || !tkw->fragment_done) ) {
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
				GF_FilterSAPType sap = mp4_mux_get_sap(ctx, pck);
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
				u64 odts = gf_filter_pck_get_dts(pck);
				if (odts==GF_FILTER_NO_TS)
					odts = gf_filter_pck_get_cts(pck);

				if (tkw->offset_dts) odts += tkw->offset_dts;

				tkw->insert_tfdt = GF_FALSE;
				gf_isom_set_traf_base_media_decode_time(ctx->file, tkw->track_id, odts);
				tkw->first_dts_in_seg = (u64) odts;
			}

			if (ctx->trun_inter) {
				GF_FilterSAPType sap = mp4_mux_get_sap(ctx, pck);
				s32 tid_group = 0;
				if (sap) {
					tkw->prev_tid_group = 0;
				} else {
					s64 dts_diff;
					s64 p_dts = gf_filter_pck_get_dts(pck);
					s64 p_cts = gf_filter_pck_get_cts(pck);
					s64 cts_o = p_cts - p_dts;
					dts_diff = p_dts - tkw->sample.DTS;
					tid_group = (s32) (cts_o / dts_diff);
					tid_group = 20 - tid_group;
					if (tid_group != tkw->prev_tid_group) {
						tkw->prev_tid_group = tid_group;
						gf_isom_set_fragment_option(ctx->file, tkw->track_id, GF_ISOM_TRUN_SET_INTERLEAVE_ID, tid_group);
					}
				}
			}

			//process packet
			e = mp4_mux_process_sample(ctx, tkw, pck, GF_TRUE);

			//discard
			gf_filter_pid_drop_packet(tkw->ipid);

			if (e) return e;
		}
		//done with this track - if single track per moof, request new fragment but don't touch the
		//fragmentation state of the track writers
		if (ctx->straf && (i+1 < count)) {
			GF_ISOStartFragmentFlags flags = 0;
			if (ctx->moof_first) flags |= GF_ISOM_FRAG_MOOF_FIRST;
#ifdef GF_ENABLE_CTRN
			if (ctx->ctrn) flags |= GF_ISOM_FRAG_USE_COMPACT;
#endif
			e = gf_isom_start_fragment(ctx->file, flags);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Unable to start new fragment: %s\n", gf_error_to_string(e) ));
				return e;
			}
			if (ctx->sdtp_traf)
				gf_isom_set_fragment_option(ctx->file, tkw->track_id, GF_ISOM_TRAF_USE_SAMPLE_DEPS_BOX, ctx->sdtp_traf);
		}
	}

	//all suspended tracks done, flush fragment
	if (nb_suspended && (nb_suspended==count)) {
		nb_done = count;
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

		mp4mx_frag_box_patch(ctx);

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

			e = gf_isom_close_segment(ctx->file, subs_sidx, track_ref_id, ctx->ref_tkw->first_dts_in_seg, ctx->ref_tkw->ts_delay, next_ref_ts, ctx->chain_sidx, ctx->ssix, ctx->sseg ? GF_FALSE : is_eos, GF_FALSE, ctx->eos_marker, &idx_start_range, &idx_end_range, &segment_size_in_bytes);
			if (e) return e;

			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MP4Mux] Done writing segment %d - estimated next fragment times start %g end %g\n", ctx->dash_seg_num, ref_start, ctx->next_frag_start ));

			if (ctx->dash_mode != MP4MX_DASH_VOD) {
				mp4_mux_flush_frag(ctx, GF_FALSE, offset + idx_start_range, idx_end_range ? offset + idx_end_range : 0);
			} else if (ctx->vodcache==MP4MX_VODCACHE_REPLACE) {
				mp4_mux_flush_frag(ctx, GF_FALSE, 0, 0);
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
				mp4_mux_flush_frag(ctx, GF_FALSE, 0, 0);

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

	if (nb_suspended && (nb_suspended==count)) {
		ctx->nb_segs=0;
		return mp4mx_reload_output(filter, ctx);
	}

check_eos:
	if (count == nb_eos) {
		if (ctx->dash_mode==MP4MX_DASH_VOD) {
			if (ctx->vodcache!=MP4MX_VODCACHE_ON) {
				ctx->final_sidx_flush = GF_TRUE;
				//flush SIDX in given space - this will reserve 8 bytes for free box if not fitting
				gf_isom_flush_sidx(ctx->file, ctx->sidx_max_size, ctx->sidx_size_exact);
			} else {
				u64 start_offset;
				//reenable packet dispatch
				ctx->store_output = GF_FALSE;
				gf_isom_flush_sidx(ctx->file, 0, GF_FALSE);
				//flush sidx packet
				mp4mux_send_output(ctx);

				mp4_mux_flush_frag(ctx, GF_TRUE, ctx->current_offset, ctx->current_offset + ctx->current_size - 1);

				ctx->flush_size = gf_ftell(ctx->tmp_store);
				ctx->flush_done = 0;
				gf_fseek(ctx->tmp_store, 0, SEEK_SET);

				if (ctx->seg_sizes) {
					start_offset = ctx->current_offset;
					for (i=0; i<ctx->nb_seg_sizes; i++) {
						ctx->current_size = ctx->seg_sizes[i];
						mp4_mux_flush_frag(ctx, GF_FALSE, 0, 0);
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

		mp4mux_send_output(ctx);

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
		if (!tkw->track_num) {
			if (gf_filter_pid_is_eos(tkw->ipid)) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MP4Mux] PID has no input packet and configuration not known after 10 retries, aborting initial timing sync\n"));
				continue;
			}
			return;
		}
		if (!pck) {
			if (gf_filter_pid_is_eos(tkw->ipid)) {
				if (tkw->cenc_state==CENC_NEED_SETUP)
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
		//we may have reorder tracks after the get_packet, redo
		if (gf_list_find(ctx->tracks, tkw) != i) {
			mp4_mux_config_timing(ctx);
			return;
		}
		ts = gf_filter_pck_get_dts(pck);
		if (ts==GF_FILTER_NO_TS)
			ts = gf_filter_pck_get_cts(pck);
		if (ts==GF_FILTER_NO_TS)
			ts=0;

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
		s64 dts_diff, dur;
		TrackWriter *tkw = gf_list_get(ctx->tracks, i);

		//compute offsets
		dts_diff = first_ts_min;
		dts_diff *= tkw->src_timescale;
		dts_diff /= 1000000;
		dts_diff = (s64) tkw->ts_shift - dts_diff;
		if (ctx->is_rewind) dts_diff = -dts_diff;
		//negative could happen due to rounding, ignore them
		if (dts_diff<=0) continue;

		// dts_diff > 0, we need to delay the track
		dur = dts_diff;
		dur *= (u32) ctx->moovts;
		dur /= tkw->src_timescale;
		if (dur) {
			gf_isom_remove_edits(ctx->file, tkw->track_num);

			gf_isom_set_edit(ctx->file, tkw->track_num, 0, dur, dts_diff, GF_ISOM_EDIT_EMPTY);
			gf_isom_set_edit(ctx->file, tkw->track_num, dur, 0, 0, GF_ISOM_EDIT_NORMAL);
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
			sprintf(szStatus, "%s", ((ctx->store==MP4MX_MODE_FLAT) || (ctx->store==MP4MX_MODE_FASTSTART)) ? "mux" : "import");
		}
		for (i=0; i<count; i++) {
			u32 pc=0;
			TrackWriter *tkw = gf_list_get(ctx->tracks, i);
			if (tkw->aborted) {
				pc=10000;
			} else if (ctx->idur.num) {
				if (ctx->idur.num>0) {
					u64 mdur = gf_isom_get_media_duration(ctx->file, tkw->track_num);
					u64 tk_done = mdur * ctx->idur.den;
					u64 tk_total = ((u64)tkw->tk_timescale) * ctx->idur.num;
					pc = (u32) ((tk_done*10000)/tk_total);
				} else {
					pc = (u32) ( (10000 * (u64) tkw->nb_samples) / (-ctx->idur.num) );
				}
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
					sprintf(szTK, " %d %%", pc/100);
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
	u32 nb_skip, nb_eos, nb_suspended, i, count = gf_list_count(ctx->tracks);
	nb_skip = 0;
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

	nb_suspended = 0;
	for (i=0; i<count; i++) {
		TrackWriter *tkw = gf_list_get(ctx->tracks, i);
		GF_FilterPacket *pck = gf_filter_pid_get_packet(tkw->ipid);

		if (tkw->suspended) {
			nb_suspended++;
			continue;
		}

		if (!pck) {
			if (gf_filter_pid_is_eos(tkw->ipid)) {
				tkw->suspended = GF_FALSE;
				nb_eos++;
			}
			continue;
		}

		if (tkw->aborted) {
			gf_filter_pid_drop_packet(tkw->ipid);
			nb_eos++;
			continue;
		}

		if (ctx->owns_mov) {
			const GF_PropertyValue *p;
			p = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENUM);
			if (p) {
				if (!ctx->cur_file_idx_plus_one) {
					ctx->cur_file_idx_plus_one = p->value.uint + 1;
					if (!ctx->cur_file_suffix) {
						p = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILESUF);
						if (p && p->value.string) ctx->cur_file_suffix = gf_strdup(p->value.string);
					}
					ctx->notify_filename = GF_TRUE;
				} else if (ctx->cur_file_idx_plus_one == p->value.uint+1) {
				} else if (!tkw->suspended) {
					tkw->suspended = GF_TRUE;
					nb_suspended++;
					ctx->next_file_idx =  p->value.uint + 1;
					p = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILESUF);
					if (p && p->value.string)
						ctx->next_file_suffix = p->value.string;
					continue;
				}
			}
		}

		//basic regulation in case we do on-the-fly interleaving
		//we need to regulate because sources do not produce packets at the same rate
		if ((ctx->store==MP4MX_MODE_FASTSTART) && ctx->cdur) {
			Double cts = (Double) gf_filter_pck_get_cts(pck);
			if (ctx->is_rewind)
				cts = tkw->ts_shift - cts;
			else
				cts -= tkw->ts_shift;
			cts /= tkw->src_timescale;
			if (!ctx->faststart_ts_regulate) {
				ctx->faststart_ts_regulate = ctx->cdur;
			}
			//ahead of our interleaving window, don't write yet
			else if (cts > ctx->faststart_ts_regulate) {
				nb_skip++;
				continue;
			}
		}

		if (tkw->cenc_state==CENC_NEED_SETUP)
			mp4_mux_cenc_update(ctx, tkw, pck, CENC_CONFIG, 0);

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

	if (nb_suspended && (nb_suspended+nb_eos==count)) {
		return mp4mx_reload_output(filter, ctx);
	}

	if (count == nb_eos) {
		if (ctx->file) {
			GF_Err e = mp4_mux_done(filter, ctx, GF_TRUE);
			if (e) return e;
		}
		return GF_EOS;
	}
	//done with this interleaving window, start next one
	else if (nb_skip + nb_eos == count) {
		ctx->faststart_ts_regulate += ctx->cdur;
	} else if (ctx->importer) {
		u64 prog_done=0, prog_total=0;
		for (i=0; i<count; i++) {
			TrackWriter *tkw = gf_list_get(ctx->tracks, i);
			if (prog_done * tkw->prog_total <= tkw->prog_done * prog_total) {
				prog_done = tkw->prog_done;
				prog_total = tkw->prog_total;
			}
		}
		gf_set_progress("Import", prog_done, prog_total);
	}

	return GF_OK;
}

static GF_Err mp4_mux_on_data_patch(void *cbk, u8 *data, u32 block_size, u64 file_offset, Bool is_insert)
{
	GF_Filter *filter = (GF_Filter *) cbk;
	u8 *output;
	GF_MP4MuxCtx *ctx = gf_filter_get_udta(filter);

	GF_FilterPacket *pck = gf_filter_pck_new_alloc(ctx->opid, block_size, &output);
	memcpy(output, data, block_size);
	gf_filter_pck_set_framing(pck, GF_FALSE, GF_FALSE);
	gf_filter_pck_set_seek_flag(pck, GF_TRUE);
	if (is_insert)
		gf_filter_pck_set_interlaced(pck, 1);
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
	mp4mux_send_output(ctx);

	if (ctx->final_sidx_flush) {
		GF_FilterPacket *pck;
		u32 free_size=0;

		if (ctx->vodcache==MP4MX_VODCACHE_INSERT) {
			pck = gf_filter_pck_new_alloc(ctx->opid, block_size, &output);
			memcpy(output, data, block_size);
			gf_filter_pck_set_framing(pck, GF_FALSE, GF_FALSE);
			gf_filter_pck_set_byte_offset(pck, ctx->sidx_chunk_offset);
			gf_filter_pck_set_seek_flag(pck, GF_TRUE);
			gf_filter_pck_set_interlaced(pck, 1);
			gf_filter_pck_send(pck);
		} else {
			GF_BitStream *bs;
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
			if (free_size) {
				gf_bs_write_u32(bs, free_size);
				gf_bs_write_u32(bs, GF_ISOM_BOX_TYPE_FREE);
				gf_bs_skip_bytes(bs, free_size-8);
			}
			gf_bs_write_data(bs, data, block_size);
			gf_bs_del(bs);
			gf_filter_pck_send(pck);
		}
		mp4_mux_flush_frag(ctx, GF_TRUE, ctx->sidx_chunk_offset+free_size, ctx->sidx_chunk_offset+free_size + block_size - 1);
		return GF_OK;
	}

	if (ctx->store_output) {
		u32 nb_write = (u32) gf_fwrite(data, block_size, ctx->tmp_store);
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
		mp4mux_send_output(ctx);
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
	gf_filter_set_max_extra_input_pids(filter, -1);

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

		if (ctx->store==MP4MX_MODE_FASTSTART) {
			gf_isom_set_storage_mode(ctx->file, GF_ISOM_STORE_FASTSTART);
		}

	}
	if (!ctx->moovts) ctx->moovts=600;

	if (ctx->mfra && (ctx->store>=MP4MX_MODE_FRAG)) {
		GF_Err e = gf_isom_enable_mfra(ctx->file);
		if (e) return e;
	}

	if (!ctx->tracks)
		ctx->tracks = gf_list_new();

#ifdef GF_ENABLE_CTRN
	if (ctx->ctrni)
		ctx->ctrn = GF_TRUE;
#endif

	if (ctx->m4cc) {
		if (strlen(ctx->m4cc)==4)
			ctx->eos_marker = GF_4CC(ctx->m4cc[0], ctx->m4cc[1], ctx->m4cc[2], ctx->m4cc[3]);
		else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MP4Mux] Invalid segment marker 4cc %s, ignoring\n", ctx->m4cc));
		}
	}
	if (ctx->compress)
		gf_isom_enable_compression(ctx->file, ctx->compress, ctx->fcomp);
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

	gf_isom_remove_edits(ctx->file, tkw->track_num);


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

			gf_isom_set_edit(ctx->file, tkw->track_num, 0, tkw->empty_init_dur, 0, GF_ISOM_EDIT_EMPTY);
			if (max_cts >= tkw->empty_init_dur) max_cts -= tkw->empty_init_dur;
			else max_cts = 0;
		}

		gf_isom_set_edit(ctx->file, tkw->track_num, tkw->empty_init_dur, max_cts, min_cts, GF_ISOM_EDIT_NORMAL);
	}
}

//todo: move from media_import.c to here once done
void gf_media_update_bitrate(GF_ISOFile *file, u32 track);


static void mp4_mux_set_hevc_groups(GF_MP4MuxCtx *ctx, TrackWriter *tkw)
{
	u32 avc_base_track, hevc_base_track, ref_track_id;
	avc_base_track = hevc_base_track = 0;
	u32 i;
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
	if ((!p || !p->value.uint) && (tkw->codecid!=GF_CODECID_LHVC)) {
		return;
	}
	//set linf
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

static GF_Err mp4_mux_done(GF_Filter *filter, GF_MP4MuxCtx *ctx, Bool is_final)
{
	GF_Err e = GF_OK;
	u32 i, count;
	GF_PropertyEntry *pe=NULL;

	count = gf_list_count(ctx->tracks);
	for (i=0; i<count; i++) {
		const GF_PropertyValue *p;
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
		} else if (tkw->ts_delay || tkw->empty_init_dur) {
			gf_isom_update_edit_list_duration(ctx->file, tkw->track_num);
		}

		if (ctx->importer && ctx->idur.num && ctx->idur.den) {
			u64 mdur = gf_isom_get_media_duration(ctx->file, tkw->track_num);
			u64 pdur = gf_isom_get_track_duration(ctx->file, tkw->track_num);
			if (pdur==mdur) {
				GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[MP4Mux] Imported %d frames - duration %g\n", tkw->nb_samples, ((Double)mdur)/tkw->tk_timescale ));
			} else {
				GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[MP4Mux] Imported %d frames - media duration %g - track duration %g\n", tkw->nb_samples, ((Double)mdur)/tkw->tk_timescale, ((Double)pdur)/ctx->moovts ));
			}
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
#ifndef GPAC_DISABLE_AV_PARSERS
					GF_ESD *esd = gf_isom_get_esd(ctx->file, tkw->track_num, tkw->stsd_idx);
					assert(esd);
					gf_m4v_rewrite_pl(&esd->decoderConfig->decoderSpecificInfo->data, &esd->decoderConfig->decoderSpecificInfo->dataLength, (u8) PL);
					gf_isom_change_mpeg4_description(ctx->file, tkw->track_num, tkw->stsd_idx, esd);
					gf_odf_desc_del((GF_Descriptor*)esd);
#endif

				}
				if (!ctx->make_qt)
					gf_isom_set_pl_indication(ctx->file, GF_ISOM_PL_VISUAL, PL);
			}
		}


		if (tkw->has_append)
			gf_isom_refresh_size_info(ctx->file, tkw->track_num);

		if ((tkw->nb_samples == 1) && (ctx->idur.num>0) && ctx->idur.den) {
			u32 dur = tkw->tk_timescale * ctx->idur.num;
			dur /= ctx->idur.den;
			gf_isom_set_last_sample_duration(ctx->file, tkw->track_num, dur);
		}

		if (tkw->has_open_gop) {
			if (ctx->importer) {
				GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("OpenGOP detected - adjusting file brand\n"));
			}
			gf_isom_modify_alternate_brand(ctx->file, GF_ISOM_BRAND_ISO6, GF_TRUE);
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
			u32 j, stsd_count = gf_isom_get_sample_description_count(ctx->file, tkw->track_num);
			p = gf_filter_pid_get_info(tkw->ipid, GF_PROP_PID_MAX_NALU_SIZE, &pe);
			msize = gf_get_bit_size(p->value.uint);
			if (msize<8) msize = 8;
			else if (msize<16) msize = 16;
			else msize = 32;

			if (msize<=0xFFFF) {
				for (j=0; j<stsd_count; j++) {
					u32 k = 8 * gf_isom_get_nalu_length_field(ctx->file, tkw->track_num, j+1);
					if (k > msize) {
						do_rewrite = GF_TRUE;
					}
				}
				if (do_rewrite) {
					GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[MP4Mux] Adjusting NALU SizeLength to %d bits\n", msize ));
					gf_media_nal_rewrite_samples(ctx->file, tkw->track_num, msize);
					msize /= 8;
					for (j=0; j<stsd_count; j++) {
						gf_isom_set_nalu_length_field(ctx->file, tkw->track_num, j+1, msize);
					}
				}
			}
		}

		//don't update bitrate info for single sample tracks, unless MPEG-4 Systems - compatibility with old arch
		if (ctx->btrt && !tkw->skip_bitrate_update && ((tkw->nb_samples>1) || ctx->m4sys) )
			gf_media_update_bitrate(ctx->file, tkw->track_num);

		if (!tkw->box_patched) {
			p = gf_filter_pid_get_property_str(tkw->ipid, "boxpatch");
			if (p && p->value.string) {
				e = gf_isom_apply_box_patch(ctx->file, tkw->track_id ? tkw->track_id : tkw->item_id, p->value.string, GF_FALSE);
				if (e) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Unable to apply box patch %s to track %d: %s\n",
						p->value.string, tkw->track_id, gf_error_to_string(e) ));
				}
			}
			tkw->box_patched = GF_TRUE;
		}
	}

	gf_filter_release_property(pe);

	if (ctx->boxpatch && !ctx->box_patched) {
		e = gf_isom_apply_box_patch(ctx->file, 0, ctx->boxpatch, GF_FALSE);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Unable to apply box patch %s: %s\n", ctx->boxpatch, gf_error_to_string(e) ));
		}
		ctx->box_patched = GF_TRUE;
	}


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
		case MP4MX_MODE_FASTSTART:
			e = gf_isom_set_storage_mode(ctx->file, GF_ISOM_STORE_FASTSTART);
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
		if (is_final)
			gf_filter_pid_set_eos(ctx->opid);
	} else {
		ctx->file = NULL;
	}
	return e;
}

static void mp4_mux_finalize(GF_Filter *filter)
{
	GF_MP4MuxCtx *ctx = gf_filter_get_udta(filter);

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

	if (ctx->cur_file_suffix) gf_free(ctx->cur_file_suffix);

}

static const GF_FilterCapability MP4MuxCaps[] =
{
	//for now don't accept files as input, although we could store them as items, to refine
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_SCENE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_OD),
	//we want framed media only
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	//and any codecid
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_NONE),
	CAP_STRING(GF_CAPS_OUTPUT_STATIC,  GF_PROP_PID_FILE_EXT, ISOM_FILE_EXT),
	CAP_STRING(GF_CAPS_OUTPUT_STATIC,  GF_PROP_PID_MIME, ISOM_FILE_MIME),
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
	{ OFFS(ctmode), "set composition offset mode for video tracks\n"
	"- edit: uses edit lists to shift first frame to presentation time 0\n"
	"- noedit: ignore edit lists and does not shift timeline\n"
	"- negctts: uses ctts v1 with possibly negative offsets and no edit lists", GF_PROP_UINT, "edit", "edit|noedit|negctts", GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(idur), "only import the specified duration. If negative, specify the number of coded frames to import", GF_PROP_FRACTION, "0", NULL, 0},
	{ OFFS(pack3gp), "pack a given number of 3GPP audio frames in one sample", GF_PROP_UINT, "1", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(importer), "compatibility with old importer, displays import progress", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(pack_nal), "repack NALU size length to minimum possible size for NALU-based video (AVC/HEVC/...)", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(xps_inband), "use inband (in sample data) param set for NALU-based video (AVC/HEVC/...)\n"
	"- no: paramater sets are not inband, several sample descriptions might be created\n"
	"- all: paramater sets are inband, no param sets in sample description\n"
	"- both: paramater sets are inband, signaled as inband, and also first set is kept in sample descripton\n"
	"- mix: creates non-standard files using single sample entry with first PSs found, and moves other PS inband", GF_PROP_UINT, "no", "no|all|both|mix|", 0},
	{ OFFS(store), "file storage mode\n"
	"- inter: perform precise interleave of the file using [-cdur]() (requires temporary storage of all media)\n"
	"- flat: write samples as they arrive and moov at end (fastest mode)\n"
	"- fstart: write samples as they arrive and moov before mdat\n"
	"- tight:  uses per-sample interleaving of all tracks (requires temporary storage of all media)\n"
	"- frag: fragments the file using cdur duration\n"
	"- sfrag: framents the file using cdur duration but adjusting to start with SAP1/3", GF_PROP_UINT, "inter", "inter|flat|fstart|tight|frag|sfrag", 0},
	{ OFFS(cdur), "chunk duration for interleaving and fragmentation modes\n"
	"- 0: no specific interleaving but moov first\n"
	"- negative: defaults to 1.0 unless overridden by storage profile", GF_PROP_DOUBLE, "-1.0", NULL, 0},
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
	{ OFFS(strun), "use a single trun per traf (smooth streaming and co)", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(psshs), "set PSSH boxes store mode\n"
	"- moof: in first moof of each segments\n"
	"- moov: in movie box\n"
	"- none: pssh is discarded", GF_PROP_UINT, "moov", "moov|moof|none", GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(sgpd_traf), "store sample group descriptions in traf (duplicated for each traf). If not used, sample group descriptions are stored in the movie box", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(vodcache), "enable temp storage for VoD dash modes - see filter help\n"
		"- on: use temp storage of complete file for sidx and ssix injection\n"
		"- insert: insert sidx and ssix by shifting bytes in output file\n"
		"- replace: precompute pace requirements for sidx and ssix and rewrite file range at end", GF_PROP_UINT, "replace", "on|insert|replace", 0},
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
	{ OFFS(sdtp_traf), "use sdtp in traf rather than using flags in trun sample entries\n"
		"- no: do not use sdtp\n"
		"- sdtp: use sdtp box to indicate sample dependencies and don't write info in trun sample flags\n"
		"- both: use sdtp box to indicate sample dependencies and also write info in trun sample flags", GF_PROP_UINT, "no", "no|sdtp|both", GF_FS_ARG_HINT_EXPERT},
	{ OFFS(trackid), "track ID of created track for single track. Default 0 uses next available trackID", GF_PROP_UINT, "0", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(fragdur), "fragment based on fragment duration rather than CTS. Mostly used for MP4Box -frag option", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(btrt), "set btrt box in sample description", GF_PROP_BOOL, "true", NULL, 0},
	{ OFFS(styp), "set segment styp major brand to the given 4CC[.version]", GF_PROP_STRING, NULL, NULL, 0},
	{ OFFS(mediats), "set media timescale. A value of 0 means inherit from pid, a value of -1 means derive from samplerate or frame rate", GF_PROP_SINT, "0", NULL, 0},
	{ OFFS(ase), "set audio sample entry mode for more than stereo layouts\n"\
			"- v0: use v0 signaling but channel count from stream, recommended for backward compatibility\n"\
			"- v0s: use v0 signaling and force channel count to 2 (stereo) if more than 2 channels\n"\
			"- v1: use v1 signaling, ISOBMFF style (will mux raw PCM as ISOBMFF style)\n"\
			"- v1qt: use v1 signaling, QTFF style"\
		, GF_PROP_UINT, "v0", "|v0|v0s|v1|v1qt", 0},
	{ OFFS(ssix), "create ssix when sidx is present, level 1 mappping I-frames byte ranges, level 0xFF mapping the rest", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(ccst), "insert coding constraint box for video tracks", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(maxchunk), "set max chunk size in bytes for runs (only used in non-fragmented mode). 0 means no constraints", GF_PROP_UINT, "0", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(noroll), "disable roll sample grouping", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(saio32), "set single segment mode for dash", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
#ifdef GF_ENABLE_CTRN
	{ OFFS(ctrn), "use compact track run (experimental)", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(ctrni), "use inheritance in compact track run for HEVC tile tracks (highly experimental)", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
#endif
	{ OFFS(sseg), "set single segment mode for dash", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_HIDE},

	{ OFFS(compress), "set top-level box compression mode\n"
						"- no: disable box compression\n"
						"- moov: compress only moov box\n"
						"- moof: compress only moof boxes\n"
						"- sidx: compress moof and sidx boxes\n"
						"- ssix: compress moof, sidx and ssix boxes\n"
						"- all: compress moov, moof, sidx and ssix boxes", GF_PROP_UINT, "no", "no|moov|moof|sidx|ssix|all", GF_FS_ARG_HINT_EXPERT},
	{ OFFS(fcomp), "force using compress box even when compressed size is larger than uncompressed", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},

	{ OFFS(trun_inter), "interleave samples in trun based on the temporal level, the lowest level are stored first - this will create as many trun as required", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(block_size), "target output block size, 0 for default internal value (10k)", GF_PROP_UINT, "10000", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(boxpatch), "apply box patch before writing", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(deps), "add samples dependencies information", GF_PROP_BOOL, "true", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(mfra), "enable movie fragment random access when fragmenting (ignored when dashing)", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(forcesync), "force all SAP types to be considered sync samples (might produce non-conformant files)", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(tags), "tag injection mode\n"
			"- none: do not inject tags\n"
			"- strict: only inject recognized itunes tags\n"
			"- all: inject all possible tags"
			, GF_PROP_UINT, "strict", "none|strict|all", GF_FS_ARG_HINT_EXPERT},
	{0}
};


GF_FilterRegister MP4MuxRegister = {
	.name = "mp4mx",
	GF_FS_SET_DESCRIPTION("ISOBMFF/QT muxer")
	GF_FS_SET_HELP("Muxes file according to ISOBMFF (14496-12 and derived specifications) or QuickTime\n"
	"  \n"
	"# Tracks and Items\n"
	"By default all input PIDs with ItemID property set are muxed as items, otherwise they are muxed as tracks.\n"
	"To prevent source items to be muxed as items, use [-itemid](mp4dmx) option from ISOBMF demuxer.\n"
	"EX -i source.mp4:itemid=false -o file.mp4\n"
	"  \n"
	"To force non-item streams to be muxed as items, use __#ItemID__ option on that PID:\n"
	"EX -i source.jpg:#ItemID=1 -o file.mp4\n"
	"  \n"
	"# Storage\n"
	"The [-store]() option allows controling if the file is fragmented ot not, and when not fragmented, how interleaving is done. For cases where disk requirements are tight and fragmentation cannot be used, it is recommended to use either `flat` or `fstart` modes.\n"
	"  \n"
	"The [-vodcache]() option allows controling how DASH onDemand segments are generated:\n"
	"- If set to `on`, file data is stored to a temporary file on disk and flushed upon completion, no padding is present.\n"
	"- If set to `insert`, SIDX/SSIX will be injected upon completion of the file by shifting bytes in file. In this case, no padding is required but this might not be compatible with all output sinks and will take longer to write the file.\n"
	"- If set to `replace`, SIDX/SSIX size will be estimated based on duration and DASH segment length, and padding will be used in the file __before__ the final SIDX. If input pids have the properties `DSegs` set, this will be as the number of segments.\n"
	"The `on` and `insert` modes will produce exactly the same file, while the mode `replace` may inject a `free` box before the sidx.\n"
	"  \n"
	"# Custom boxes\n"
	"Custom boxes can be specified as box patches:\n"
	"For movie-level patch, the [-boxpatch]() option of the filter should be used.\n"
	"Per PID box patch can be specified through the PID property `boxpatch`.\n"
	"EX src=source:#boxpatch=myfile.xml dst=mux.mp4\n"
	"Per Item box patch can be specified through the PID property `boxpatch`.\n"
	"EX src=source:1ItemID=1:#boxpatch=myfile.xml dst=mux.mp4\n"
	"  \n"
	"The box patch is applied before writing the initial moov box in fragmented mode, or when writing the complete file otherwise.\n"
	"The box patch can either be a filename or the full XML string.\n"
	"  \n"
	"# Tagging\n"
	"When tagging is enabled, the filter will watch the property `CoverArt` and all custom properties on incoming pid.\n"
	"The built-in tag names are `album`, `artist`, `comment`, `complilation`, `composer`, `year`, `disk`, `tool`, `genre`, `contentgroup`, `title`, `tempo`, `track`, `tracknum`, `writer`, `encoder`, `album_artist`, `gapless`, `conductor`.\n"
	"Other tag class may be specified using `tag_NAME` property names, and will be added if [-tags]() is set to `all` using:\n"
	"- `NAME` as a box 4CC if `NAME` is four characters long\n"
	"- the CRC32 of the `NAME` as a box 4CC if `NAME` is not four characters long\n"
	"  \n"
	"# Notes\n"
	"The filter watches the property `FileNumber` on incoming packets to create new files or new segments in DASH mode.\n"
	)
	.private_size = sizeof(GF_MP4MuxCtx),
	.args = MP4MuxArgs,
	.initialize = mp4_mux_initialize,
	.finalize = mp4_mux_finalize,
	.flags = GF_FS_REG_DYNAMIC_REDIRECT,
	SETCAPS(MP4MuxCaps),
	.configure_pid = mp4_mux_configure_pid,
	.process = mp4_mux_process,
	.process_event = mp4_mux_process_event
};


const GF_FilterRegister *mp4_mux_register(GF_FilterSession *session)
{
	return &MP4MuxRegister;
}

#else
const GF_FilterRegister *mp4_mux_register(GF_FilterSession *session)
{
	return NULL;
}
#endif // GPAC_DISABLE_ISOM_WRITE

