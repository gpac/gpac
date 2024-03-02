/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2018-2023
 *					All rights reserved
 *
 *  This file is part of GPAC / MPEG-2 TS mux filter
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
#include <gpac/mpegts.h>
#include <gpac/iso639.h>
#include <gpac/webvtt.h>

#if !defined(GPAC_DISABLE_MPEG2TS_MUX) || !defined(GPAC_DISABLE_MP4MX)

void mux_assign_mime_file_ext(GF_FilterPid *ipid, GF_FilterPid *opid, const char *file_exts, const char *mime_types, const char *def_ext)
{
	Bool found=GF_FALSE;
	const GF_PropertyValue *p;

	p = gf_filter_pid_get_property(ipid, GF_PROP_PID_FILE_EXT);
	if (p) {
		char *match = strstr(file_exts, p->value.string);
		if (match) {
			u32 slen = (u32) strlen(match);
			if (!match[slen-1] || (match[slen-1]=='|'))
				found = GF_TRUE;
		}
	}
	if (!found)
		gf_filter_pid_set_property(opid, GF_PROP_PID_FILE_EXT, &PROP_STRING(def_ext ? (char *)def_ext : "*") );

	p = gf_filter_pid_get_property(ipid, GF_PROP_PID_MIME);
	found = GF_FALSE;
	if (p) {
		char *match = strstr(mime_types, p->value.string);
		if (match) {
			u32 slen = (u32) strlen(match);
			if (!match[slen-1] || (match[slen-1]=='|'))
				found = GF_TRUE;
		}
	}
	if (!found)
		gf_filter_pid_set_property(opid, GF_PROP_PID_MIME, &PROP_STRING("*") );
}
#endif


#ifndef GPAC_DISABLE_MPEG2TS_MUX


#define M2TS_FILE_EXTS "ts|m2t|mts|dmb|trp"
#define M2TS_MIMES "video/mpeg-2|video/mp2t|video/mpeg|audio/mp2t"




enum
{
	TEMI_TC64_AUTO=0,
	TEMI_TC64_OFF,
	TEMI_TC64_ALWAYS,
};


typedef struct
{
	u64 sap_time;
	u64 offset;
	u32 nb_pck;
	u32 sap_type;
	u64 min_pts_plus_one;
	u64 max_pts;
} TS_SIDX;

typedef struct
{
	//filter args
	u32 pmt_id, pmt_rate, pmt_version, sdt_rate, breq, mpeg4;
	u64 pcr_offset, first_pts;
	u32 rate, pat_rate, repeat_rate, repeat_img, max_pcr, nb_pack, sid, bifs_pes;
	GF_M2TS_PackMode pes_pack;
	Bool flush_rap, realtime, pcr_only, disc, latm;
	s64 pcr_init;
	char *name, *provider, *temi;
	u32 log_freq;
	s32 subs_sidx;
	Bool keepts;
	GF_Fraction cdur;

	//internal
	GF_FilterPid *opid;
	GF_FilterPid *idx_opid;
	GF_Filter *idx_filter;
	GF_M2TS_Mux *mux;

	GF_List *pids;

	Bool check_pcr;
	Bool update_mux;
	char *pack_buffer;
	u64 nb_pck;
	Bool init_buffering;
	u32 last_log_time;
	Bool pmt_update_pending;

	//SCTE-35 data is conveyed by packet properties
	GF_M2TS_Mux_Stream *scte35_stream;
	u8 *scte35_payload;
	u32 scte35_size;

	u32 dash_mode;
	Bool init_dash;
	u32 dash_seg_num;
	Bool wait_dash_flush, last_is_eods_flush;
	Bool dash_file_switch;
	Bool next_is_start;
	u32 nb_pck_in_seg;
	u64 pck_start_idx;

	char dash_file_name[GF_MAX_PATH];
	char idx_file_name[GF_MAX_PATH];

	//dash indexing
	u32 nb_sidx_entries, nb_sidx_alloc;
	TS_SIDX *sidx_entries;
	GF_BitStream *idx_bs;
	u32 nb_pck_in_file, nb_pck_first_sidx;
	u64 total_bytes_in;

	u32 nb_suspended, cur_file_idx_plus_one;
	char *cur_file_suffix;
	Bool notify_filename;

	Bool is_playing;
	Double start_range;

	struct __tsmx_pid *ref_pid;
	Bool wait_llhls_flush;

	u32 llhls;
	Bool next_is_llhls_start;
	u32 frag_num;

	u32 frag_offset, frag_size, frag_duration;
	Bool frag_has_intra;

	Bool force_seg_sync;
	u32 pending_packets;

	u32 sync_init_time;
	GF_Fraction64 dash_seg_start;
} GF_TSMuxCtx;

typedef struct
{
	u32 id, delay, timescale;
	u64 offset, init_val;
	char *url;
	Bool ntp, use_init_val;
	u32 mode_64bits;
	u64 cts_at_init_val_plus_one;
} TEMIDesc;

enum
{
	M2TS_EODS_NO=0,
	//regular end of DASH segment (found new segment start packet)
	M2TS_EODS_FOUND,
	//forced end of DASH segment (found empty packet with EODS set)
	M2TS_EODS_FORCED,
	//end of LL-HLS part
	M2TS_EODS_LLHLS,
};

typedef struct __tsmx_pid
{
	GF_ESInterface esi;
	GF_FilterPid *ipid;
	GF_M2TS_Mux_Stream *mstream;
	GF_M2TS_Mux_Program *prog;

	u32 sid;
	u32 codec_id;
	u32 pmt_pid;
	u32 nb_pck;
	Bool is_repeat;
	u32 nb_repeat_last;

	GF_TSMuxCtx *ctx;
	u32 last_cv;
	//ts media skip
	s64 media_delay, max_media_skip;
	Bool done;

	GF_List *temi_descs;

	u32 last_temi_url;
	u8 *af_data;
	u32 af_data_alloc;
	GF_BitStream *temi_af_bs;

	Bool rewrite_odf;
	u32 has_seen_eods;
	u32 pck_duration;

	u64 loop_ts_offset;
	u64 last_dts;
	u32 last_dur;

	u8 *pck_data_buf;

	u32 suspended;
	u64 llhls_dts_init;
	Bool is_sparse;
} M2Pid;

static GF_Err tsmux_format_af_descriptor(GF_BitStream *bs, u32 timeline_id, u64 timecode, u32 timescale, u32 mode_64bits, u64 ntp, const char *temi_url, u32 temi_delay, u32 *last_url_time)
{
	u32 len;
	u32 last_time;
	if (ntp) {
		last_time = 1000*(ntp>>32);
		last_time += 1000*(ntp&0xFFFFFFFF)/0xFFFFFFFF;
	} else {
		last_time = (u32) (1000*timecode/timescale);
	}
	if (temi_url && (!*last_url_time || (last_time - *last_url_time + 1 >= temi_delay)) ) {
		u64 start = gf_bs_get_position(bs);
		u64 end;
		*last_url_time = last_time + 1;
		len = 0;
		gf_bs_write_int(bs,	GF_M2TS_AFDESC_LOCATION_DESCRIPTOR, 8);
		gf_bs_write_int(bs,	len, 8);

		gf_bs_write_int(bs,	0, 1); //force_reload
		gf_bs_write_int(bs,	0, 1); //is_announcement
		gf_bs_write_int(bs,	0, 1); //splicing_flag
		gf_bs_write_int(bs,	0, 1); //use_base_temi_url
		gf_bs_write_int(bs,	0xFF, 5); //reserved
		gf_bs_write_int(bs,	timeline_id, 7); //timeline_id

		if (strlen(temi_url)) {
			char *url = (char *)temi_url;
			if (!strnicmp(temi_url, "http://", 7)) {
				gf_bs_write_int(bs,	1, 8); //url_scheme
				url = (char *) temi_url + 7;
			} else if (!strnicmp(temi_url, "https://", 8)) {
				gf_bs_write_int(bs,	2, 8); //url_scheme
				url = (char *) temi_url + 8;
			} else {
				gf_bs_write_int(bs,	0, 8); //url_scheme
			}
			gf_bs_write_u8(bs, (u32) strlen(url)); //url_path_len
			gf_bs_write_data(bs, url, (u32) strlen(url) ); //url
			gf_bs_write_u8(bs, 0); //nb_addons
		}
		//rewrite len
		end = gf_bs_get_position(bs);
		len = (u32) (end - start - 2);
		gf_bs_seek(bs, start+1);
		gf_bs_write_int(bs,	len, 8);
		gf_bs_seek(bs, end);
	}

	if (timescale || ntp) {
		Bool use64;
		if (mode_64bits==TEMI_TC64_AUTO) {
			use64 = (timecode > 0xFFFFFFFFUL) ? GF_TRUE : GF_FALSE;
		} else if (mode_64bits==TEMI_TC64_ALWAYS) {
			use64 = GF_TRUE;
		} else {
			use64 = GF_FALSE;
			while (timecode > 0xFFFFFFFFUL) {
				timecode -= 0xFFFFFFFFUL;
			}

		}

		len = 3; //3 bytes flags

		if (timescale) len += 4 + (use64 ? 8 : 4);
		if (ntp) len += 8;

		//write timeline descriptor
		gf_bs_write_int(bs,	GF_M2TS_AFDESC_TIMELINE_DESCRIPTOR, 8);
		gf_bs_write_int(bs,	len, 8);

		gf_bs_write_int(bs,	timescale ? (use64 ? 2 : 1) : 0, 2); //has_timestamp
		gf_bs_write_int(bs,	ntp ? 1 : 0, 1); //has_ntp
		gf_bs_write_int(bs,	0, 1); //has_ptp
		gf_bs_write_int(bs,	0, 2); //has_timecode
		gf_bs_write_int(bs,	0, 1); //force_reload
		gf_bs_write_int(bs,	0, 1); //paused
		gf_bs_write_int(bs,	0, 1); //discontinuity
		gf_bs_write_int(bs,	0xFF, 7); //reserved
		gf_bs_write_int(bs,	timeline_id, 8); //timeline_id
		if (timescale) {
			gf_bs_write_u32(bs,	timescale); //timescale
			if (use64)
				gf_bs_write_u64(bs,	timecode); //timestamp
			else
				gf_bs_write_u32(bs,	(u32) timecode); //timestamp
		}
		if (ntp) {
			gf_bs_write_u64(bs,	ntp); //ntp
		}
	}
	return GF_OK;
}


GF_SLConfig *tsmux_get_sl_config(GF_TSMuxCtx *ctx, u32 timescale, GF_SLConfig *slc)
{
	if (!slc) slc = (GF_SLConfig *) gf_odf_desc_new(GF_ODF_SLC_TAG);
	slc->predefined = 0;
	slc->useAccessUnitStartFlag = 1;
	slc->useAccessUnitEndFlag = 1;
	slc->useRandomAccessPointFlag = 1;
	slc->useTimestampsFlag = 1;
	slc->timestampLength = 33;
	slc->timestampResolution = timescale;

	/*test mode in which time stamps are 90khz and not coded but copied over from PES header*/
	if (ctx->bifs_pes==2) {
		slc->timestampLength = 0;
		slc->timestampResolution = 90000;
	}
	return slc;
}

static void tsmux_rewrite_odf(GF_TSMuxCtx *ctx, GF_ESIPacket *es_pck)
{
	u32 com_count, com_index, od_count, esd_index, od_index;
	GF_ODUpdate *odU;
	GF_ESDUpdate *esdU;
	GF_ESD *esd;
	GF_ODCodec *od_codec = gf_odf_codec_new();

	gf_odf_codec_set_au(od_codec, es_pck->data, es_pck->data_len);
	gf_odf_codec_decode(od_codec);
	com_count = gf_list_count(od_codec->CommandList);
	for (com_index = 0; com_index < com_count; com_index++) {
		GF_ODCom *com = (GF_ODCom *)gf_list_get(od_codec->CommandList, com_index);
		switch (com->tag) {
		case GF_ODF_OD_UPDATE_TAG:
			odU = (GF_ODUpdate*)com;
			od_count = gf_list_count(odU->objectDescriptors);
			for (od_index=0; od_index<od_count; od_index++) {
				GF_ObjectDescriptor *od = (GF_ObjectDescriptor *)gf_list_get(odU->objectDescriptors, od_index);
				esd_index = 0;
				while ( (esd = gf_list_enum(od->ESDescriptors, &esd_index)) ) {
					gf_assert(esd->slConfig);
					esd->slConfig = tsmux_get_sl_config(ctx, esd->slConfig->timestampResolution, esd->slConfig);
				}
			}
			break;
		case GF_ODF_ESD_UPDATE_TAG:
			esdU = (GF_ESDUpdate*)com;
			esd_index = 0;
			while ( (esd = gf_list_enum(esdU->ESDescriptors, &esd_index)) ) {
					gf_assert(esd->slConfig);
					esd->slConfig = tsmux_get_sl_config(ctx, esd->slConfig->timestampResolution, esd->slConfig);
			}
			break;
		}
	}
	gf_odf_codec_encode(od_codec, 1);
	es_pck->data = NULL;
	es_pck->data_len = 0;
	gf_odf_codec_get_au(od_codec, &es_pck->data, &es_pck->data_len);
	gf_odf_codec_del(od_codec);

}

static void tsmux_check_mpd_start_time(GF_TSMuxCtx *ctx, GF_FilterPacket *pck)
{
	const GF_PropertyValue *p = gf_filter_pck_get_property(pck, GF_PROP_PCK_MPD_SEGSTART);
	if (p) {
		ctx->dash_seg_start = p->value.lfrac;
	} else {
		ctx->dash_seg_start.num = 0;
		ctx->dash_seg_start.den = 0;
	}
}

void gf_m2ts_mux_table_update(GF_M2TS_Mux_Stream *stream, u8 table_id, u16 table_id_extension,
                              u8 *table_payload, u32 table_payload_length,
                              Bool use_syntax_indicator, Bool private_indicator,
                              Bool use_checksum);
static u32 tsmux_stream_process_scte35(GF_M2TS_Mux *muxer, GF_M2TS_Mux_Stream *stream)
{
	if (stream->table_needs_update) { /* generate table payload */
		GF_TSMuxCtx *ctx = ((M2Pid*)stream->ifce->input_udta)->ctx;
		gf_m2ts_mux_table_update(stream, GF_M2TS_TABLE_ID_SCTE35_SPLICE_INFO, stream->program->number, ctx->scte35_payload, ctx->scte35_size, GF_FALSE, GF_FALSE, GF_FALSE);
		ctx->scte35_size = 0;
		gf_free(ctx->scte35_payload);
		ctx->scte35_payload = NULL;

		stream->table_needs_update = GF_FALSE;
		stream->table_needs_send = GF_TRUE;
	}
	if (stream->table_needs_send)
		return 1;
	if (stream->refresh_rate_ms)
		return 1;
	return 0;
}

static GF_Err tsmux_esi_ctrl(GF_ESInterface *ifce, u32 act_type, void *param)
{
	u32 cversion;
	u64 cts, cts_diff;
	M2Pid *tspid = (M2Pid *)ifce->input_udta;
	if (!tspid) return GF_BAD_PARAM;

	switch (act_type) {
	case GF_ESI_INPUT_DATA_FLUSH:
	{
		u64 dts;
		GF_ESIPacket es_pck;
		const GF_PropertyValue *p;
		GF_FilterPacket *pck;
		pck = gf_filter_pid_get_packet(tspid->ipid);
		//if PMT update is pending after this packet fetch (reconfigure), don't send the packet
		if (tspid->ctx->pmt_update_pending) return GF_OK;

		if (!pck) {
			if (gf_filter_pid_is_eos(tspid->ipid)) {
				if (tspid->ctx->realtime
					&& tspid->ctx->repeat_img
					&& (tspid->nb_pck==1)
					&& (tspid->esi.stream_type==GF_STREAM_VISUAL)
				) {
					tspid->nb_repeat_last++;
					tspid->is_repeat = GF_TRUE;
				} else {
					if (!gf_filter_pid_is_flush_eos(tspid->ipid)) {
						tspid->done = GF_TRUE;
						ifce->caps |= GF_ESI_STREAM_IS_OVER;
					}
				}
				if (tspid->ctx->dash_mode)
					tspid->has_seen_eods = M2TS_EODS_FOUND;
			}
			if (tspid->is_sparse) ifce->caps |= GF_ESI_STREAM_SPARSE;
			return GF_OK;
		}
		if (tspid->is_sparse) ifce->caps &= ~GF_ESI_STREAM_SPARSE;

		p = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENUM);
		if (tspid->ctx->dash_mode) {

			if (tspid->has_seen_eods) {
				ifce->caps |= GF_ESI_STREAM_FLUSH;
				return GF_OK;
			}
			ifce->caps &= ~GF_ESI_STREAM_FLUSH;

			//detect segment change
			if (p && tspid->ctx->dash_seg_num && (tspid->ctx->dash_seg_num < p->value.uint)) {
				if (!tspid->ctx->last_is_eods_flush)
					tspid->ctx->wait_dash_flush = GF_TRUE;
				tspid->ctx->dash_seg_num = p->value.uint;
				tspid->ctx->dash_file_name[0] = 0;
				tsmux_check_mpd_start_time(tspid->ctx, pck);
			}

			//segment change is pending, check for filename as well - we don't do that in the previous test
			//since the filename property is sent on a single pid, not each of them
			if (p && (tspid->ctx->dash_seg_num == p->value.uint)) {
				if (tspid->ctx->wait_dash_flush) {
					tspid->has_seen_eods = M2TS_EODS_FOUND;

					p = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENAME);
					if (p) {
						strcpy(tspid->ctx->dash_file_name, p->value.string);
						tspid->ctx->dash_file_switch = GF_TRUE;
					}
					return GF_OK;
				}
				else if (tspid->ctx->last_is_eods_flush) {
					const GF_PropertyValue *p2 = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENAME);
					if (p2) {
						strcpy(tspid->ctx->dash_file_name, p2->value.string);
						tspid->ctx->next_is_start = GF_TRUE;
					}
				}
			}

			//at this point the new segment is started
			if (p)
				tspid->ctx->dash_seg_num = p->value.uint;

			p = gf_filter_pck_get_property(pck, GF_PROP_PCK_IDXFILENAME);
			if (p) {
				strcpy(tspid->ctx->idx_file_name, p->value.string);
			}

			p = gf_filter_pck_get_property(pck, GF_PROP_PCK_EODS);
			if (p && p->value.boolean) {
				tspid->has_seen_eods = M2TS_EODS_FORCED;
				tspid->ctx->wait_dash_flush = GF_TRUE;
				gf_filter_pid_drop_packet(tspid->ipid);
				return GF_OK;
			}
		} else if (p) {
			if (!tspid->ctx->cur_file_idx_plus_one) {
				tspid->ctx->cur_file_idx_plus_one = p->value.uint + 1;
				if (!tspid->ctx->cur_file_suffix) {
					p = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILESUF);
					if (p && p->value.string) tspid->ctx->cur_file_suffix = gf_strdup(p->value.string);
				}
				tspid->ctx->notify_filename = GF_TRUE;
			} else if (tspid->ctx->cur_file_idx_plus_one == p->value.uint+1) {
			} else if (tspid->suspended) {
				return GF_OK;
			} else if (!tspid->ctx->notify_filename) {
				tspid->suspended = GF_TRUE;
				tspid->ctx->nb_suspended++;
				if (tspid->ctx->nb_suspended==1) {
					tspid->ctx->cur_file_idx_plus_one = p->value.uint + 1;
					p = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILESUF);
					if (p && p->value.string && !tspid->ctx->cur_file_suffix)
						tspid->ctx->cur_file_suffix = gf_strdup(p->value.string);
				}
				ifce->caps |= GF_ESI_STREAM_IS_OVER;
				return GF_OK;
			} else {
				//notify filename pending
				return GF_OK;
			}
		}

		if (tspid->ctx->ref_pid) {
			dts = gf_filter_pck_get_dts(pck);

			if (!tspid->llhls_dts_init) {
				tspid->llhls_dts_init = dts+1;
				if (tspid->ctx->ref_pid == tspid) {
					if (gf_filter_pck_get_sap(pck))
						tspid->ctx->frag_has_intra = GF_TRUE;
				}
			} else {
				u64 dur = dts - tspid->llhls_dts_init + 1;
				if (gf_timestamp_greater_or_equal(dur, ifce->timescale, tspid->ctx->cdur.num, tspid->ctx->cdur.den)) {
					if (tspid->ctx->ref_pid == tspid) {
						tspid->ctx->wait_llhls_flush = GF_TRUE;
						tspid->ctx->frag_duration = (u32) dur;
					}
					tspid->has_seen_eods = M2TS_EODS_LLHLS;
					return GF_OK;
				}
				if (tspid->ctx->ref_pid == tspid) {
					tspid->ctx->frag_duration = (u32) dur + gf_filter_pck_get_duration(pck);
				}
			}
		}

		memset(&es_pck, 0, sizeof(GF_ESIPacket));
		es_pck.flags = GF_ESI_DATA_AU_START | GF_ESI_DATA_AU_END | GF_ESI_DATA_HAS_CTS;
		es_pck.sap_type = gf_filter_pck_get_sap(pck);
		tspid->pck_duration = gf_filter_pck_get_duration(pck);
		cversion = gf_filter_pck_get_carousel_version(pck);
		if (cversion+1 == tspid->last_cv) {
			es_pck.flags |= GF_ESI_DATA_REPEAT;
		}
		tspid->last_cv = cversion+1;

		if (tspid->is_repeat)
			es_pck.flags |= GF_ESI_DATA_REPEAT;

		cts = gf_filter_pck_get_cts(pck);
		dts = gf_filter_pck_get_dts(pck);
		if (dts != GF_FILTER_NO_TS) {
			dts += tspid->loop_ts_offset;
			//loop detected
			if (tspid->last_dts && (dts<tspid->last_dts)) {
				dts -= tspid->loop_ts_offset;
				tspid->loop_ts_offset = tspid->last_dts - dts + tspid->last_dur;
				dts += tspid->loop_ts_offset;
			}
		}
		if (cts != GF_FILTER_NO_TS) {
			cts += tspid->loop_ts_offset;
			es_pck.cts = cts;
		} else {
			es_pck.cts = 0;
		}

		if (tspid->temi_descs) {
			u32 i, count=gf_list_count(tspid->temi_descs);

			if (!tspid->temi_af_bs) tspid->temi_af_bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
			else gf_bs_reassign_buffer(tspid->temi_af_bs, tspid->af_data, tspid->af_data_alloc);

			for (i=0; i<count; i++) {
				TEMIDesc *temi = gf_list_get(tspid->temi_descs, i);
				u64 ntp=0;
				u32 timescale = ifce->timescale;
				u64 tc;

				if (temi->timescale && (temi->timescale!=timescale)) {
					timescale = temi->timescale;
				}
				if (temi->use_init_val) {
					if (!temi->cts_at_init_val_plus_one) temi->cts_at_init_val_plus_one = cts+1;

					tc = (cts - (temi->cts_at_init_val_plus_one-1));
					if (timescale != ifce->timescale) {
						tc = gf_timestamp_rescale(tc, ifce->timescale, temi->timescale);
					}
					tc += temi->init_val;
				} else {
					//we want media timeline
					if ((s64) cts + tspid->media_delay >= 0)
						tc = cts + tspid->media_delay;
					else
						tc = 0;

					if (timescale != ifce->timescale) {
						tc = gf_timestamp_rescale(tc, ifce->timescale, temi->timescale);
					}
				}

				if (temi->offset) {
					if (timescale != ifce->timescale)
						tc += (u64) gf_timestamp_rescale(temi->offset, 1000, ifce->timescale);
					else
						tc += temi->offset;
				}

				if (temi->ntp) {
					u32 sec, frac;
					gf_net_get_ntp(&sec, &frac);
					ntp = sec;
					ntp <<= 32;
					ntp |= frac;
				}
				tsmux_format_af_descriptor(tspid->temi_af_bs, temi->id, tc, timescale, temi->mode_64bits, ntp, temi->url, temi->delay, &tspid->last_temi_url);
			}
			gf_bs_get_content_no_truncate(tspid->temi_af_bs, &tspid->af_data, &es_pck.mpeg2_af_descriptors_size, &tspid->af_data_alloc);
			es_pck.mpeg2_af_descriptors = tspid->af_data;
		}
		es_pck.cts += tspid->max_media_skip + tspid->media_delay;

		p = gf_filter_pck_get_property_str(pck, "scte35");
		if (p) {
			gf_assert(tspid->ctx->scte35_stream);
			gf_assert(!tspid->ctx->scte35_payload);
			tspid->ctx->scte35_payload = gf_malloc(p->value.data.size);
			memcpy(tspid->ctx->scte35_payload, p->value.data.ptr, p->value.data.size);
			tspid->ctx->scte35_size = p->value.data.size;
			tspid->ctx->scte35_stream->table_needs_update = GF_TRUE;
			tspid->ctx->scte35_stream->table_needs_send = GF_TRUE;
		}

		if (tspid->nb_repeat_last) {
			es_pck.cts += tspid->nb_repeat_last * ifce->timescale * tspid->ctx->repeat_img / 1000;
		}

		cts_diff = 0;
		if (tspid->prog->cts_offset) {
			cts_diff = gf_timestamp_rescale(tspid->prog->cts_offset, 1000000, tspid->esi.timescale);

			es_pck.cts += cts_diff;
		}

		es_pck.dts = es_pck.cts;
		if (dts != GF_FILTER_NO_TS) {
			tspid->last_dts = dts;
			es_pck.dts = dts;
			es_pck.dts += tspid->max_media_skip + tspid->media_delay;

			if (es_pck.dts > es_pck.cts) {
				u64 diff;
				//we don't have reliable dts - double the diff should make sure we don't try to adjust too often
				diff = cts_diff = 2*(es_pck.dts - es_pck.cts);
				diff = gf_timestamp_rescale(diff, tspid->esi.timescale, 1000000);
				gf_assert(tspid->prog->cts_offset <= diff);
				tspid->prog->cts_offset += (u32) diff;

				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[M2TSMux] Packet CTS "LLU" is less than packet DTS "LLU", adjusting all CTS by %d / %d!\n", es_pck.cts, es_pck.dts, cts_diff, tspid->esi.timescale));

				es_pck.cts += cts_diff;
			}
			if (tspid->esi.stream_type!=GF_STREAM_VISUAL) {
				es_pck.dts += cts_diff;
			}

			if (es_pck.dts != es_pck.cts) {
				es_pck.flags |= GF_ESI_DATA_HAS_DTS;
			}
		}
		es_pck.data = (char *) gf_filter_pck_get_data(pck, &es_pck.data_len);
		es_pck.duration = gf_filter_pck_get_duration(pck);
		tspid->last_dur = es_pck.duration;

		if (tspid->rewrite_odf) {
			tsmux_rewrite_odf(tspid->ctx, &es_pck);
		}

		tspid->ctx->total_bytes_in += es_pck.data_len;

		if (tspid->pck_data_buf) gf_free(tspid->pck_data_buf);
		tspid->pck_data_buf = NULL;

		//drop formatting for TX3G
		if (tspid->codec_id == GF_CODECID_TX3G) {
			u16 len = es_pck.data[0];
			len<<=8;
			len |= es_pck.data[1];
			es_pck.data += 2;
			es_pck.data_len = len;
		}
		//serialize webvtt cue formatting for TX3G
		else if (tspid->codec_id == GF_CODECID_WEBVTT) {
#ifndef GPAC_DISABLE_VTT
			u32 i;
			u64 start_ts, end_ts;
			void webvtt_write_cue_bs(GF_BitStream *bs, GF_WebVTTCue *cue, Bool write_srt);
			GF_List *cues;
			GF_BitStream *bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

			start_ts = gf_timestamp_rescale(es_pck.cts, tspid->esi.timescale, 1000);
			end_ts = gf_timestamp_rescale(es_pck.cts + es_pck.duration, tspid->esi.timescale, 1000);

			cues = gf_webvtt_parse_cues_from_data(es_pck.data, es_pck.data_len, start_ts, end_ts);
			for (i = 0; i < gf_list_count(cues); i++) {
				GF_WebVTTCue *cue = (GF_WebVTTCue *)gf_list_get(cues, i);
				webvtt_write_cue_bs(bs, cue, GF_FALSE);
				gf_webvtt_cue_del(cue);
			}
			gf_list_del(cues);
			gf_bs_get_content(bs, &es_pck.data, &es_pck.data_len);
			gf_bs_del(bs);
			tspid->pck_data_buf = es_pck.data;
#endif
		}
		//for TTML we keep the entire payload as a PES packet

		tspid->nb_pck++;
		ifce->output_ctrl(ifce, GF_ESI_OUTPUT_DATA_DISPATCH, &es_pck);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[M2TSMux] PID %d: packet %d CTS "LLU"\n", tspid->esi.stream_id, tspid->nb_pck, es_pck.cts));

		//data is copied by muxer for now, should need rewrite to avoid un-needed allocations
		gf_filter_pid_drop_packet(tspid->ipid);

		if (tspid->rewrite_odf) {
			gf_free(es_pck.data);
		}

	}
	return GF_OK;

	case GF_ESI_INPUT_DESTROY:
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


void update_m4sys_info(GF_TSMuxCtx *ctx, GF_M2TS_Mux_Program *prog)
{
	GF_M2TS_Mux_Stream *stream = prog->streams;

	if (prog->iod) gf_odf_desc_del(prog->iod);
	prog->iod = gf_odf_desc_new(GF_ODF_IOD_TAG);
	while (stream) {
		M2Pid *tspid = (M2Pid *)stream->ifce->input_udta;
		const GF_PropertyValue *p = gf_filter_pid_get_property(tspid->ipid, GF_PROP_PID_IN_IOD);
		if (p && p->value.boolean) {
			GF_ESD *esd = gf_odf_desc_esd_new(0);
			esd->decoderConfig->objectTypeIndication = stream->ifce->codecid;
			esd->decoderConfig->streamType = stream->ifce->stream_type;
			esd->ESID = stream->ifce->stream_id;
			esd->dependsOnESID = stream->ifce->depends_on_stream;
			if (stream->ifce->decoder_config_size) {
				esd->decoderConfig->decoderSpecificInfo->data = gf_malloc(sizeof(char)*stream->ifce->decoder_config_size);
				memcpy(esd->decoderConfig->decoderSpecificInfo->data, stream->ifce->decoder_config, stream->ifce->decoder_config_size);
				esd->decoderConfig->decoderSpecificInfo->dataLength = stream->ifce->decoder_config_size;
			}
			tsmux_get_sl_config(ctx, stream->ifce->timescale, esd->slConfig);
			gf_list_add( ((GF_ObjectDescriptor *)prog->iod)->ESDescriptors, esd);
		}
		stream->ifce->sl_config = tsmux_get_sl_config(ctx, stream->ifce->timescale, stream->ifce->sl_config);
		stream = stream->next;
	}
}

static Bool tsmux_setup_esi(GF_TSMuxCtx *ctx, GF_M2TS_Mux_Program *prog, M2Pid *tspid, u32 stream_type)
{
	const GF_PropertyValue *p;
	GF_ESInterface bckp = tspid->esi;

	memset(&tspid->esi, 0, sizeof(GF_ESInterface));

	if (stream_type == GF_STREAM_ENCRYPTED) {
		p = gf_filter_pid_get_property(tspid->ipid, GF_PROP_PID_PROTECTION_SCHEME_TYPE);
		if (p && (p->value.uint == GF_HLS_SAMPLE_AES_SCHEME)) {
			p = gf_filter_pid_get_property(tspid->ipid, GF_PROP_PID_ORIG_STREAM_TYPE);
			if (p) {
				stream_type = p->value.uint;
				tspid->esi.caps |= GF_ESI_STREAM_HLS_SAES;
			}

		}
	}
	tspid->esi.stream_type = stream_type;

	p = gf_filter_pid_get_property(tspid->ipid, GF_PROP_PID_TIMESCALE);
	tspid->esi.timescale = p->value.uint;

	p = gf_filter_pid_get_property(tspid->ipid, GF_PROP_PID_DECODER_CONFIG);
	if (p) {
		tspid->esi.decoder_config = p->value.data.ptr;
		tspid->esi.decoder_config_size = p->value.data.size;
	}
	p = gf_filter_pid_get_property(tspid->ipid, GF_PROP_PID_ID);
	if (p) tspid->esi.stream_id = p->value.uint;

	p = gf_filter_pid_get_property(tspid->ipid, GF_PROP_PID_DEPENDENCY_ID);
	if (p) tspid->esi.depends_on_stream = p->value.uint;

	tspid->esi.codecid = tspid->codec_id;

	p = gf_filter_pid_get_property(tspid->ipid, GF_PROP_PID_LANGUAGE);
	if (p) {
		s32 idx = gf_lang_find(p->value.string);
		if (idx>=0) {
			const char *code = gf_lang_get_3cc(idx);
			if (code) tspid->esi.lang = GF_4CC(code[0], code[1], code[2], ' ');
		}
	}

	p = gf_filter_pid_get_property(tspid->ipid, GF_PROP_PID_DURATION);
	if (p && p->value.lfrac.den) {
		tspid->esi.duration = (Double) p->value.lfrac.num;
		if (tspid->esi.duration<0) tspid->esi.duration=-tspid->esi.duration;
		tspid->esi.duration /= p->value.lfrac.den;
	}
	p = gf_filter_pid_get_property(tspid->ipid, GF_PROP_PID_BITRATE);
	if (p) tspid->esi.bit_rate = p->value.uint;
	/*repeat rate in ms for carrouseling - 0 if no repeat*/

	tspid->esi.repeat_rate = ctx->repeat_rate;
	p = gf_filter_pid_get_property(tspid->ipid, GF_PROP_PID_CAROUSEL_RATE);
	if (p) tspid->esi.repeat_rate = p->value.uint;

	tspid->rewrite_odf = GF_FALSE;
	if (tspid->esi.stream_type==GF_STREAM_OD) {
		tspid->rewrite_odf = GF_TRUE;
		update_m4sys_info(ctx, prog);
	} else if (prog->iod) {
		update_m4sys_info(ctx, prog);
	}

	switch (tspid->esi.stream_type) {
	case GF_STREAM_AUDIO:
		if (ctx->latm && !(tspid->esi.caps & GF_ESI_STREAM_HLS_SAES))
			tspid->esi.caps |= GF_ESI_AAC_USE_LATM;
	case GF_STREAM_VISUAL:
		if (ctx->mpeg4==2) {
			tspid->esi.caps |= GF_ESI_STREAM_WITHOUT_MPEG4_SYSTEMS;
		}
		break;
	}

	p = gf_filter_pid_get_property(tspid->ipid, GF_PROP_PID_DOLBY_VISION);
	if (p && p->value.data.ptr && (p->value.data.size==24)) {
		memcpy(tspid->esi.dv_info, p->value.data.ptr, 24);
		if (! (tspid->esi.dv_info[3] & 0x1))
			tspid->esi.caps |= GF_ESI_FORCE_DOLBY_VISION;
	}

	p = gf_filter_pid_get_property(tspid->ipid, GF_PROP_PID_ISOM_SUBTYPE);
	if (p && (((p->value.uint>>24) & 0xFF) == 'd') && (((p->value.uint>>16) & 0xFF) == 'v'))
		tspid->esi.caps |= GF_ESI_FORCE_DOLBY_VISION;

	tspid->esi.input_ctrl = tsmux_esi_ctrl;
	tspid->esi.input_udta = tspid;
	tspid->prog = prog;

	tspid->esi.output_ctrl = bckp.output_ctrl;
	tspid->esi.output_udta = bckp.output_udta;

	Bool changed = GF_FALSE;
	if (bckp.stream_id != tspid->esi.stream_id) changed = GF_TRUE;
	else if (bckp.codecid != tspid->esi.codecid) changed = GF_TRUE;
	else if (bckp.depends_on_stream != tspid->esi.depends_on_stream) changed = GF_TRUE;
	else if (bckp.lang != tspid->esi.lang) changed = GF_TRUE;
	else if (bckp.decoder_config && !tspid->esi.decoder_config) changed = GF_TRUE;
	else if (!bckp.decoder_config && tspid->esi.decoder_config) changed = GF_TRUE;
	else if (bckp.decoder_config && tspid->esi.decoder_config &&
		( (bckp.decoder_config_size != tspid->esi.decoder_config_size)
		|| memcmp(bckp.decoder_config, tspid->esi.decoder_config, bckp.decoder_config_size))
	)
		changed = GF_TRUE;
	else if (memcmp(bckp.dv_info, tspid->esi.dv_info, 24))
		changed = GF_TRUE;

	else if ((tspid->esi.caps & GF_ESI_STREAM_HLS_SAES) != (bckp.caps & GF_ESI_STREAM_HLS_SAES))
		changed = GF_TRUE;
	else if ((tspid->esi.caps & GF_ESI_AAC_USE_LATM) != (bckp.caps & GF_ESI_AAC_USE_LATM))
		changed = GF_TRUE;
	else if ((tspid->esi.caps & GF_ESI_STREAM_WITHOUT_MPEG4_SYSTEMS) != (bckp.caps & GF_ESI_STREAM_WITHOUT_MPEG4_SYSTEMS))
		changed = GF_TRUE;
	else if ((tspid->esi.caps & GF_ESI_FORCE_DOLBY_VISION) != (bckp.caps & GF_ESI_FORCE_DOLBY_VISION))
		changed = GF_TRUE;

	u32 crc_old = 0, crc_new = 0;
	if (tspid->esi.gpac_meta_dsi) {
		crc_old = gf_crc_32(tspid->esi.gpac_meta_dsi, tspid->esi.gpac_meta_dsi_size);
		gf_free(tspid->esi.gpac_meta_dsi);
		tspid->esi.gpac_meta_dsi = NULL;
		tspid->esi.gpac_meta_dsi_size = 0;
	}
	p = gf_filter_pid_get_property(tspid->ipid, GF_PROP_PID_META_DEMUX_CODEC_ID);
	if (p) {
		GF_BitStream *bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		gf_bs_write_u32(bs, tspid->esi.codecid);
		gf_bs_write_u8(bs, tspid->esi.stream_type);
		gf_bs_write_u8(bs, 1); //version
		gf_bs_write_u32(bs, p->value.uint);
		p = gf_filter_pid_get_property(tspid->ipid, GF_PROP_PID_META_DEMUX_CODEC_NAME);
		gf_bs_write_utf8(bs, p ? p->value.string : NULL);
		tspid->esi.gpac_meta_name = p ? p->value.string : NULL;
		p = gf_filter_pid_get_property(tspid->ipid, GF_PROP_PID_META_DEMUX_OPAQUE);
		gf_bs_write_u32(bs, p ? p->value.uint : 0);

		//DSI before stream-specific attributes so we can further extends without breaking
		if (tspid->esi.decoder_config) {
			gf_bs_write_u32(bs, tspid->esi.decoder_config_size);
			gf_bs_write_data(bs, tspid->esi.decoder_config, tspid->esi.decoder_config_size);
		} else {
			gf_bs_write_u32(bs, 0);
		}
		
		if (tspid->esi.stream_type==GF_STREAM_VISUAL) {
			p = gf_filter_pid_get_property(tspid->ipid, GF_PROP_PID_WIDTH);
			gf_bs_write_u32(bs, p ? p->value.uint : 0);
			p = gf_filter_pid_get_property(tspid->ipid, GF_PROP_PID_HEIGHT);
			gf_bs_write_u32(bs, p ? p->value.uint : 0);
		} else if (tspid->esi.stream_type==GF_STREAM_AUDIO) {
			p = gf_filter_pid_get_property(tspid->ipid, GF_PROP_PID_SAMPLE_RATE);
			gf_bs_write_u32(bs, p ? p->value.uint : 0);
			p = gf_filter_pid_get_property(tspid->ipid, GF_PROP_PID_NUM_CHANNELS);
			gf_bs_write_u32(bs, p ? p->value.uint : 0);
		}

		gf_bs_get_content(bs, &tspid->esi.gpac_meta_dsi, &tspid->esi.gpac_meta_dsi_size);
		gf_bs_del(bs);
		crc_new = gf_crc_32(tspid->esi.gpac_meta_dsi, tspid->esi.gpac_meta_dsi_size);
	}
	if (crc_old != crc_new) {
		changed = GF_TRUE;
	}

	u32 m2ts_ra = 0;
	p = gf_filter_pid_get_property_str(tspid->ipid, "M2TSRA");
	if (p) {
		if ((p->type==GF_PROP_UINT) || (p->type==GF_PROP_4CC)) m2ts_ra = p->value.uint;
		else if (p->type==GF_PROP_STRING) m2ts_ra = gf_4cc_parse(p->value.string);
	}
	if (m2ts_ra != tspid->esi.ra_code) {
		changed = GF_TRUE;
		tspid->esi.ra_code = m2ts_ra;
	}

	return changed;
}

static void tsmux_setup_temi(GF_TSMuxCtx *ctx, M2Pid *tspid)
{
	GF_M2TS_Mux_Stream *a_stream;
	u32 st_idx=0;
	const GF_PropertyValue *p;
	char *temi_cfg;

	p = gf_filter_pid_get_property_str(tspid->ipid, "tsmux:temi");
	if (p && (p->type==GF_PROP_STRING)) temi_cfg = p->value.string;
	else temi_cfg = ctx->temi;

	if (!temi_cfg) return;
	u32 temi_id=0;

	//find our stream index
	a_stream = tspid->mstream->program->streams;
	while (a_stream) {
		if (tspid->mstream == a_stream) break;
		st_idx++;
		a_stream = a_stream->next;
	}

	while (temi_cfg) {
		u32 service_id=0;
		u32 temi_delay=1000;
		u64 temi_offset=0;
		u32 temi_timescale=0;
		Bool temi_ntp=GF_FALSE;
		u32 temi_64bits=TEMI_TC64_AUTO;
		Bool temi_use_init_val=GF_FALSE;
		u64 temi_init_val = 0;
		u32 temi_streamtype=0;
		u32 temi_index=0;
		Bool done=GF_FALSE;
		char *sep;

		if (!strlen(temi_cfg))
			break;

		while (temi_cfg[0]=='#') {
			sep = strchr(temi_cfg+1, '#');
			if (!sep) {
				temi_cfg += 1;
				break;
			}
			sep[0] = 0;
			switch (temi_cfg[1]) {
			case 'S':
				service_id = atoi(temi_cfg+2);
				break;
			case 'N':
				temi_ntp = GF_TRUE;
				break;
			case 'D':
				temi_delay = atoi(temi_cfg+2);
				break;
			case 'O':
				sscanf(temi_cfg+2, LLU, &temi_offset);
				break;
			case 'I':
				sscanf(temi_cfg+2, LLU, &temi_init_val);
				temi_use_init_val=GF_TRUE;
				break;
			case 'T':
				temi_timescale = atoi(temi_cfg+2);
				break;
			case 'L':
				if (temi_cfg[2]=='Y') temi_64bits = TEMI_TC64_ALWAYS;
				else if (temi_cfg[2]=='N') temi_64bits = TEMI_TC64_OFF;
				break;
			case 'P':
				//all pids target
				if (!temi_cfg[2] || (temi_cfg[2]=='*')) { }
				else if ((temi_cfg[2]=='v') || (temi_cfg[2]=='V')) { temi_streamtype = GF_STREAM_VISUAL; }
				else if ((temi_cfg[2]=='a') || (temi_cfg[2]=='A')) { temi_streamtype = GF_STREAM_AUDIO; }
				else if ((temi_cfg[2]=='t') || (temi_cfg[2]=='T')) { temi_streamtype = GF_STREAM_TEXT; }
				else {
					temi_index = atoi(temi_cfg+2);
				}
				break;
			default:
				done=GF_TRUE;
				break;
			}
			sep[0] = '#';
			if (done) {
				temi_cfg++;
				break;
			}
			temi_cfg = sep;
		}
		if (!temi_cfg || !strlen(temi_cfg)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[M2TSMux] Invalid temi syntax, no ID or URL specified\n"));
			return;
		}

		sep = strchr(temi_cfg, ',');
		if (sep) sep[0] = 0;	

		done = GF_TRUE;
		if (temi_index && (temi_index!=st_idx)) done = GF_FALSE;
		else if (temi_streamtype && (temi_streamtype!=tspid->esi.stream_type)) done = GF_FALSE;
		else if (service_id && (service_id != tspid->mstream->program->number)) done = GF_FALSE;

		//that's for this stream
		if (done)  {
			TEMIDesc *temi;
			if (!tspid->temi_descs) tspid->temi_descs = gf_list_new();
			GF_SAFEALLOC(temi, TEMIDesc);
			temi->id = atoi(temi_cfg);
			if (!temi->id) {
				temi->url = gf_strdup(temi_cfg);
				temi->id = temi_id+1;
			}
			temi->ntp = temi_ntp;
			temi->offset = temi_offset;
			temi->delay = temi_delay;
			temi->timescale = temi_timescale;
			temi->mode_64bits = temi_64bits;
			temi->init_val = temi_init_val;
			temi->use_init_val = temi_use_init_val;
			gf_list_add(tspid->temi_descs, temi);
		}
		if (!sep) break;
		sep[0] = ',';
		temi_cfg = sep+1;
	}
}

static void tsmux_del_stream(M2Pid *tspid)
{
	if (tspid->temi_descs) {
		while (gf_list_count(tspid->temi_descs)) {
			TEMIDesc *temi = gf_list_pop_back(tspid->temi_descs);
			if (temi->url) gf_free(temi->url);
			gf_free(temi);
		}
		gf_list_del(tspid->temi_descs);
	}
	if (tspid->af_data) gf_free(tspid->af_data);
	if (tspid->temi_af_bs) gf_bs_del(tspid->temi_af_bs);

	if (tspid->esi.sl_config) gf_free(tspid->esi.sl_config);
	if (tspid->pck_data_buf) gf_free(tspid->pck_data_buf);
	if (tspid->esi.gpac_meta_dsi) gf_free(tspid->esi.gpac_meta_dsi);
	gf_free(tspid);
}

static GF_Err tsmux_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	u32 service_id, codec_id, streamtype;
	GF_M2TS_Mux_Stream *ts_stream;
	const GF_PropertyValue *p;
	GF_PropertyEntry *pe=NULL;
	M2Pid *tspid=NULL;
	char *sname, *pname;
	GF_M2TS_Mux_Program *prog;
	GF_TSMuxCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		tspid = gf_filter_pid_get_udta(pid);
		if (!tspid) return GF_OK;
		//remove stream - this will update PMT as well
		gf_m2ts_program_stream_remove(tspid->mstream);
		//destroy or object
		gf_list_del_item(ctx->pids, tspid);
		tsmux_del_stream(tspid);
		return GF_OK;
	}

	if (!gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (!p) return GF_NOT_SUPPORTED;
	codec_id = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
	if (!p) return GF_NOT_SUPPORTED;
	streamtype = p->value.uint;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_SERVICE_ID);
	service_id = p ? p->value.uint : ctx->sid;

	sname = ctx->name;
	pname = ctx->provider;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_SERVICE_NAME);
	if (p) sname = p->value.string;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_SERVICE_PROVIDER);
	if (p) pname = p->value.string;

	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		gf_filter_pid_set_name(ctx->opid, "ts_mux");
	}
	//set output properties at init or reconfig
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, NULL);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT, NULL);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, NULL);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_FILE) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_FAKE_MP2T));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_TIMESCALE, &PROP_UINT(90000));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_NO_TS_LOOP, &PROP_BOOL(GF_TRUE));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DASH_MODE, NULL);

	mux_assign_mime_file_ext(pid, ctx->opid, M2TS_FILE_EXTS, M2TS_MIMES, "ts");

	p = gf_filter_pid_get_info(pid, GF_PROP_PID_DASH_MODE, &pe);
	if (p) {
		if (!ctx->dash_mode && p->value.uint) ctx->init_dash = GF_TRUE;
		if (!ctx->dash_mode) {
			//use large pack buffer for dash unless not default value
			if (ctx->nb_pack==4) {
				ctx->nb_pack = 200;
				ctx->pack_buffer = gf_realloc(ctx->pack_buffer, sizeof(u8)*188*ctx->nb_pack);
			}
			//in dash, force singel PES per AU, some demuxers have issues with PES packets with no ADTS headers (middle of a frame)
			gf_m2ts_mux_use_single_au_pes_mode(ctx->mux, GF_M2TS_PACK_NONE);
		}
		ctx->dash_mode = p->value.uint;
		if (ctx->dash_mode) {
			ctx->mux->flush_pes_at_rap = GF_TRUE;
			gf_m2ts_mux_set_initial_pcr(ctx->mux, 0);

			p = gf_filter_pid_get_property(pid, GF_PROP_PID_FORCE_SEG_SYNC);
			if (p && p->value.boolean)
				ctx->force_seg_sync = GF_TRUE;
		}
	}
	p = gf_filter_pid_get_info(pid, GF_PROP_PID_LLHLS, &pe);
	ctx->llhls = p ? p->value.uint : 0;

	gf_filter_release_property(pe);

	tspid = gf_filter_pid_get_udta(pid);
	if (!tspid) {
		GF_SAFEALLOC(tspid, M2Pid);
		if (!tspid) return GF_OUT_OF_MEM;
		gf_filter_pid_set_udta(pid, tspid);
		tspid->ipid = pid;
		tspid->sid = service_id;
		tspid->ctx = ctx;
		gf_list_add(ctx->pids, tspid);
		gf_filter_pid_set_framing_mode(pid, GF_TRUE);

		if (ctx->breq) {
			GF_FilterEvent evt;
			GF_FEVT_INIT(evt, GF_FEVT_BUFFER_REQ, pid);
			evt.buffer_req.max_buffer_us = 1000*ctx->breq;
			evt.buffer_req.pid_only = GF_TRUE;
			gf_filter_pid_send_event(pid, &evt);
		}
		if (ctx->is_playing) {
			GF_FilterEvent evt;
			GF_FEVT_INIT(evt, GF_FEVT_PLAY, pid);
			evt.play.start_range = ctx->start_range;
			gf_filter_pid_send_event(pid, &evt);
		}
	}

	if (ctx->llhls && (!ctx->ref_pid || (streamtype == GF_STREAM_VISUAL)) ) {
		ctx->ref_pid = tspid;
	}

	//do we need a new program
	prog = gf_m2ts_mux_program_find(ctx->mux, service_id);
	if (!prog) {
		u32 nb_progs;
		u64 first_pts_val;
		u64 pcr_offset=0;
		u32 pmt_id = ctx->pmt_id;

		if (!pmt_id) pmt_id = 100;
		nb_progs = 1+gf_m2ts_mux_program_count(ctx->mux);
		if (nb_progs>1) {
			if (ctx->dash_mode) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[M2TSMux] Muxing several programs (%d) in DASH mode is not allowed\n", nb_progs+1));
				return GF_BAD_PARAM;
			}
			pmt_id += (nb_progs - 1) * 100;
		}

		p = gf_filter_pid_get_property_str(tspid->ipid, "tsmux:pcr_offset");
		if (p && (p->type==GF_PROP_LUINT)) {
			pcr_offset = p->value.longuint;
		} else if (ctx->pcr_offset==(u64)-1) {
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_MAX_FRAME_SIZE);
			if (p && p->value.uint && ctx->rate) {
				Double r = p->value.uint * 8;
				r *= 90000;
				r/= ctx->rate;
				//add 10% of safety to cover TS signaling and other potential table update while sending the largest PES
				r *= 1.1;
				pcr_offset = (u64) r;
			} else {
				pcr_offset = -1;
			}
		} else {
			pcr_offset = ctx->pcr_offset;
		}
		p = gf_filter_pid_get_property_str(tspid->ipid, "tsmux:force_pts");
		if (p && (p->type==GF_PROP_LUINT)) first_pts_val = p->value.longuint;
		else first_pts_val = ctx->first_pts;

		prog = gf_m2ts_mux_program_add(ctx->mux, service_id, pmt_id, ctx->pmt_rate, pcr_offset, ctx->mpeg4, ctx->pmt_version, ctx->disc, first_pts_val);

		if (sname) gf_m2ts_mux_program_set_name(prog, sname, pname);

		p = gf_filter_pid_get_property(tspid->ipid, GF_PROP_PID_DASH_SPARSE);
		if (p && p->value.boolean) ctx->keepts = GF_TRUE;
		//move to NO_TS, adjust min when checking buffering
		if (ctx->keepts) {
			prog->force_first_pts = GF_FILTER_NO_TS;
		}

		p = gf_filter_pid_get_property(tspid->ipid, GF_PROP_PID_SCTE35_PID);
		if (p && !tspid->ctx->scte35_stream) {
			M2Pid *m2pid;
			GF_SAFEALLOC(m2pid, M2Pid);
			if (!m2pid) return GF_OUT_OF_MEM;

			m2pid->sid = service_id; // belongs to the same service
			m2pid->ipid = pid;       // attach to parent pid
			m2pid->ctx = ctx;
			m2pid->codec_id = GF_CODECID_SCTE35;
			m2pid->is_sparse = GF_TRUE;
			m2pid->prog = prog;

			m2pid->esi.stream_type = GF_STREAM_METADATA;
			m2pid->esi.codecid = GF_CODECID_SCTE35;
			m2pid->esi.timescale = 90000;
			m2pid->esi.caps = GF_ESI_STREAM_WITHOUT_MPEG4_SYSTEMS|GF_ESI_STREAM_SPARSE;
			//m2pid->esi.input_ctrl = tsmux_esi_ctrl;
			m2pid->esi.input_udta = m2pid;

			gf_list_add(tspid->ctx->pids, m2pid);

			int scte35_pid = gf_m2ts_mux_program_get_pmt_pid(m2pid->prog);
			scte35_pid += 1 + gf_m2ts_mux_program_get_stream_count(m2pid->prog);
			m2pid->ctx->scte35_stream = gf_m2ts_program_stream_add(m2pid->prog, &m2pid->esi, scte35_pid, GF_FALSE, GF_FALSE, GF_FALSE);

			GF_M2TSDescriptor *scte35_desc;
			GF_SAFEALLOC(scte35_desc, GF_M2TSDescriptor);
			if (!scte35_desc) return GF_OUT_OF_MEM;
			scte35_desc->tag = GF_M2TS_REGISTRATION_DESCRIPTOR;
			scte35_desc->data = gf_strdup("CUEI");
			if (!scte35_desc->data) return GF_OUT_OF_MEM;
			scte35_desc->data_len = 4;
			gf_list_add(m2pid->prog->loop_descriptors, scte35_desc);

			m2pid->ctx->scte35_stream->process = tsmux_stream_process_scte35;

			m2pid->prog->pmt->table_needs_update = GF_TRUE;
			m2pid->ctx->pmt_update_pending = GF_TRUE;
			m2pid->ctx->update_mux = GF_TRUE;
		}

		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[M2TSMux] Setting up program ID %d - send rates: PSI %d ms PCR every %d ms max - PCR offset %d\n", service_id, ctx->pmt_rate, ctx->max_pcr, ctx->pcr_offset));
	}
	if (tspid->esi.stream_type != streamtype)
		tspid->codec_id = 0;

	//first setup
	if (!tspid->codec_id) {
		Bool is_pcr=GF_FALSE;
		Bool force_pes=GF_FALSE;
		u32 pes_pid;
		gf_assert(!tspid->esi.stream_type);
		tspid->codec_id = codec_id;
		tspid->esi.stream_type = streamtype;

		if (ctx->bifs_pes && (tspid->esi.stream_type==GF_STREAM_SCENE))
			force_pes = GF_TRUE;

		pes_pid = gf_m2ts_mux_program_get_pmt_pid(prog);
		pes_pid += 1 + gf_m2ts_mux_program_get_stream_count(prog);
		if (gf_m2ts_mux_program_get_pcr_pid(prog)==0) {
			if (streamtype==GF_STREAM_VISUAL)
				is_pcr = GF_TRUE;
			else
				ctx->check_pcr = GF_TRUE;
			//if no video track we will update the PCR at the first packets
		}

		tsmux_setup_esi(ctx, prog, tspid, streamtype);
		tspid->mstream = gf_m2ts_program_stream_add(prog, &tspid->esi, pes_pid, is_pcr, force_pes, GF_FALSE);
		tsmux_setup_temi(ctx, tspid);

		if (!ctx->mux->ref_pid || (streamtype==GF_STREAM_VISUAL))
			ctx->mux->ref_pid = pes_pid;

		tspid->is_sparse = GF_FALSE;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_SPARSE);
		if (p && p->value.boolean) {
			tspid->is_sparse = GF_TRUE;
		} else {
			switch (tspid->esi.stream_type) {
			case GF_STREAM_AUDIO:
			case GF_STREAM_VISUAL:
				break;
			default:
				tspid->is_sparse = GF_TRUE;
				break;
			}
		}
	}
	//no changes in codec ID, update ifce, notify pmt update if needed and return
	else if (tspid->codec_id == codec_id) {
		if (tsmux_setup_esi(ctx, prog, tspid, streamtype)) {
			prog->pmt->table_needs_update = GF_TRUE;
			ctx->pmt_update_pending = GF_TRUE;
			ctx->update_mux = GF_TRUE;
		}
		return GF_OK;
	} else {
		tspid->codec_id = codec_id;
		tsmux_setup_esi(ctx, prog, tspid, streamtype);
		prog->pmt->table_needs_update = GF_TRUE;
		ctx->pmt_update_pending = GF_TRUE;
	}
	ctx->update_mux = GF_TRUE;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DELAY);
	if (p) {
		tspid->media_delay = p->value.longsint;

		//compute max ts skip for this program
		s64 max_media_skip = 0;
		u32 max_skip_ts = 0;
		ts_stream = prog->streams;
		while (ts_stream) {
			M2Pid *atspid = ts_stream->ifce->input_udta;
			s64 media_skip;
			if (atspid->media_delay>=0) {
				ts_stream = ts_stream->next;
				continue;
			}

			media_skip = -atspid->media_delay;
			if (!max_media_skip || gf_timestamp_greater(media_skip, atspid->esi.timescale, max_media_skip, max_skip_ts) ) {
				max_media_skip = media_skip ;
				max_skip_ts = atspid->esi.timescale;
			}
			ts_stream = ts_stream->next;
		}

		ts_stream = prog->streams;
		while (ts_stream) {
			M2Pid *atspid = ts_stream->ifce->input_udta;
			if (max_skip_ts) {
				atspid->max_media_skip = (max_media_skip * atspid->esi.timescale / max_skip_ts);
			} else {
				atspid->max_media_skip = 0;
			}
			ts_stream = ts_stream->next;
		}
	}
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CTS_SHIFT);
	if (p) {
		u64 diff = gf_timestamp_rescale(p->value.uint, tspid->esi.timescale, 1000000);
		if (diff > tspid->prog->cts_offset)
			tspid->prog->cts_offset = (u32) diff;
	}
	return GF_OK;
}

static void tsmux_assign_pcr(GF_TSMuxCtx *ctx)
{
	GF_M2TS_Mux_Program *prog = ctx->mux->programs;
	while (prog) {
		GF_M2TS_Mux_Stream *stream;
		if (prog->pcr) {
			prog = prog->next;
			continue;
		}
		stream = prog->streams;
		while (stream) {
			if (stream->ifce->stream_type==GF_STREAM_VISUAL) {
				prog->pcr = stream;
				break;
			}
			stream = stream->next;
		}
		if (!prog->pcr) prog->pcr = prog->streams;
		ctx->update_mux = GF_TRUE;
		prog = prog->next;
	}
}

static Bool tsmux_init_buffering(GF_Filter *filter, GF_TSMuxCtx *ctx)
{
	u32 not_ready=0;
	u32 mbuf = ctx->breq*1000;
	u32 i, count = gf_list_count(ctx->pids);

	for (i=0; i<count; i++) {
		u32 buf;
		Bool buf_ok;
		M2Pid *tspid = gf_list_get(ctx->pids, i);
		buf_ok = gf_filter_pid_get_buffer_occupancy(tspid->ipid, NULL, NULL, NULL, &buf);
		if (buf_ok && (buf < mbuf) && !gf_filter_pid_has_seen_eos(tspid->ipid)) {
			if (!tspid->is_sparse) not_ready++;
		}
		if (!ctx->keepts) continue;

		GF_FilterPacket *pck = gf_filter_pid_get_packet(tspid->ipid);
		if (!pck) {
			if (!tspid->is_sparse && !gf_filter_pid_has_seen_eos(tspid->ipid)) not_ready++;
			continue;
		}
		u64 min_ts = gf_filter_pck_get_cts(pck);
		if (min_ts==GF_FILTER_NO_TS) min_ts = gf_filter_pck_get_dts(pck);
		if (min_ts==GF_FILTER_NO_TS) continue;
 		min_ts += tspid->max_media_skip + tspid->media_delay;

		min_ts = gf_timestamp_rescale(min_ts, gf_filter_pid_get_timescale(tspid->ipid), 90000);
		if ((tspid->prog->force_first_pts == GF_FILTER_NO_TS) || (tspid->prog->force_first_pts>min_ts)) {
			tspid->prog->force_first_pts = min_ts;
		}
	}
	if (not_ready) {
		u32 now = gf_sys_clock();
		if (!ctx->sync_init_time) {
			ctx->sync_init_time = now;
			return GF_FALSE;
		} else if (now - ctx->sync_init_time < 5000) {
			return GF_FALSE;
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[M2TSMux] Failed to fetch one sample on each stream after %d ms, skipping initial sync\n", now - ctx->sync_init_time));
		}
	}
	ctx->sync_init_time=0;

	ctx->init_buffering = GF_FALSE;
	gf_m2ts_mux_update_config(ctx->mux, GF_TRUE);
	return GF_TRUE;
}

static void tsmux_send_seg_event(GF_Filter *filter, GF_TSMuxCtx *ctx)
{
	GF_FilterEvent evt;
	u32 i;
	M2Pid *tspid = NULL;

		for (i=0; i<gf_list_count(ctx->pids); i++) {
		tspid = gf_list_get(ctx->pids, i);
		if (ctx->mux->ref_pid == tspid->mstream->pid) break;
		tspid = NULL;
	}
	if (!tspid) tspid = gf_list_get(ctx->pids, 0);

	//in MP4Box HLS mode, sub_sidx can be 0 (single sidx per segment) but not produced (ctx->idx_file_name is empty)
	if (ctx->nb_sidx_entries && ctx->idx_file_name[0]) {
		GF_FilterPacket *idx_pck;
		u8 *output;
		Bool large_sidx = GF_FALSE;
		u32 segidx_size=0;
		u64 last_pck_dur = gf_timestamp_rescale(tspid->pck_duration, tspid->esi.timescale, 90000);

		if (ctx->sidx_entries[ctx->nb_sidx_entries-1].sap_time > 0xFFFFFFFFUL)
			large_sidx = GF_TRUE;
		if (ctx->sidx_entries[0].offset*188 > 0xFFFFFFFFUL)
			large_sidx = GF_TRUE;

		//styp box: 8(box) + 4(major) + 4(version) + 4(compat brand)
		segidx_size = 20;
		//sidx size: 12 (fullbox) + 8 + large ? 16 : 8 + 4 + nb_entries+12
		segidx_size += 24 +( large_sidx ? 16 : 8) + ctx->nb_sidx_entries*12;

		if (!ctx->idx_opid) {
			const char *ext = gf_file_ext_start(ctx->idx_file_name);
			if (!ext) ext = "idx";
			else ext++;
			ctx->idx_filter = gf_filter_connect_destination(filter, ctx->idx_file_name, NULL);
			ctx->idx_opid = gf_filter_pid_new(filter);
			gf_filter_pid_set_property(ctx->idx_opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_FILE) );
			gf_filter_pid_set_property(ctx->idx_opid, GF_PROP_PID_FILE_EXT, &PROP_STRING(ext) );
			gf_filter_pid_set_property(ctx->idx_opid, GF_PROP_PID_MIME, &PROP_STRING("*") );
			gf_filter_pid_set_name(ctx->idx_opid, "ts_idx");
			if (ctx->idx_filter) gf_filter_set_source(ctx->idx_filter, filter, NULL);
		}
		idx_pck = gf_filter_pck_new_alloc(ctx->idx_opid, segidx_size, &output);
		if (!idx_pck) return;

		if (!ctx->idx_bs) ctx->idx_bs = gf_bs_new(output, segidx_size, GF_BITSTREAM_WRITE);
		else gf_bs_reassign_buffer(ctx->idx_bs, output, segidx_size);

		//write styp box
		gf_bs_write_u32(ctx->idx_bs, 20);
		gf_bs_write_u32(ctx->idx_bs, GF_4CC('s','t','y','p') );
		gf_bs_write_u32(ctx->idx_bs, GF_4CC('s','i','s','x') );
		gf_bs_write_u32(ctx->idx_bs, 0);
		gf_bs_write_u32(ctx->idx_bs, GF_4CC('s','i','s','x') );

		//write sidx box
		gf_bs_write_u32(ctx->idx_bs, segidx_size - 20);
		gf_bs_write_u32(ctx->idx_bs, GF_4CC('s','i','d','x') );
		gf_bs_write_u8(ctx->idx_bs, large_sidx ? 1 : 0);
		gf_bs_write_int(ctx->idx_bs, 0, 24);
		//reference id
		gf_bs_write_u32(ctx->idx_bs, ctx->mux->ref_pid);
		//timescale
		gf_bs_write_u32(ctx->idx_bs, 90000);
		if (large_sidx) {
			gf_bs_write_u64(ctx->idx_bs, ctx->sidx_entries[0].min_pts_plus_one-1);
			gf_bs_write_u64(ctx->idx_bs, ctx->sidx_entries[0].offset*188);
		} else {
			gf_bs_write_u32(ctx->idx_bs, (u32) ctx->sidx_entries[0].min_pts_plus_one-1);
			gf_bs_write_u32(ctx->idx_bs, (u32) ctx->sidx_entries[0].offset*188);
		}
		gf_bs_write_u16(ctx->idx_bs, 0);
		gf_bs_write_u16(ctx->idx_bs, ctx->nb_sidx_entries);
		for (i=0; i<ctx->nb_sidx_entries; i++) {
			u64 duration = ctx->sidx_entries[i].max_pts - (ctx->sidx_entries[i].min_pts_plus_one-1);
			if (i+1 == ctx->nb_sidx_entries) duration += last_pck_dur;

			gf_bs_write_int(ctx->idx_bs, 0, 1);
			gf_bs_write_int(ctx->idx_bs, ctx->sidx_entries[i].nb_pck * 188, 31);
			gf_bs_write_int(ctx->idx_bs, (u32) duration, 32);
			gf_bs_write_int(ctx->idx_bs, ctx->sidx_entries[i].sap_type ? 1 : 0, 1);
			gf_bs_write_int(ctx->idx_bs, ctx->sidx_entries[i].sap_type, 3);
			gf_bs_write_int(ctx->idx_bs, (u32) (ctx->sidx_entries[i].sap_time - (ctx->sidx_entries[i].min_pts_plus_one-1) ), 28);
		}
		gf_filter_pck_set_property(idx_pck, GF_PROP_PCK_FILENAME, &PROP_STRING(ctx->idx_file_name) );

		gf_filter_pck_send(idx_pck);
		ctx->nb_sidx_entries = 0;
	}


	GF_FEVT_INIT(evt, GF_FEVT_SEGMENT_SIZE, tspid->ipid);
	evt.seg_size.media_range_start = 188*ctx->pck_start_idx;
	evt.seg_size.media_range_end = evt.seg_size.media_range_start + 188*ctx->nb_pck_in_seg - 1;

	gf_filter_pid_send_event(tspid->ipid, &evt);
	ctx->nb_pck_in_seg = 0;
	ctx->nb_sidx_entries = 0;
}

static void tsmux_insert_sidx(GF_TSMuxCtx *ctx, Bool final_flush)
{
	TS_SIDX *tsidx = NULL;
	if (ctx->subs_sidx<0) return;

	if (!ctx->mux->ref_pid) return;

	if (ctx->nb_sidx_entries) {
		tsidx = &ctx->sidx_entries[ctx->nb_sidx_entries-1];

		if (!tsidx->min_pts_plus_one) tsidx->min_pts_plus_one = ctx->mux->last_pts + 1;
		else if (tsidx->min_pts_plus_one-1 > ctx->mux->last_pts) tsidx->min_pts_plus_one = ctx->mux->last_pts + 1;

		if (tsidx->max_pts < ctx->mux->last_pts) tsidx->max_pts = ctx->mux->last_pts;

		if (!final_flush && !ctx->mux->sap_inserted) return;

		tsidx->nb_pck = ctx->nb_pck_in_seg - tsidx->nb_pck;
		if (tsidx->nb_pck)
			tsidx = NULL;
	}

	if (final_flush) return;
	if (!ctx->mux->sap_inserted) return;

	if (!tsidx) {
		if (ctx->nb_sidx_entries == ctx->nb_sidx_alloc) {
			ctx->nb_sidx_alloc += 10;
			ctx->sidx_entries = gf_realloc(ctx->sidx_entries, sizeof(TS_SIDX)*ctx->nb_sidx_alloc);
		}
		tsidx = &ctx->sidx_entries[ctx->nb_sidx_entries];
		ctx->nb_sidx_entries ++;
	}
	tsidx->sap_time = ctx->mux->sap_time;
	tsidx->sap_type = ctx->mux->sap_type;
	tsidx->min_pts_plus_one  = ctx->mux->sap_time + 1;
	tsidx->max_pts = ctx->mux->sap_time;
	tsidx->nb_pck = (ctx->nb_sidx_entries>1) ? ctx->nb_pck_in_seg : 0;
	tsidx->offset = (ctx->nb_sidx_entries>1) ? 0 : ctx->nb_pck_first_sidx;
}

static void tsmux_flush_frag_hls(GF_TSMuxCtx *ctx, Bool is_last)
{
	GF_FilterEvent evt;

	GF_FEVT_INIT(evt, GF_FEVT_FRAGMENT_SIZE, ctx->ref_pid->ipid);
	evt.frag_size.is_last = is_last;
	evt.frag_size.offset = ctx->frag_offset;
	evt.frag_size.size = ctx->frag_size;
	evt.frag_size.duration.num = (s64) ctx->frag_duration;
	evt.frag_size.duration.den = ctx->ref_pid->esi.timescale;
	evt.frag_size.independent = ctx->frag_has_intra;

	gf_filter_pid_send_event(ctx->ref_pid->ipid, &evt);

	ctx->frag_offset += ctx->frag_size;
	ctx->frag_size = 0;
	ctx->frag_duration = 0;
	ctx->frag_has_intra = GF_FALSE;
}

static void ts_mux_on_packet_del(GF_Filter *filter, GF_FilterPid *pid, GF_FilterPacket *pck)
{
	GF_TSMuxCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->force_seg_sync) {
		gf_filter_lock(filter, GF_TRUE);
		if (ctx->pending_packets)
			ctx->pending_packets--;
		if (!ctx->pending_packets)
			gf_filter_post_process_task(filter);
		gf_filter_lock(filter, GF_FALSE);
	}
}

static GF_Err tsmux_process(GF_Filter *filter)
{
	u32 nb_pck_in_pack, nb_pck_in_call;
	GF_M2TSMuxState status;
	u32 usec_till_next;
	GF_FilterPacket *pck;
	GF_TSMuxCtx *ctx = gf_filter_get_udta(filter);

	if (ctx->check_pcr) {
		ctx->check_pcr = GF_FALSE;
		tsmux_assign_pcr(ctx);
	}

	if (ctx->init_buffering && !tsmux_init_buffering(filter, ctx)) {
		gf_filter_ask_rt_reschedule(filter, 1000);
		return GF_OK;
	}

	if (ctx->init_dash) {
		u32 i, count = gf_list_count(ctx->pids);
		for (i=0; i<count; i++) {
			const GF_PropertyValue *p;
			M2Pid *tspid = gf_list_get(ctx->pids, i);
			pck = gf_filter_pid_get_packet(tspid->ipid);
			if (!pck) return GF_OK;
			p = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENUM);
			if (p) {
				tspid->ctx->dash_seg_num = p->value.uint;
				tsmux_check_mpd_start_time(tspid->ctx, pck);
			}
			p = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENAME);
			if (p)
				strcpy(tspid->ctx->dash_file_name, p->value.string);
			p = gf_filter_pck_get_property(pck, GF_PROP_PCK_IDXFILENAME);
			if (p)
				strcpy(tspid->ctx->idx_file_name, p->value.string);
		}
		ctx->init_dash = GF_FALSE;
		ctx->next_is_start = GF_TRUE;
		ctx->wait_dash_flush = GF_FALSE;
	}

	if (ctx->update_mux) {
		gf_m2ts_mux_update_config(ctx->mux, GF_FALSE);
	}


	if (ctx->wait_dash_flush || ctx->wait_llhls_flush) {
		//we are waiting for all packets to be flushed
		if (ctx->pending_packets)
			return GF_OK;

		Bool is_llhls_flush = GF_FALSE;
		Bool is_eods_flush = GF_FALSE;
		u32 i, done=0, count = gf_list_count(ctx->pids);
		for (i=0; i<count; i++) {
			M2Pid *tspid = gf_list_get(ctx->pids, i);
			if (tspid->has_seen_eods) done++;
		}

		if (done==count) {
			for (i=0; i<count; i++) {
				M2Pid *tspid = gf_list_get(ctx->pids, i);
				if (tspid->has_seen_eods==M2TS_EODS_FORCED)
					is_eods_flush = GF_TRUE;
				else if (tspid->has_seen_eods==M2TS_EODS_LLHLS) {
					is_llhls_flush = GF_TRUE;
				}
				tspid->has_seen_eods = 0;
				tspid->llhls_dts_init = 0;
			}
			ctx->wait_dash_flush = GF_FALSE;
			ctx->mux->force_pat = GF_TRUE;

			if (is_llhls_flush) {
				ctx->wait_llhls_flush = GF_FALSE;
				ctx->next_is_llhls_start = GF_TRUE;

				tsmux_flush_frag_hls(ctx, GF_FALSE);
			} else {
				ctx->next_is_start = ctx->dash_file_switch;
				ctx->dash_file_switch = GF_FALSE;

				if (ctx->llhls) {
					tsmux_flush_frag_hls(ctx, GF_TRUE);
					ctx->frag_offset = 0;
				}

				if (ctx->nb_pck_in_seg) {
					tsmux_insert_sidx(ctx, GF_TRUE);
					tsmux_send_seg_event(filter, ctx);
				}

				if (!is_eods_flush)
					ctx->dash_seg_num = 0;
				else
					ctx->last_is_eods_flush = GF_TRUE;

				ctx->nb_pck_in_seg = 0;
				ctx->pck_start_idx = ctx->nb_pck;
				if (ctx->next_is_start) ctx->nb_pck_in_file = 0;
				ctx->nb_pck_first_sidx = ctx->nb_pck_in_file;
			}
		}
	}

	nb_pck_in_call = 0;
	nb_pck_in_pack=0;
	while (1) {
		u64 pck_ts;
		u8 *output;
		u32 osize;
		Bool is_pack_flush = GF_FALSE;
		const char *ts_pck;

		ts_pck = gf_m2ts_mux_process(ctx->mux, &status, &usec_till_next);
		if (ts_pck == NULL) {
			if (!nb_pck_in_pack)
				break;
			ts_pck = (const char *) ctx->pack_buffer;
			is_pack_flush = GF_TRUE;
		} else {

			tsmux_insert_sidx(ctx, GF_FALSE);

			if (ctx->nb_pack>1) {
				memcpy(ctx->pack_buffer + 188 * nb_pck_in_pack, ts_pck, 188);
				nb_pck_in_pack++;

				if (nb_pck_in_pack < ctx->nb_pack)
					continue;

				ts_pck = (const char *) ctx->pack_buffer;
			} else {
				nb_pck_in_pack = 1;
			}
		}
		osize = nb_pck_in_pack * 188;
		if (ctx->force_seg_sync) {
			pck = gf_filter_pck_new_alloc_destructor(ctx->opid, osize, &output, ts_mux_on_packet_del);
			if (pck) ctx->pending_packets++;
		} else {
			pck = gf_filter_pck_new_alloc(ctx->opid, osize, &output);
		}
		if (!pck) return GF_OUT_OF_MEM;
		
		memcpy(output, ts_pck, osize);
		gf_filter_pck_set_framing(pck, ctx->nb_pck ? ctx->next_is_start : GF_TRUE, (status==GF_M2TS_STATE_EOS) ? GF_TRUE : GF_FALSE);

		if (ctx->next_is_start && ctx->dash_mode) {
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[M2TSMux] starting TS segment %d\r", ctx->dash_seg_num));
			gf_filter_pck_set_property(pck, GF_PROP_PCK_FILENUM, &PROP_UINT(ctx->dash_seg_num) );
			if (ctx->dash_file_name[0])
				gf_filter_pck_set_property(pck, GF_PROP_PCK_FILENAME, &PROP_STRING(ctx->dash_file_name) ) ;
			if (ctx->dash_seg_start.den)
				gf_filter_pck_set_property(pck, GF_PROP_PCK_MPD_SEGSTART, &PROP_FRAC64(ctx->dash_seg_start) ) ;

			ctx->dash_file_name[0] = 0;
			ctx->next_is_start = GF_FALSE;
			if (ctx->llhls>1) {
				ctx->frag_num=1;
				gf_filter_pck_set_property(pck, GF_PROP_PCK_HLS_FRAG_NUM, &PROP_UINT(ctx->frag_num));
			}
		}
		else if (ctx->next_is_llhls_start) {
			if (ctx->llhls>1) {
				ctx->frag_num++;
				gf_filter_pck_set_property(pck, GF_PROP_PCK_HLS_FRAG_NUM, &PROP_UINT(ctx->frag_num));
			}
			ctx->next_is_llhls_start = GF_FALSE;
		}

		pck_ts = gf_m2ts_get_ts_clock_90k(ctx->mux);
		gf_filter_pck_set_dts(pck, pck_ts);
		gf_filter_pck_set_cts(pck, pck_ts);

		if (ctx->notify_filename) {
			gf_filter_pck_set_framing(pck, GF_TRUE, (status==GF_M2TS_STATE_EOS) ? GF_TRUE : GF_FALSE);
			gf_filter_pck_set_property(pck, GF_PROP_PCK_FILENUM, &PROP_UINT(ctx->cur_file_idx_plus_one-1));
			if (ctx->cur_file_suffix) {
				gf_filter_pck_set_property(pck, GF_PROP_PCK_FILESUF, &PROP_STRING_NO_COPY(ctx->cur_file_suffix));
				ctx->cur_file_suffix = NULL;
			}
			if (ctx->dash_seg_start.den)
				gf_filter_pck_set_property(pck, GF_PROP_PCK_MPD_SEGSTART, &PROP_FRAC64(ctx->dash_seg_start) ) ;
			ctx->notify_filename = GF_FALSE;
		}
		gf_filter_pck_send(pck);
		ctx->nb_pck += nb_pck_in_pack;
		ctx->nb_pck_in_seg += nb_pck_in_pack;
		ctx->nb_pck_in_file += nb_pck_in_pack;
		nb_pck_in_call += nb_pck_in_pack;
		nb_pck_in_pack = 0;
		if (ctx->llhls)
			ctx->frag_size += osize;

		if (is_pack_flush)
			break;

		if (status>=GF_M2TS_STATE_PADDING) {
			break;
		}
		if (nb_pck_in_call>100)
			break;
	}

	if (ctx->wait_dash_flush || ctx->wait_llhls_flush) {
		u32 i, done=0, count = gf_list_count(ctx->pids);
		for (i=0; i<count; i++) {
			M2Pid *tspid = gf_list_get(ctx->pids, i);
			if (!tspid->done && tspid->has_seen_eods) done++;
		}
		if (done==count) {
			gf_filter_pid_send_flush(ctx->opid);
		}
	}

	if (gf_filter_reporting_enabled(filter)) {
		char szStatus[1024];
		if (status==GF_M2TS_STATE_EOS) {
			Double ohead = 0;
			u64 total_bytes_out = ctx->nb_pck;
			total_bytes_out *= 188;

			if (ctx->total_bytes_in) ohead =  ((Double) (total_bytes_out - ctx->total_bytes_in)*100 / ctx->total_bytes_in);

			sprintf(szStatus, "done - TS clock % 6d ms bitrate %d kbps - bytes "LLD" in "LLD" out overhead %02.02f%%", gf_m2ts_get_ts_clock(ctx->mux), ctx->mux->bit_rate/1000, ctx->total_bytes_in, total_bytes_out, ohead);
			gf_filter_update_status(filter, 10000, szStatus);
		} else {

			sprintf(szStatus, "sysclock % 6d ms TS clock % 6d ms bitrate % 8d kbps", gf_m2ts_get_sys_clock(ctx->mux), gf_m2ts_get_ts_clock(ctx->mux), ctx->mux->bit_rate/1000);
			gf_filter_update_status(filter, -1, szStatus);
		}
	}

	if (ctx->nb_suspended && (ctx->nb_suspended==gf_list_count(ctx->pids)) ) {
		u32 i, count = gf_list_count(ctx->pids);
		for (i=0; i<count; i++) {
			M2Pid *tspid = gf_list_get(ctx->pids, i);
			tspid->esi.caps &= ~GF_ESI_STREAM_IS_OVER;
			tspid->suspended = GF_FALSE;
		}
		ctx->nb_suspended = 0;
		ctx->mux->force_pat = GF_TRUE;
		ctx->notify_filename = GF_TRUE;
		status = GF_M2TS_STATE_IDLE;
	}

	if (status==GF_M2TS_STATE_EOS) {
		gf_filter_pid_set_eos(ctx->opid);
		if (ctx->nb_pck_in_seg) {
			tsmux_insert_sidx(ctx, GF_TRUE);
			tsmux_send_seg_event(filter, ctx);
			ctx->nb_pck_in_seg = 0;
		}
		return GF_EOS;
	}

	if (ctx->realtime) {
		u32 now = gf_sys_clock();
		if (!ctx->last_log_time)
			ctx->last_log_time = now;
		else if (now > ctx->last_log_time + ctx->log_freq) {
			ctx->last_log_time = now;
			GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[M2TSMux] time % 6d TS time % 6d bitrate % 8d\r", gf_m2ts_get_sys_clock(ctx->mux), gf_m2ts_get_ts_clock(ctx->mux), ctx->mux->average_birate_kbps));
		}
		if (status == GF_M2TS_STATE_IDLE) {
#if 0
			u64 sleep_for=0;
			/*wait till next packet is ready to be sent*/
			if (usec_till_next>1000) {
				sleep_for = usec_till_next;
			}
			if (sleep_for)
				gf_filter_ask_rt_reschedule(filter, (u32) sleep_for);
#else
			//we don't have enough precision on usec counting and we end up eating one core on most machines,
			// so let's just sleep one ms whenever we are idle - it's maybe too much but the muxer will catchup afterwards
			gf_filter_ask_rt_reschedule(filter, 1000);
#endif
		}
	} else if (!nb_pck_in_call) {
		//not in end of stream and we did not emit packets, force reschedule to signal we're still active
		gf_filter_ask_rt_reschedule(filter, 0);
	}
	//PMT update management is still under progress...
	ctx->pmt_update_pending = GF_FALSE;

	return GF_OK;
}

static GF_Err tsmux_initialize(GF_Filter *filter)
{
	GF_TSMuxCtx *ctx = gf_filter_get_udta(filter);
	gf_filter_set_max_extra_input_pids(filter, -1);

	ctx->mux = gf_m2ts_mux_new(ctx->rate, ctx->pat_rate, ctx->realtime);
	ctx->mux->flush_pes_at_rap = ctx->flush_rap;

	if (gf_sys_is_test_mode() && ctx->pcr_init<0)
		ctx->pcr_init = 1000000;

	gf_m2ts_mux_use_single_au_pes_mode(ctx->mux, ctx->pes_pack);
	if (ctx->pcr_init>=0) gf_m2ts_mux_set_initial_pcr(ctx->mux, (u64) ctx->pcr_init);
	gf_m2ts_mux_set_pcr_max_interval(ctx->mux, ctx->max_pcr);
	gf_m2ts_mux_enable_pcr_only_packets(ctx->mux, ctx->pcr_only);

	if (!ctx->sid) ctx->sid = 1;
	if (ctx->sdt_rate) {
		gf_m2ts_mux_enable_sdt(ctx->mux, ctx->sdt_rate);
	}

	if (!gf_filter_block_enabled(filter)) {
		ctx->breq = 0;
	} else {
		ctx->init_buffering = GF_TRUE;
	}
	ctx->pids = gf_list_new();
	if (ctx->nb_pack>1) ctx->pack_buffer = gf_malloc(sizeof(u8)*188*ctx->nb_pack);

#ifdef GPAC_ENABLE_COVERAGE
	if (gf_sys_is_cov_mode()) {
		gf_m2ts_get_sys_clock(ctx->mux);
	}
#endif
	return GF_OK;
}


static void tsmux_finalize(GF_Filter *filter)
{
	GF_TSMuxCtx *ctx = gf_filter_get_udta(filter);
#ifndef GPAC_DISABLE_LOG
	u64 bits = ctx->mux->tot_pck_sent*8*188;
#endif
	u64 dur_ms = gf_m2ts_get_ts_clock(ctx->mux);
	if (!dur_ms) dur_ms = 1;
	GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[M2TSMux] Done muxing - %.02f sec - %sbitrate %d kbps "LLD" packets written\nPadding: "LLD" packets (%g kbps) - "LLD" PES padded bytes (%g kbps)\n",
		((Double) dur_ms)/1000.0, ctx->rate ? "" : "average ", (u32) (bits/dur_ms), ctx->mux->tot_pck_sent,
		 ctx->mux->tot_pad_sent, (Double) (ctx->mux->tot_pad_sent*188*8.0/dur_ms) , ctx->mux->tot_pes_pad_bytes, (Double) (ctx->mux->tot_pes_pad_bytes*8.0/dur_ms)
	));

	while (gf_list_count(ctx->pids)) {
		M2Pid *tspid = gf_list_pop_back(ctx->pids);
		tsmux_del_stream(tspid);
	}
	gf_list_del(ctx->pids);
	gf_m2ts_mux_del(ctx->mux);
	if (ctx->pack_buffer) gf_free(ctx->pack_buffer);
	if (ctx->sidx_entries) gf_free(ctx->sidx_entries);
	if (ctx->idx_bs) gf_bs_del(ctx->idx_bs);
	if (ctx->cur_file_suffix) gf_free(ctx->cur_file_suffix);
}

static Bool tsmux_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	if ((evt->base.type==GF_FEVT_STOP) || (evt->base.type==GF_FEVT_PLAY) ) {
		u32 i;
		GF_TSMuxCtx *ctx = gf_filter_get_udta(filter);
		for (i=0; i<gf_list_count(ctx->pids); i++) {
			M2Pid *tspid = gf_list_get(ctx->pids, i);
			if (evt->base.type==GF_FEVT_STOP)
				tspid->esi.caps |= GF_ESI_STREAM_IS_OVER;
			else
				tspid->esi.caps &= ~GF_ESI_STREAM_IS_OVER;
		}
		if (evt->base.type==GF_FEVT_PLAY) {
			ctx->is_playing = GF_TRUE;
			ctx->start_range = evt->play.start_range;
		} else {
			ctx->is_playing = GF_FALSE;
		}
	}
	return GF_FALSE;
}

static const GF_FilterCapability TSMuxCaps[] =
{
	//first set of caps describe streams that need reframing (NALU, mp4v, mpegh audio)
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	//unframed streams only
	CAP_BOOL(GF_CAPS_INPUT, GF_PROP_PID_UNFRAMED, GF_TRUE),
	//for NAL-based, we want annexB format
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_AVC),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_MVC),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_SVC),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_HEVC),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_LHVC),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_VVC),
	// For AV1, we need to split TU into 1 frame header per PES
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_AV1),
	//for m4vp2 we want DSI reinsertion
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_MPEG4_PART2),
	//for vc1 we want DSI reinsertion
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_SMPTE_VC1),
	//for AAC we use the AAC->ADTS or AAC->LATM of the mux, so don't insert here

	//for MPEG-H audio MHAS we need to insert sync packets
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_MHAS),

	//static output cap file extension
	CAP_UINT(GF_CAPS_OUTPUT_STATIC,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_FILE_EXT, M2TS_FILE_EXTS),
	CAP_STRING(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_MIME, M2TS_MIMES),
	{0},

	//for now don't accept files as input, although we could store them as items, to refine
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	//these caps are framed
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	//exclude caps from above
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_AVC),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_MVC),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_SVC),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_HEVC),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_LHVC),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_VVC),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_MPEG4_PART2),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_MHAS),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_AV1),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_SMPTE_VC1),
	//we don't accept MPEG-H audio without MHAS
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_MPHA),
	//no RAW support for now$
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	{0},

	//for SAES, accepte encrypted AVC with scheme SAES, unframed
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_ENCRYPTED),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_PROTECTION_SCHEME_TYPE, GF_4CC('s','a','e','s') ),
	CAP_BOOL(GF_CAPS_INPUT, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_AVC),
};


#define OFFS(_n)	#_n, offsetof(GF_TSMuxCtx, _n)
static const GF_FilterArgs TSMuxArgs[] =
{
	{ OFFS(breq), "buffer requirements in ms for input PIDs", GF_PROP_UINT, "100", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(pmt_id), "define the ID of the first PMT to use in the mux", GF_PROP_UINT, "100", NULL, 0},
	{ OFFS(rate), "target rate in bps of the multiplex. If not set, variable rate is used", GF_PROP_UINT, "0", NULL, 0},
	{ OFFS(pmt_rate), "interval between PMT in ms", GF_PROP_UINT, "200", NULL, 0},
	{ OFFS(pat_rate), "interval between PAT in ms", GF_PROP_UINT, "200", NULL, 0},
	{ OFFS(first_pts), "force PTS value of first packet, in 90kHz", GF_PROP_LUINT, "0", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(pcr_offset), "offset all timestamps from PCR by V, in 90kHz (default value is computed based on input media)", GF_PROP_LUINT, "-1", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(mpeg4), "force usage of MPEG-4 signaling (IOD and SL Config)\n"
				"- none: disables 4on2\n"
				"- full: sends AUs as SL packets over section for OD, section/pes for scene (cf bifs_pes)\n"
				"- scene: sends only scene streams as 4on2 but uses regular PES without SL for audio and video"
				, GF_PROP_UINT, "none", "none|full|scene", GF_FS_ARG_HINT_EXPERT},
	{ OFFS(pmt_version), "set version number of the PMT", GF_PROP_UINT, "200", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(disc), "set the discontinuity marker for the first packet of each stream", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(repeat_rate), "interval in ms between two carousel send for MPEG-4 systems (overridden by `CarouselRate` PID property if defined)", GF_PROP_UINT, "0", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(repeat_img), "interval in ms between re-sending (as PES) of single-image streams (if 0, image data is sent once only)", GF_PROP_UINT, "0", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(max_pcr), "set max interval in ms between 2 PCR", GF_PROP_UINT, "100", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(nb_pack), "pack N TS packets in output packets", GF_PROP_UINT, "4", NULL, 0},
	{ OFFS(pes_pack), "set AU to PES packing mode\n"
		"- audio: will pack only multiple audio AUs in a PES\n"
		"- none: make exactly one AU per PES\n"
		"- all: will pack multiple AUs per PES for all streams", GF_PROP_UINT, "audio", "audio|none|all", GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(realtime), "use real-time output", GF_PROP_BOOL, "false", NULL, 0},
	{ OFFS(bifs_pes), "select BIFS streams packetization (PES vs sections)\n"
	"- on: uses BIFS PES\n"
	"- off: uses BIFS sections\n"
	"- copy: uses BIFS PES but removes timestamps in BIFS SL and only carries PES timestamps", GF_PROP_UINT, "off", "off|on|copy", GF_FS_ARG_HINT_EXPERT},
	{ OFFS(flush_rap), "force flushing mux program when RAP is found on video, and injects PAT and PMT before the next video PES begin", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(pcr_only), "enable PCR-only TS packets", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(pcr_init), "set initial PCR value for the programs. A negative value means random value is picked", GF_PROP_LSINT, "-1", NULL, 0},
	{ OFFS(sid), "set service ID for the program", GF_PROP_UINT, "0", NULL, 0},
	{ OFFS(name), "set service name for the program", GF_PROP_STRING, NULL, NULL, 0},
	{ OFFS(provider), "set service provider name for the program", GF_PROP_STRING, NULL, NULL, 0},
	{ OFFS(sdt_rate), "interval in ms between two DVB SDT tables (if 0, SDT is disabled)", GF_PROP_UINT, "0", NULL, 0},
	{ OFFS(temi), "insert TEMI time codes in adaptation field", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(log_freq), "delay between logs for realtime mux", GF_PROP_UINT, "500", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(latm), "use LATM AAC encapsulation instead of regular ADTS", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(subs_sidx), "number of subsegments per sidx (negative value disables sidx)", GF_PROP_SINT, "-1", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(keepts), "keep cts/dts untouched and adjust PCR accordingly, used to keep TS unmodified when dashing", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(cdur), "chunk duration for fragmentation modes", GF_PROP_FRACTION, "-1/1", NULL, GF_FS_ARG_HINT_HIDE},

	{0}
};


GF_FilterRegister TSMuxRegister = {
	.name = "m2tsmx",
	GF_FS_SET_DESCRIPTION("MPEG-2 TS multiplexer")
	GF_FS_SET_HELP("This filter multiplexes one or more input PIDs into a MPEG-2 Transport Stream multiplex.\n"
		"\n"
		"# PID selection\n"
		"The MPEG-2 TS multiplexer assigns M2TS PID for media streams using the PID of the PMT plus the stream index.\n"
	 	"For example, the default config creates the first program with a PMT PID 100, the first stream will have a PID of 101.\n"
		"Streams are grouped in programs based on input PID property ServiceID if present. If absent, stream will go in the program with service ID as indicated by [-sid]() option.\n"
		"- [-name]() option is overridden by input PID property `ServiceName`.\n"
		"- [-provider]() option is overridden by input PID property `ServiceProvider`.\n"
		"- [-pcr_offset]() option is overridden by input PID property `\"tsmux:pcr_offset\"`\n"
		"- [-first_pts]() option is overridden by input PID property `\"tsmux:force_pts\"`\n"
		"- [-temi]() option is overridden by input PID property `\"tsmux:temi\"`\n"
		"\n"
		"# Time and External Media Information (TEMI)\n"
		"The [-temi]() option allows specifying a list of URLs or timeline IDs to insert in streams of a program.\n"
		"One or more TEMI timeline can be specified per PID.\n"
		"The syntax is a comma-separated list of one or more TEMI description.\n"
		"Each TEMI description is formatted as ID_OR_URL or #OPT1[#OPT2]#ID_OR_URL. Options are:\n"
		"- S`N`: indicate the target service with ID `N`\n"
		"- T`N`: set timescale to use (default: PID timescale)\n"
		"- D`N`: set delay in ms between two TEMI url descriptors (default 1000)\n"
		"- O`N`: set offset (max 64 bits) to add to TEMI timecodes (default 0). If timescale is not specified, offset value is in ms, otherwise in timescale units.\n"
		"- I`N`: set initial value (max 64 bits) of TEMI timecodes. If not set, initial value will match first packet CTS. If timescale is not specified, value is in PID timescale units, otherwise in specified timescale units.\n"
		"- P`N`: indicate target PID in program. Possible values are\n"
		"  - `V`: only insert for video streams.\n"
		"  - `A`: only insert for audio streams.\n"
		"  - `T`: only insert for text streams.\n"
		"  - N: only insert for stream with index `N` (0-based) in the program.\n"
		"- L`C`: set 64bit timecode signaling. Possible values for `C` are:\n"
		"  - `A`: automatic switch between 32 and 64 bit depending on timecode value (default if not specified).\n"
		"  - `Y`: use 64 bit signaling only.\n"
		"  - `N`: use 32 bit signaling only and wrap around timecode value.\n"
		"- N: insert NTP timestamp in TEMI timeline descriptor\n"
		"- ID_OR_URL: If number, indicate the TEMI ID to use for external timeline. Otherwise, give the URL to insert\n"
		"  \n"
		"EX temi=\"url\"\n"
		"Inserts a TEMI URL+timecode in the each stream of each program.\n"
		"EX temi=\"#P0#url,#P1#4\"\n"
		"Inserts a TEMI URL+timecode in the first stream of all programs and an external TEMI with ID 4 in the second stream of all programs.\n"
		"EX temi=\"#P0#2,#P0#url,#P1#4\"\n"
		"Inserts a TEMI with ID 2 and a TEMI URL+timecode in the first stream of all programs, and an external TEMI with ID 4 in the second stream of all programs.\n"
		"EX temi=\"#S20#4,#S10#URL\"\n"
		"Inserts an external TEMI with ID 4 in the each stream of program with ServiceID 20 and a TEMI URL in each stream of program with ServiceID 10.\n"
		"EX temi=\"#N#D500#PV#T30000#4\"\n"
		"Inserts an external TEMI with ID 4 and timescale 30000, NTP injection and carousel of 500 ms in the video stream of all programs.\n"
		"\n"
		"Warning: multipliers (k,m,g) are not supported in TEMI options.\n"
		"# Adaptive Streaming\n"
		"In DASH and HLS mode:\n"
		"- the PCR is always initialized at 0, and [-flush_rap]() is automatically set.\n"
		"- unless `nb_pack` is specified, 200 TS packets will be used as pack output in DASH mode.\n"
		"- `pes_pack=none` is forced since some demultiplexers have issues with non-aligned ADTS PES.\n"
		"\n"
		"The filter watches the property `FileNumber` on incoming packets to create new files, or new segments in DASH mode.\n"
		"The filter will look for property `M2TSRA` set on the input stream.\n"
		"The value can either be a 4CC or a string, indicating the MP2G-2 TS Registration tag for unknown media types.\n"
		"\n"
		"# Notes\n"
		"In LATM mux mode, the decoder configuration is inserted at the given [-repeat_rate]() or `CarouselRate` PID property if defined.\n"
	)
	.private_size = sizeof(GF_TSMuxCtx),
	.args = TSMuxArgs,
	.initialize = tsmux_initialize,
	.finalize = tsmux_finalize,
	.flags = GF_FS_REG_DYNAMIC_REDIRECT,
	SETCAPS(TSMuxCaps),
	.configure_pid = tsmux_configure_pid,
	.process = tsmux_process,
	.process_event = tsmux_process_event,
};


const GF_FilterRegister *m2tsmx_register(GF_FilterSession *session)
{
	return &TSMuxRegister;
}

#else

const GF_FilterRegister *m2tsmx_register(GF_FilterSession *session)
{
	return NULL;
}

#endif /*GPAC_DISABLE_MPEG2TS_MUX*/
